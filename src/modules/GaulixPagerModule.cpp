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
#include <NonBlockingRtttl.h>
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
meshtastic_MeshPacket GaulixPagerModule::alertSourcePacket = meshtastic_MeshPacket_init_zero;
bool GaulixPagerModule::hasAlertSourcePacket = false;
char GaulixPagerModule::activationCode[32] = GAULIX_DEFAULT_ACTIVATION_CODE;
uint8_t GaulixPagerModule::configuredBeepCount = GaulixPagerModule::DEFAULT_BEEP_COUNT;
uint32_t GaulixPagerModule::alertStartedMs = 0;
uint32_t GaulixPagerModule::lastContinuousBeepMs = 0;
bool GaulixPagerModule::ledBlinkState = false;
GaulixPagerModule::SeenPacket GaulixPagerModule::seenPackets[GaulixPagerModule::ALERT_PACKET_DEDUP_SIZE] = {};
size_t GaulixPagerModule::seenPacketIndex = 0;

GaulixPagerModule::GaulixPagerModule()
    : SinglePortModule("gaulixpager", meshtastic_PortNum_TEXT_MESSAGE_APP), concurrency::OSThread("GaulixPager")
{
    isPromiscuous = true;
    requestFocus();
    // Force pager alerting defaults on each boot so stale persisted settings
    // don't make two flashed devices behave differently.
    ensureBuzzerReady();
    moduleConfig.has_external_notification = true;
    moduleConfig.external_notification.enabled = true;
    // GaulixPagerModule owns message alerts; avoid duplicate buzzer from ExternalNotificationModule.
    moduleConfig.external_notification.alert_message = false;
    moduleConfig.external_notification.alert_message_buzzer = false;
    moduleConfig.external_notification.alert_message_vibra = false;
#if defined(PIN_BUZZER)
    moduleConfig.external_notification.output_buzzer = PIN_BUZZER;
    moduleConfig.external_notification.use_pwm = true;
#endif
    applyChannelMuteDefaults();
    // Force EU 868 MHz on every boot; stale persisted region breaks Gaulix mesh interoperability.
    config.lora.region = meshtastic_Config_LoRaConfig_RegionCode_EU_868;
    config.lora.tx_enabled = true;
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

void GaulixPagerModule::applyChannelMuteDefaults()
{
    const int alerteCh = findAlerteChannelIndex();
    for (ChannelIndex i = 0; i < MAX_NUM_CHANNELS; i++) {
        meshtastic_Channel &ch = channels.getByIndex(i);
        if (!ch.has_settings || !ch.settings.name[0]) {
            continue;
        }
        if (!ch.settings.has_module_settings) {
            ch.settings.has_module_settings = true;
        }
        ch.settings.module_settings.is_muted = (static_cast<int>(i) != alerteCh);
    }
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

bool GaulixPagerModule::isPagerInternalReply(const char *msg)
{
    msg = skipSpaces(msg);
    if (!msg) {
        return false;
    }
    // ACK receipts must reach the message UI / phone app on the alert originator.
    if (strncmp(msg, "Pager ACK", 9) == 0) {
        return false;
    }
    return strncmp(msg, "Pager OK", 8) == 0 || strncmp(msg, "Pager ERR", 9) == 0 ||
           strncmp(msg, "Pager Gaulix", 12) == 0;
}

bool GaulixPagerModule::recentlySeenPacket(NodeNum from, uint32_t id)
{
    if (id == 0) {
        return false;
    }

    for (size_t i = 0; i < ALERT_PACKET_DEDUP_SIZE; i++) {
        if (seenPackets[i].id == id && seenPackets[i].from == from) {
            return true;
        }
    }
    return false;
}

void GaulixPagerModule::recordSeenPacket(NodeNum from, uint32_t id)
{
    if (id == 0) {
        return;
    }

    seenPackets[seenPacketIndex].from = from;
    seenPackets[seenPacketIndex].id = id;
    seenPacketIndex = (seenPacketIndex + 1) % ALERT_PACKET_DEDUP_SIZE;
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

bool GaulixPagerModule::parseAlertWithText(const char *msg, const char *keyword, const char **outText)
{
    msg = skipSpaces(msg);
    if (!msg || !keyword || !outText) {
        return false;
    }

    const size_t keywordLen = strlen(keyword);
    if (strncmp(msg, keyword, keywordLen) != 0 ||
        (msg[keywordLen] != '\0' && !std::isspace(static_cast<unsigned char>(msg[keywordLen])))) {
        return false;
    }

    const char *text = skipSpaces(msg + keywordLen);
    if (!text || !text[0]) {
        return false;
    }

    *outText = text;
    return true;
}

bool GaulixPagerModule::activationCodeMatches(const char *code)
{
    return code && code[0] && strcmp(code, activationCode) == 0;
}

void GaulixPagerModule::ensureBuzzerReady()
{
    config.device.buzzer_mode = meshtastic_Config_DeviceConfig_BuzzerMode_ALL_ENABLED;
#if defined(PIN_BUZZER)
    if (!config.device.buzzer_gpio) {
        config.device.buzzer_gpio = PIN_BUZZER;
    }
#endif
}

void GaulixPagerModule::prepareBuzzerForAlert()
{
    rtttl::stop();
#if defined(PIN_BUZZER)
    if (config.device.buzzer_gpio) {
        noTone(config.device.buzzer_gpio);
    }
#endif
    ensureBuzzerReady();
}

void GaulixPagerModule::scheduleAlertMaintenance()
{
    setIntervalFromNow(LED_BLINK_MS);
    runASAP = true;
}

int8_t GaulixPagerModule::alertLedPin()
{
#if defined(PIN_LED1) && (!defined(PIN_BUZZER) || (PIN_LED1) != (PIN_BUZZER))
    return PIN_LED1;
#elif defined(PIN_LED2) && (!defined(PIN_BUZZER) || (PIN_LED2) != (PIN_BUZZER))
    return PIN_LED2;
#else
    return -1;
#endif
}

void GaulixPagerModule::playBeeps(uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        playGaulixPagerBeep();
    }
}

void GaulixPagerModule::playFinBeeps()
{
    playGaulixPagerFinBeep();
    delay(GAULIX_BUZZER_PULSE_GAP_MS);
    playGaulixPagerFinBeep();
}

void GaulixPagerModule::drawAlertFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->clear();

    const int16_t centerX = x + display->getWidth() / 2;

    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(FONT_MEDIUM);
    display->drawString(centerX, y + 2, "ALERTE");

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
    alertSourcePacket = mp;
    hasAlertSourcePacket = true;

    recordAlert();
    alertActive = true;
    ledBlinkState = false;
    alertStartedMs = millis();
    lastContinuousBeepMs = alertStartedMs;

    const char *via = isBroadcast(mp.to) ? "broadcast canal Alerte" : "DM bipper";
    LOG_INFO("GaulixPager: alerte '%s' via %s de 0x%08x ch %u bips=%u", alertText, via, alertSourceNode, alertSourceChannel,
             configuredBeepCount);

    prepareBuzzerForAlert();
    if (configuredBeepCount == 0) {
        playBeep();
    } else {
        playBeeps(configuredBeepCount);
    }
    showAlertScreen();

    const int8_t ledPin = alertLedPin();
    if (ledPin >= 0) {
        pinMode(ledPin, OUTPUT);
        digitalWrite(ledPin, HIGH);
    }

    scheduleAlertMaintenance();
}

void GaulixPagerModule::sendReplyDm(const meshtastic_MeshPacket &rx, const char *text)
{
    if (!text || !rx.from || rx.from == NODENUM_BROADCAST) {
        LOG_WARN("GaulixPager: pas de destinataire pour reponse DM");
        return;
    }

    meshtastic_MeshPacket *p = allocDataPacket();
    p->to = rx.from;
    p->channel = rx.channel;
    p->want_ack = false;
    p->decoded.want_response = false;
    p->decoded.dest = rx.from;

    meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(rx.from);
    const NodeNum myNodeNum = nodeDB->getNodeNum();
    if (node && node->num != myNodeNum && nodeInfoLiteHasUser(node) && node->public_key.size == 32) {
        p->pki_encrypted = true;
        p->channel = 0;
    }

    const size_t len = strnlen(text, sizeof(p->decoded.payload.bytes));
    p->decoded.payload.size = len;
    memcpy(p->decoded.payload.bytes, text, len);

    service->sendToMesh(p, RX_SRC_LOCAL, true);
    LOG_INFO("GaulixPager: DM vers 0x%08x ch %u", rx.from, p->channel);
}

void GaulixPagerModule::sendAckDm()
{
    if (!alertSourceNode || alertSourceNode == NODENUM_BROADCAST) {
        LOG_WARN("GaulixPager: ACK ignore — source 0x%08x", alertSourceNode);
        return;
    }

    char timeBuf[8] = "--:--";
    time_t t = getTime();
    struct tm *tmInfo = localtime(&t);
    if (tmInfo) {
        strftime(timeBuf, sizeof(timeBuf), "%H:%M", tmInfo);
    }

    char reply[64];
    snprintf(reply, sizeof(reply), "Pager ACK alerte %s", timeBuf);

    if (hasAlertSourcePacket) {
        sendReplyDm(alertSourcePacket, reply);
    } else {
        meshtastic_MeshPacket ackTarget = meshtastic_MeshPacket_init_zero;
        ackTarget.from = alertSourceNode;
        ackTarget.channel = alertSourceChannel;
        sendReplyDm(ackTarget, reply);
    }
    LOG_INFO("GaulixPager: ACK vers 0x%08x ch %u", alertSourceNode, alertSourceChannel);
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
    playGaulixPagerFinBeep();
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
    hasAlertSourcePacket = false;
    alertStartedMs = 0;
    lastContinuousBeepMs = 0;
    ledBlinkState = false;

    const int8_t ledPin = alertLedPin();
    if (ledPin >= 0) {
        digitalWrite(ledPin, LOW);
    }

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

    const NodeNum sender = getFrom(&mp);
    if (recentlySeenPacket(sender, mp.id)) {
        return ProcessMessage::CONTINUE;
    }
    recordSeenPacket(sender, mp.id);

    char buf[260];
    memset(buf, 0, sizeof(buf));
    size_t n = mp.decoded.payload.size;
    if (n > sizeof(buf) - 1) {
        n = sizeof(buf) - 1;
    }
    memcpy(buf, mp.decoded.payload.bytes, n);

    if (isPagerInternalReply(buf)) {
        return ProcessMessage::STOP;
    }

    if (parseFinCommand(buf)) {
        clearAlert(true);
        return ProcessMessage::STOP;
    }

    if (parseStatusCommand(buf)) {
        sendStatusReply(mp);
        return ProcessMessage::STOP;
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
        return ProcessMessage::STOP;
    }

    if (handleCodeCommand(buf, mp)) {
        return ProcessMessage::STOP;
    }

    const char *alertTextArg = nullptr;
    const bool isAlerte = parseAlertWithText(buf, "#alerte", &alertTextArg);
    const bool isSecours = !isAlerte && parseAlertWithText(buf, "#secours", &alertTextArg);
    if (isAlerte || isSecours) {
        triggerAlert(alertTextArg, mp);
        return ProcessMessage::STOP;
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
    const int8_t ledPin = alertLedPin();

    if (!alertActive) {
        if (ledPin >= 0) {
            digitalWrite(ledPin, LOW);
        }
        return INT32_MAX;
    }

    if (configuredBeepCount == 0 &&
        !Throttle::isWithinTimespanMs(lastContinuousBeepMs, CONTINUOUS_BEEP_INTERVAL_MS)) {
        lastContinuousBeepMs = millis();
        prepareBuzzerForAlert();
        playGaulixPagerBeep();
    }

    if (ledPin >= 0) {
        ledBlinkState = !ledBlinkState;
        digitalWrite(ledPin, ledBlinkState ? HIGH : LOW);
    }

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
    char lineBuf[32];
    snprintf(lineBuf, sizeof(lineBuf), "Alerte(s) : %lu", static_cast<unsigned long>(alertCount));
    display->drawString(x + 2, lineY, lineBuf);
    lineY += FONT_HEIGHT_SMALL + 1;

    formatLastAlertLine(lineBuf, sizeof(lineBuf));
    display->drawString(x + 2, lineY, lineBuf);
    lineY += FONT_HEIGHT_SMALL + 1;

    formatBatteryLine(lineBuf, sizeof(lineBuf));
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + 2, lineY, lineBuf);
}

#endif
