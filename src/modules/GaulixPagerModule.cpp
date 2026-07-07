#include "configuration.h"

#if defined(GAULIX_PAGER) && HAS_SCREEN

#include "GaulixPagerModule.h"
#include "Channels.h"
#include "FSCommon.h"
#include "MeshService.h"
#include "MeshTypes.h"
#include "NodeDB.h"
#include "SafeFile.h"
#include "buzz.h"
#include "gps/RTC.h"
#include "graphics/Screen.h"
#include "graphics/ScreenFonts.h"
#include "main.h"
#include "mesh/Throttle.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>

static const char *GAULIX_PAGER_CONFIG_FILE = "/prefs/gaulixpager.cfg";

GaulixPagerModule *gaulixPagerModule = nullptr;

uint32_t GaulixPagerModule::alertCount = 0;
uint32_t GaulixPagerModule::lastAlertTime = 0;
bool GaulixPagerModule::alertActive = false;
char GaulixPagerModule::alertText[96] = {};
NodeNum GaulixPagerModule::alertSourceNode = 0;
uint8_t GaulixPagerModule::alertSourceChannel = 0;
char GaulixPagerModule::activationCode[32] = GAULIX_DEFAULT_ACTIVATION_CODE;
uint8_t GaulixPagerModule::configuredBeepCount = GaulixPagerModule::DEFAULT_BEEP_COUNT;
uint32_t GaulixPagerModule::alertStartedMs = 0;
uint32_t GaulixPagerModule::lastContinuousBeepMs = 0;
bool GaulixPagerModule::ledBlinkState = false;

GaulixPagerModule::GaulixPagerModule()
    : SinglePortModule("gaulixpager", meshtastic_PortNum_TEXT_MESSAGE_APP), concurrency::OSThread("GaulixPager")
{
    isPromiscuous = true;
    requestFocus();
    // Force pager alerting defaults on each boot so stale persisted settings
    // don't make two flashed devices behave differently.
    config.device.buzzer_mode = meshtastic_Config_DeviceConfig_BuzzerMode_ALL_ENABLED;
    moduleConfig.has_external_notification = true;
    moduleConfig.external_notification.enabled = true;
    moduleConfig.external_notification.alert_message_buzzer = true;
#if defined(PIN_BUZZER)
    moduleConfig.external_notification.output_buzzer = PIN_BUZZER;
    moduleConfig.external_notification.use_pwm = true;
#endif
    loadConfig();
#if !defined(MESHTASTIC_EXCLUDE_INPUTBROKER)
    if (inputBroker) {
        inputObserver.observe(inputBroker);
    }
#endif
}

void GaulixPagerModule::loadConfig()
{
    strncpy(activationCode, GAULIX_DEFAULT_ACTIVATION_CODE, sizeof(activationCode) - 1);
    activationCode[sizeof(activationCode) - 1] = '\0';
    configuredBeepCount = DEFAULT_BEEP_COUNT;

#ifdef FSCom
    auto file = FSCom.open(GAULIX_PAGER_CONFIG_FILE, FILE_O_READ);
    if (!file) {
        return;
    }

    String codeLine = file.readStringUntil('\n');
    codeLine.trim();
    if (codeLine.length() > 0 && codeLine.length() < sizeof(activationCode)) {
        strncpy(activationCode, codeLine.c_str(), sizeof(activationCode) - 1);
        activationCode[sizeof(activationCode) - 1] = '\0';
    }

    if (file.available()) {
        const long beepCount = file.parseInt();
        if (beepCount >= 0 && beepCount <= 20) {
            configuredBeepCount = static_cast<uint8_t>(beepCount);
        }
    }
    file.close();
#endif
}

bool GaulixPagerModule::saveConfig()
{
#ifdef FSCom
    SafeFile file(GAULIX_PAGER_CONFIG_FILE, true);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s\n%u\n", activationCode, configuredBeepCount);
    file.write(reinterpret_cast<const uint8_t *>(buf), strlen(buf));
    return file.close();
#else
    return false;
#endif
}

void GaulixPagerModule::recordAlert()
{
    alertCount++;
    lastAlertTime = getTime();
}

int GaulixPagerModule::findAlerteChannelIndex()
{
    for (ChannelIndex i = 0; i < MAX_NUM_CHANNELS; i++) {
        const meshtastic_Channel &ch = channels.getByIndex(i);
        if (ch.settings.name[0] && strcmp(ch.settings.name, "Alerte") == 0) {
            return i;
        }
    }
    return -1;
}

bool GaulixPagerModule::isAcceptedPacket(const meshtastic_MeshPacket &mp)
{
    if (isFromUs(&mp)) {
        return false;
    }

    if (isToUs(&mp)) {
        return true;
    }

    const int alerteCh = findAlerteChannelIndex();
    if (alerteCh >= 0 && mp.channel == static_cast<uint8_t>(alerteCh) && isBroadcast(mp.to)) {
        return true;
    }

    return false;
}

const char *GaulixPagerModule::skipSpaces(const char *msg)
{
    while (msg && (*msg == ' ' || *msg == '\t')) {
        msg++;
    }
    return msg;
}

bool GaulixPagerModule::parseFinCommand(const char *msg)
{
    msg = skipSpaces(msg);
    if (!msg) {
        return false;
    }
    return strncmp(msg, "#fin", 4) == 0 && (msg[4] == '\0' || std::isspace(static_cast<unsigned char>(msg[4])));
}

bool GaulixPagerModule::parseStatusCommand(const char *msg)
{
    msg = skipSpaces(msg);
    if (!msg) {
        return false;
    }
    return strncmp(msg, "#status", 7) == 0 && (msg[7] == '\0' || std::isspace(static_cast<unsigned char>(msg[7])));
}

bool GaulixPagerModule::parseBeepCommand(const char *msg, uint8_t *outCount)
{
    msg = skipSpaces(msg);
    if (!msg || strncmp(msg, "#b", 2) != 0 || (msg[2] != '\0' && !std::isspace(static_cast<unsigned char>(msg[2])))) {
        return false;
    }

    const char *arg = skipSpaces(msg + 2);
    if (!arg || !arg[0]) {
        return false;
    }

    char *end = nullptr;
    const long value = strtol(arg, &end, 10);
    if (end == arg || (end && *skipSpaces(end) != '\0') || value < 0 || value > 20) {
        return false;
    }

    *outCount = static_cast<uint8_t>(value);
    return true;
}

bool GaulixPagerModule::parseCodeCommand(const char *msg, char *oldCode, size_t oldLen, char *newCode, size_t newLen)
{
    msg = skipSpaces(msg);
    if (!msg || strncmp(msg, "#code", 5) != 0 || (msg[5] != '\0' && !std::isspace(static_cast<unsigned char>(msg[5])))) {
        return false;
    }

    const char *arg = skipSpaces(msg + 5);
    if (!arg || !arg[0]) {
        return false;
    }

    size_t oldIdx = 0;
    while (arg[oldIdx] && !std::isspace(static_cast<unsigned char>(arg[oldIdx])) && oldIdx < oldLen - 1) {
        oldCode[oldIdx] = arg[oldIdx];
        oldIdx++;
    }
    oldCode[oldIdx] = '\0';
    if (oldIdx == 0) {
        return false;
    }

    arg = skipSpaces(arg + oldIdx);
    if (!arg || !arg[0]) {
        return false;
    }

    size_t newIdx = 0;
    while (arg[newIdx] && !std::isspace(static_cast<unsigned char>(arg[newIdx])) && newIdx < newLen - 1) {
        newCode[newIdx] = arg[newIdx];
        newIdx++;
    }
    newCode[newIdx] = '\0';
    return newIdx > 0;
}

bool GaulixPagerModule::parseAlertCommand(const char *msg, const char *keyword, char *code, size_t codeLen,
                                          const char **outText)
{
    msg = skipSpaces(msg);
    if (!msg || !keyword) {
        return false;
    }

    const size_t keywordLen = strlen(keyword);
    if (strncmp(msg, keyword, keywordLen) != 0 ||
        (msg[keywordLen] != '\0' && !std::isspace(static_cast<unsigned char>(msg[keywordLen])))) {
        return false;
    }

    const char *arg = skipSpaces(msg + keywordLen);
    if (!arg || !arg[0]) {
        return false;
    }

    size_t codeIdx = 0;
    while (arg[codeIdx] && !std::isspace(static_cast<unsigned char>(arg[codeIdx])) && codeIdx < codeLen - 1) {
        code[codeIdx] = arg[codeIdx];
        codeIdx++;
    }
    code[codeIdx] = '\0';
    if (codeIdx == 0) {
        return false;
    }

    arg = skipSpaces(arg + codeIdx);
    *outText = arg ? arg : "";
    return true;
}

bool GaulixPagerModule::activationCodeMatches(const char *code)
{
    return code && code[0] && strcmp(code, activationCode) == 0;
}

void GaulixPagerModule::playBeeps(uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        playBeep();
    }
}

void GaulixPagerModule::playFinBeeps()
{
    playBeep();
    playBeep();
}

void GaulixPagerModule::drawAlertFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->clear();

    const int16_t centerX = x + display->getWidth() / 2;

    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_MEDIUM);
    display->drawString(centerX, y + 2, "ALERTE SECOURS");

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    if (alertText[0]) {
        display->drawStringMaxWidth(x + 2, y + 20, display->getWidth() - 4, alertText);
    }

    if (lastAlertTime != 0) {
        time_t t = lastAlertTime;
        struct tm *tmInfo = localtime(&t);
        if (tmInfo) {
            char timeBuf[16];
            strftime(timeBuf, sizeof(timeBuf), "%H:%M", tmInfo);
            display->setTextAlignment(TEXT_ALIGN_RIGHT);
            display->drawString(x + display->getWidth() - 2, y + display->getHeight() - FONT_HEIGHT_SMALL - 10, timeBuf);
        }
    }

    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(centerX, y + display->getHeight() - FONT_HEIGHT_SMALL - 2, "Appui = acquitter");
}

void GaulixPagerModule::showAlertScreen()
{
    if (!screen) {
        return;
    }
    screen->startAlert(drawAlertFrame);
}

void GaulixPagerModule::triggerAlert(const char *text, const meshtastic_MeshPacket &mp)
{
    memset(alertText, 0, sizeof(alertText));
    if (text && text[0]) {
        strncpy(alertText, text, sizeof(alertText) - 1);
    } else {
        strncpy(alertText, "Alerte secours", sizeof(alertText) - 1);
    }

    alertSourceNode = mp.from;
    alertSourceChannel = mp.channel;

    recordAlert();
    alertActive = true;
    ledBlinkState = false;
    alertStartedMs = millis();
    lastContinuousBeepMs = alertStartedMs;

    LOG_INFO("GaulixPager: alerte '%s' de 0x%08x", alertText, alertSourceNode);

    if (configuredBeepCount == 0) {
        playBeep();
    } else {
        playBeeps(configuredBeepCount);
    }
    showAlertScreen();

#if defined(PIN_LED2)
    pinMode(PIN_LED2, OUTPUT);
    digitalWrite(PIN_LED2, HIGH);
#endif

    setIntervalFromNow(LED_BLINK_MS);
}

void GaulixPagerModule::sendReplyDm(const meshtastic_MeshPacket &rx, const char *text)
{
    if (!text || !rx.from || rx.from == NODENUM_BROADCAST) {
        return;
    }

    meshtastic_MeshPacket *p = allocDataPacket();
    p->to = rx.from;
    p->channel = rx.channel;
    p->want_ack = false;
    p->decoded.want_response = false;

    const size_t len = strnlen(text, sizeof(p->decoded.payload.bytes));
    p->decoded.payload.size = len;
    memcpy(p->decoded.payload.bytes, text, len);

    service->sendToMesh(p);
}

void GaulixPagerModule::sendAckDm()
{
    if (!alertSourceNode || alertSourceNode == NODENUM_BROADCAST) {
        return;
    }

    char timeBuf[8] = "--:--";
    time_t t = getTime();
    struct tm *tmInfo = localtime(&t);
    if (tmInfo) {
        strftime(timeBuf, sizeof(timeBuf), "%H:%M", tmInfo);
    }

    char reply[64];
    snprintf(reply, sizeof(reply), "Pager OK — alerte reçue à %s", timeBuf);

    meshtastic_MeshPacket ackTarget = meshtastic_MeshPacket_init_zero;
    ackTarget.from = alertSourceNode;
    ackTarget.channel = alertSourceChannel;
    sendReplyDm(ackTarget, reply);
    LOG_INFO("GaulixPager: ACK vers 0x%08x", alertSourceNode);
}

void GaulixPagerModule::sendStatusReply(const meshtastic_MeshPacket &mp)
{
    char batteryLine[32];
    formatBatteryLine(batteryLine, sizeof(batteryLine));

    char bipsLine[12];
    if (configuredBeepCount == 0) {
        strncpy(bipsLine, "continu", sizeof(bipsLine) - 1);
        bipsLine[sizeof(bipsLine) - 1] = '\0';
    } else {
        snprintf(bipsLine, sizeof(bipsLine), "%u", configuredBeepCount);
    }

    char reply[160];
    snprintf(reply, sizeof(reply), "Pager Gaulix - %s | Alertes: %lu | %s | Bips: %s | Code: %s",
             alertActive ? "En alerte" : "En ecoute", static_cast<unsigned long>(alertCount), batteryLine, bipsLine,
             activationCode);

    sendReplyDm(mp, reply);
}

bool GaulixPagerModule::handleCodeCommand(const char *msg, const meshtastic_MeshPacket &mp)
{
    char oldCode[32];
    char newCode[32];
    if (!parseCodeCommand(msg, oldCode, sizeof(oldCode), newCode, sizeof(newCode))) {
        return false;
    }

    if (!activationCodeMatches(oldCode)) {
        sendReplyDm(mp, "Pager ERR — code incorrect");
        return true;
    }

    strncpy(activationCode, newCode, sizeof(activationCode) - 1);
    activationCode[sizeof(activationCode) - 1] = '\0';
    saveConfig();
    sendReplyDm(mp, "Pager OK — code mis à jour");
    LOG_INFO("GaulixPager: code d'activation mis à jour");
    return true;
}

void GaulixPagerModule::acknowledgeAlert()
{
    if (!alertActive) {
        return;
    }

    sendAckDm();
    clearAlert(false);
    playBoop();
}

void GaulixPagerModule::clearAlert(bool playFinMelody)
{
    if (!alertActive && !playFinMelody) {
        return;
    }

    LOG_INFO("GaulixPager: fin d'alerte");

    alertActive = false;
    alertSourceNode = 0;
    alertSourceChannel = 0;
    alertStartedMs = 0;
    lastContinuousBeepMs = 0;
    ledBlinkState = false;

#if defined(PIN_LED2)
    digitalWrite(PIN_LED2, LOW);
#endif

    if (screen) {
        screen->endAlert();
    }

    if (playFinMelody) {
        playFinBeeps();
    }
}

ProcessMessage GaulixPagerModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (!isAcceptedPacket(mp) || mp.decoded.payload.size == 0) {
        return ProcessMessage::CONTINUE;
    }

    char buf[260];
    memset(buf, 0, sizeof(buf));
    size_t n = mp.decoded.payload.size;
    if (n > sizeof(buf) - 1) {
        n = sizeof(buf) - 1;
    }
    memcpy(buf, mp.decoded.payload.bytes, n);

    if (parseFinCommand(buf)) {
        clearAlert(true);
        return ProcessMessage::CONTINUE;
    }

    if (parseStatusCommand(buf)) {
        sendStatusReply(mp);
        return ProcessMessage::CONTINUE;
    }

    uint8_t beepCount = 0;
    if (parseBeepCommand(buf, &beepCount)) {
        configuredBeepCount = beepCount;
        saveConfig();
        char reply[48];
        if (beepCount == 0) {
            snprintf(reply, sizeof(reply), "Pager OK — bips: continu");
        } else {
            snprintf(reply, sizeof(reply), "Pager OK — bips: %u", beepCount);
        }
        sendReplyDm(mp, reply);
        return ProcessMessage::CONTINUE;
    }

    if (handleCodeCommand(buf, mp)) {
        return ProcessMessage::CONTINUE;
    }

    char code[32];
    const char *text = nullptr;
    const bool isAlerte = parseAlertCommand(buf, "#alerte", code, sizeof(code), &text);
    const bool isSecours = !isAlerte && parseAlertCommand(buf, "#secours", code, sizeof(code), &text);
    if (isAlerte || isSecours) {
        if (!activationCodeMatches(code)) {
            LOG_WARN("GaulixPager: code d'activation invalide");
            sendReplyDm(mp, "Pager ERR — code invalide");
            return ProcessMessage::CONTINUE;
        }
        triggerAlert(text, mp);
    }

    return ProcessMessage::CONTINUE;
}

int GaulixPagerModule::handleInputEvent(const InputEvent *event)
{
    if (!alertActive || !event) {
        return 0;
    }

    if (event->inputEvent == INPUT_BROKER_SELECT || event->inputEvent == INPUT_BROKER_USER_PRESS) {
        acknowledgeAlert();
        return 1;
    }

    return 0;
}

int32_t GaulixPagerModule::runOnce()
{
    if (!alertActive) {
#if defined(PIN_LED2)
        digitalWrite(PIN_LED2, LOW);
#endif
        return INT32_MAX;
    }

    if ((uint32_t)(millis() - alertStartedMs) >= ALERT_TIMEOUT_MS) {
        LOG_INFO("GaulixPager: coupure auto apres 30 min");
        clearAlert(false);
        return INT32_MAX;
    }

    if (configuredBeepCount == 0 &&
        !Throttle::isWithinTimespanMs(lastContinuousBeepMs, CONTINUOUS_BEEP_INTERVAL_MS)) {
        lastContinuousBeepMs = millis();
        playBeep();
    }

#if defined(PIN_LED2)
    ledBlinkState = !ledBlinkState;
    digitalWrite(PIN_LED2, ledBlinkState ? HIGH : LOW);
#endif

    return LED_BLINK_MS;
}

void GaulixPagerModule::formatBatteryLine(char *buf, size_t len)
{
    if (!powerStatus) {
        snprintf(buf, len, "Batterie : --");
        return;
    }

    if (powerStatus->getHasBattery()) {
        const uint8_t pct = powerStatus->getBatteryChargePercent();
        if (powerStatus->getIsCharging() && pct < 100) {
            snprintf(buf, len, "Batterie : %u %% +", pct);
        } else {
            snprintf(buf, len, "Batterie : %u %%", pct);
        }
        return;
    }

    if (powerStatus->getHasUSB() || powerStatus->getBatteryChargePercent() == 101) {
        snprintf(buf, len, "Batterie : USB");
        return;
    }

    snprintf(buf, len, "Batterie : --");
}

void GaulixPagerModule::formatLastAlertLine(char *buf, size_t len)
{
    if (lastAlertTime == 0) {
        snprintf(buf, len, "Dernière : --:--");
        return;
    }

    time_t t = lastAlertTime;
    struct tm *tmInfo = localtime(&t);
    if (!tmInfo) {
        snprintf(buf, len, "Dernière : --:--");
        return;
    }

    char timeBuf[8];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", tmInfo);
    snprintf(buf, len, "Dernière : %s", timeBuf);
}

void GaulixPagerModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->clear();

    int16_t lineY = y + 2;

    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_MEDIUM);
    if (display->getStringWidth(GAULIX_PAGER_TITLE) > display->getWidth()) {
        display->setFont(FONT_SMALL);
        display->drawString(x + display->getWidth() / 2, lineY, GAULIX_PAGER_TITLE);
        lineY += FONT_HEIGHT_SMALL + 1;
    } else {
        display->drawString(x + display->getWidth() / 2, lineY, GAULIX_PAGER_TITLE);
        lineY += FONT_HEIGHT_MEDIUM + 1;
    }

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const int16_t statusX = x + 4;
    display->fillCircle(statusX + 3, lineY + FONT_HEIGHT_SMALL / 2, 3);
    display->drawString(statusX + 10, lineY, alertActive ? "En alerte" : "En écoute");
    lineY += FONT_HEIGHT_SMALL + 1;

    char lineBuf[32];
    snprintf(lineBuf, sizeof(lineBuf), "Alerte(s) : %lu", static_cast<unsigned long>(alertCount));
    display->drawString(x + 2, lineY, lineBuf);

    formatLastAlertLine(lineBuf, sizeof(lineBuf));
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(x + display->getWidth() - 2, lineY, lineBuf);
    lineY += FONT_HEIGHT_SMALL + 1;

    formatBatteryLine(lineBuf, sizeof(lineBuf));
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + 2, lineY, lineBuf);
}

#endif
