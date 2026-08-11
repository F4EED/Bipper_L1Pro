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
#if !MESHTASTIC_EXCLUDE_GPS
#include "GPSStatus.h"
#endif
#include "graphics/Screen.h"
#include "graphics/ScreenFonts.h"
#include "main.h"
#include "mesh/MeshModule.h"
#include "mesh/PositionPrecision.h"
#include "mesh/Throttle.h"
#if !MESHTASTIC_EXCLUDE_GPS
#include "gps/GPS.h"
#include "modules/PositionModule.h"
#endif
#include "meshUtils.h"
#include <NonBlockingRtttl.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>

static const char *GAULIX_PAGER_CONFIG_FILE = "/prefs/gaulixpager.cfg";
static const char *GAULIX_PAGER_CONFIG_TAGS_MARKER = "TAGS";

static constexpr const char *GAULIX_DEFAULT_OWNER_NAME = "Bipper de demo";

static bool gaulixOwnerNameIsFactoryDefault()
{
    if (!owner.long_name[0]) {
        return true;
    }
    if (strncmp(owner.long_name, "Meshtastic ", 11) == 0) {
        return true;
    }
    if (strncmp(owner.long_name, "42BIP_", 6) == 0) {
        return true;
    }
    if (strstr(owner.long_name, "CHANGER") != nullptr || strstr(owner.long_name, "ATTENTION") != nullptr) {
        return true;
    }
    return false;
}

static void applyGaulixOwnerNameDefaults()
{
    if (!gaulixOwnerNameIsFactoryDefault()) {
        return;
    }

    snprintf(owner.long_name, sizeof(owner.long_name), "%s", GAULIX_DEFAULT_OWNER_NAME);
    clampLongName(owner.long_name);

    meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(nodeDB->getNodeNum());
    if (node) {
        strncpy(node->long_name, owner.long_name, sizeof(node->long_name) - 1);
        node->long_name[sizeof(node->long_name) - 1] = '\0';
    }
}

GaulixPagerModule *gaulixPagerModule = nullptr;

uint32_t GaulixPagerModule::alertCount = 0;
uint32_t GaulixPagerModule::lastAlertTime = 0;
bool GaulixPagerModule::alertActive = false;
char GaulixPagerModule::alertText[96] = {};
char GaulixPagerModule::alertEmitterName[40] = {};
uint32_t GaulixPagerModule::activeAlertId = 0;
GaulixPagerModule::PagerCommandKind GaulixPagerModule::activeAlertKind = GaulixPagerModule::PagerCommandKind::Alerte;
NodeNum GaulixPagerModule::alertSourceNode = 0;
uint8_t GaulixPagerModule::alertSourceChannel = 0;
meshtastic_MeshPacket GaulixPagerModule::alertSourcePacket = meshtastic_MeshPacket_init_zero;
bool GaulixPagerModule::hasAlertSourcePacket = false;
char GaulixPagerModule::activationCode[32] = GAULIX_DEFAULT_ACTIVATION_CODE;
uint8_t GaulixPagerModule::configuredBeepCount = GaulixPagerModule::DEFAULT_BEEP_COUNT;
char GaulixPagerModule::configuredServiceTagValues[GaulixPagerModule::SERVICE_TAG_SLOT_COUNT]
                                                  [GaulixPagerModule::SERVICE_TAG_VALUE_LEN] = {};
uint32_t GaulixPagerModule::alertStartedMs = 0;
uint32_t GaulixPagerModule::lastContinuousBeepMs = 0;
uint32_t GaulixPagerModule::lastLowBatteryBeepMs = 0;
bool GaulixPagerModule::lowBatteryWarningActive = false;
bool GaulixPagerModule::ledBlinkState = false;
GaulixPagerModule::AlertHistoryEntry GaulixPagerModule::alertHistory[GaulixPagerModule::ALERT_HISTORY_MAX] = {};
size_t GaulixPagerModule::alertHistoryCount = 0;
size_t GaulixPagerModule::alertHistoryHead = 0;
size_t GaulixPagerModule::alertHistoryScrollIndex = 0;
size_t GaulixPagerModule::currentAlertHistoryPhysIdx = SIZE_MAX;
GaulixPagerModule::SeenPacket GaulixPagerModule::seenPackets[GaulixPagerModule::ALERT_PACKET_DEDUP_SIZE] = {};
size_t GaulixPagerModule::seenPacketIndex = 0;
uint8_t GaulixPagerModule::pagerFrameIndex = 255;

GaulixPagerModule::GaulixPagerModule()
    : SinglePortModule("gaulixpager", meshtastic_PortNum_TEXT_MESSAGE_APP), concurrency::OSThread("GaulixPager")
{
    alertCount = 0;
    lastAlertTime = 0;
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
    ensureGaulixChannelsInstalled();
    // Force Gaulix operational defaults on every boot (stale flash must not diverge).
    config.lora.region = meshtastic_Config_LoRaConfig_RegionCode_EU_868;
    config.lora.tx_enabled = true;
    config.device.rebroadcast_mode = meshtastic_Config_DeviceConfig_RebroadcastMode_LOCAL_ONLY;
    config.position.gps_update_interval = 200;
    config.bluetooth.fixed_pin = 123456;
    config.bluetooth.mode = meshtastic_Config_BluetoothConfig_PairingMode_FIXED_PIN;
    applyGaulixOwnerNameDefaults();
    loadConfig();
#if !defined(MESHTASTIC_EXCLUDE_INPUTBROKER)
    if (inputBroker) {
        inputObserver.observe(inputBroker);
    }
#endif
    setIntervalFromNow(LOW_BATTERY_CHECK_MS);
}

void GaulixPagerModule::loadConfig()
{
    strncpy(activationCode, GAULIX_DEFAULT_ACTIVATION_CODE, sizeof(activationCode) - 1);
    activationCode[sizeof(activationCode) - 1] = '\0';
    configuredBeepCount = DEFAULT_BEEP_COUNT;
    memset(configuredServiceTagValues, 0, sizeof(configuredServiceTagValues));
    setServiceTagValue(1, GAULIX_DEFAULT_SERVICE_TAG_T1);

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
        // Consomme le saut de ligne après le nombre de bips.
        while (file.available() && file.peek() != '\n' && file.peek() != '\r') {
            file.read();
        }
        while (file.available() && (file.peek() == '\n' || file.peek() == '\r')) {
            file.read();
        }
    }

    if (!file.available()) {
        file.close();
        return;
    }

    String firstTagLine = file.readStringUntil('\n');
    firstTagLine.trim();

    if (!file.available()) {
        // Ancien format 3 lignes : masque numérique T1..T4
        const long legacyMask = firstTagLine.toInt();
        auto migrateLegacyTag = [](uint8_t tag) {
            switch (tag) {
            case 1:
                setServiceTagValue(1, "SDIS42");
                break;
            case 2:
                setServiceTagValue(2, "UIDIOM42");
                break;
            case 3:
                setServiceTagValue(3, "Ricamarie");
                break;
            case 4:
                setServiceTagValue(4, "ligerien");
                break;
            default:
                break;
            }
        };
        if (legacyMask >= 1 && legacyMask <= 4) {
            migrateLegacyTag(static_cast<uint8_t>(legacyMask));
        } else if (legacyMask > 0 && legacyMask <= 15) {
            for (uint8_t tag = 1; tag <= 4; tag++) {
                if (legacyMask & static_cast<uint8_t>(1U << (tag - 1))) {
                    migrateLegacyTag(tag);
                }
            }
        }
        file.close();
        return;
    }

    if (firstTagLine.equals(GAULIX_PAGER_CONFIG_TAGS_MARKER)) {
        for (uint8_t tag = 1; tag <= SERVICE_TAG_SLOT_COUNT && file.available(); tag++) {
            String valueLine = file.readStringUntil('\n');
            valueLine.trim();
            setServiceTagValue(tag, valueLine.c_str());
        }
        file.close();
        return;
    }

    // v1.9 sans marqueur TAGS : première ligne = T1 (jusqu'à 4 ou 10 lignes)
    setServiceTagValue(1, firstTagLine.c_str());
    for (uint8_t tag = 2; tag <= SERVICE_TAG_SLOT_COUNT && file.available(); tag++) {
        String valueLine = file.readStringUntil('\n');
        valueLine.trim();
        setServiceTagValue(tag, valueLine.c_str());
    }
    file.close();
#endif

    if (!getServiceTagValue(1)[0]) {
        setServiceTagValue(1, GAULIX_DEFAULT_SERVICE_TAG_T1);
    }
}

bool GaulixPagerModule::saveConfig()
{
#ifdef FSCom
    SafeFile file(GAULIX_PAGER_CONFIG_FILE, true);
    char buf[96];
    int len = snprintf(buf, sizeof(buf), "%s\n%u\n%s\n", activationCode, configuredBeepCount, GAULIX_PAGER_CONFIG_TAGS_MARKER);
    if (len <= 0 || static_cast<size_t>(len) >= sizeof(buf)) {
        return false;
    }
    file.write(reinterpret_cast<const uint8_t *>(buf), len);
    for (uint8_t tag = 1; tag <= SERVICE_TAG_SLOT_COUNT; tag++) {
        len = snprintf(buf, sizeof(buf), "%s\n", getServiceTagValue(tag));
        if (len <= 0 || static_cast<size_t>(len) >= sizeof(buf)) {
            return false;
        }
        file.write(reinterpret_cast<const uint8_t *>(buf), len);
    }
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

void GaulixPagerModule::ensureGaulixChannelsInstalled()
{
    if (!channels.ensureGaulixFactoryChannels()) {
        return;
    }
    applyChannelMuteDefaults();
    if (nodeDB) {
        nodeDB->saveToDisk(SEGMENT_CHANNELS);
    }
}

void GaulixPagerModule::recordAlert()
{
    alertCount++;
    lastAlertTime = getTime();
}

void GaulixPagerModule::unrecordAlert()
{
    if (alertCount > 0) {
        alertCount--;
    }
}

size_t GaulixPagerModule::getAlertHistoryCount()
{
    return alertHistoryCount;
}

bool GaulixPagerModule::getAlertHistoryEntry(size_t index, AlertHistoryEntry &out)
{
    if (index >= alertHistoryCount) {
        return false;
    }
    const size_t phys = (alertHistoryHead + ALERT_HISTORY_MAX - 1 - index) % ALERT_HISTORY_MAX;
    out = alertHistory[phys];
    return true;
}

size_t GaulixPagerModule::getAlertHistoryScrollIndex()
{
    return alertHistoryScrollIndex;
}

void GaulixPagerModule::scrollAlertHistory(int delta)
{
    if (alertHistoryCount == 0) {
        return;
    }
    if (delta < 0 && alertHistoryScrollIndex > 0) {
        alertHistoryScrollIndex--;
    } else if (delta > 0 && alertHistoryScrollIndex + 1 < alertHistoryCount) {
        alertHistoryScrollIndex++;
    }
}

void GaulixPagerModule::addAlertHistoryEntry(const char *text, const meshtastic_MeshPacket &mp, bool isInfo)
{
    AlertHistoryEntry &entry = alertHistory[alertHistoryHead];
    memset(&entry, 0, sizeof(entry));
    entry.time = getTime();
    entry.from = getFrom(&mp);
    entry.viaDm = isToUs(&mp) && !isBroadcast(mp.to);
    entry.isInfo = isInfo;
    if (text && text[0]) {
        strncpy(entry.text, text, sizeof(entry.text) - 1);
    } else if (isInfo) {
        strncpy(entry.text, "Message info", sizeof(entry.text) - 1);
    } else {
        strncpy(entry.text, "Alerte secours", sizeof(entry.text) - 1);
    }

    if (!isInfo) {
        currentAlertHistoryPhysIdx = alertHistoryHead;
    }
    alertHistoryHead = (alertHistoryHead + 1) % ALERT_HISTORY_MAX;
    if (alertHistoryCount < ALERT_HISTORY_MAX) {
        alertHistoryCount++;
    }
    alertHistoryScrollIndex = 0;
}

void GaulixPagerModule::markCurrentAlertHistoryAcknowledged()
{
    if (currentAlertHistoryPhysIdx >= ALERT_HISTORY_MAX) {
        return;
    }
    alertHistory[currentAlertHistoryPhysIdx].acknowledged = true;
}

void GaulixPagerModule::markCurrentAlertHistoryTimedOut()
{
    if (currentAlertHistoryPhysIdx >= ALERT_HISTORY_MAX) {
        return;
    }
    if (!alertHistory[currentAlertHistoryPhysIdx].acknowledged) {
        alertHistory[currentAlertHistoryPhysIdx].timedOut = true;
    }
}

void GaulixPagerModule::markCurrentAlertHistoryClosedByFin()
{
    if (currentAlertHistoryPhysIdx >= ALERT_HISTORY_MAX) {
        return;
    }
    alertHistory[currentAlertHistoryPhysIdx].closedByFin = true;
    alertHistory[currentAlertHistoryPhysIdx].timedOut = false;
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

int GaulixPagerModule::findBaliseChannelIndex()
{
    for (ChannelIndex i = 0; i < MAX_NUM_CHANNELS; i++) {
        const meshtastic_Channel &ch = channels.getByIndex(i);
        if (ch.settings.name[0] && strcmp(ch.settings.name, "Fr_Balise") == 0) {
            return i;
        }
    }
    return 0;
}

bool GaulixPagerModule::isLocalConfigCommand(const char *msg)
{
    if (parseStatusCommand(msg)) {
        return true;
    }
    if (parseAckCommand(msg)) {
        return true;
    }
    uint8_t beepCount = 0;
    if (parseBeepCommand(msg, &beepCount)) {
        return true;
    }
    uint8_t tag = 0;
    char tagValue[SERVICE_TAG_VALUE_LEN];
    if (parseTagValueSetCommand(msg, &tag, tagValue, sizeof(tagValue))) {
        return true;
    }
    if (parseTagSetBulkCommand(msg, false)) {
        return true;
    }
    char oldCode[32];
    char newCode[32];
    return parseCodeCommand(msg, oldCode, sizeof(oldCode), newCode, sizeof(newCode));
}

bool GaulixPagerModule::isAcceptedPacket(const meshtastic_MeshPacket &mp)
{
    if (isFromUs(&mp)) {
        if (!isToUs(&mp) || mp.decoded.payload.size == 0) {
            return false;
        }
        char buf[260];
        memset(buf, 0, sizeof(buf));
        size_t n = mp.decoded.payload.size;
        if (n > sizeof(buf) - 1) {
            n = sizeof(buf) - 1;
        }
        memcpy(buf, mp.decoded.payload.bytes, n);
        return isLocalConfigCommand(buf);
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
    uint32_t alertId = 0;
    char affiliation[SERVICE_TAG_VALUE_LEN];
    return parseFinCommandEx(msg, &alertId, affiliation, sizeof(affiliation));
}

bool GaulixPagerModule::parseFinCommandEx(const char *msg, uint32_t *outAlertId, char *outAffiliation, size_t affiliationLen)
{
    if (outAlertId) {
        *outAlertId = 0;
    }
    if (outAffiliation && affiliationLen > 0) {
        outAffiliation[0] = '\0';
    }
    msg = skipSpaces(msg);
    if (!msg || strncmp(msg, "#fin", 4) != 0 ||
        (msg[4] != '\0' && !std::isspace(static_cast<unsigned char>(msg[4])))) {
        return false;
    }

    const char *cursor = skipSpaces(msg + 4);
    // Format: #fin [N] [#entité…] — N optionnel, tags d'appartenance optionnels.
    bool sawId = false;
    while (cursor && cursor[0]) {
        const char *tokenEnd = cursor;
        while (tokenEnd[0] && !std::isspace(static_cast<unsigned char>(tokenEnd[0]))) {
            tokenEnd++;
        }
        const size_t tokenLen = static_cast<size_t>(tokenEnd - cursor);
        if (tokenLen == 0) {
            break;
        }

        if (!sawId && cursor[0] != '#') {
            bool allDigits = true;
            for (size_t i = 0; i < tokenLen; i++) {
                if (!std::isdigit(static_cast<unsigned char>(cursor[i]))) {
                    allDigits = false;
                    break;
                }
            }
            if (allDigits) {
                char numBuf[16];
                size_t copyLen = tokenLen < sizeof(numBuf) - 1 ? tokenLen : sizeof(numBuf) - 1;
                memcpy(numBuf, cursor, copyLen);
                numBuf[copyLen] = '\0';
                if (outAlertId) {
                    *outAlertId = static_cast<uint32_t>(strtoul(numBuf, nullptr, 10));
                }
                sawId = true;
                cursor = skipSpaces(tokenEnd);
                continue;
            }
        }

        if (cursor[0] == '#' && tokenLen > 1 && outAffiliation && affiliationLen > 1 && !outAffiliation[0]) {
            char tmp[SERVICE_TAG_VALUE_LEN];
            size_t nameLen = tokenLen - 1;
            if (nameLen >= sizeof(tmp)) {
                nameLen = sizeof(tmp) - 1;
            }
            memcpy(tmp, cursor + 1, nameLen);
            tmp[nameLen] = '\0';
            if (!isReservedEntityHashtag(tmp)) {
                strncpy(outAffiliation, tmp, affiliationLen - 1);
                outAffiliation[affiliationLen - 1] = '\0';
            }
        }

        cursor = skipSpaces(tokenEnd);
    }
    return true;
}

bool GaulixPagerModule::parseStatusCommand(const char *msg)
{
    msg = skipSpaces(msg);
    if (!msg) {
        return false;
    }
    return strncmp(msg, "#status", 7) == 0 && (msg[7] == '\0' || std::isspace(static_cast<unsigned char>(msg[7])));
}

bool GaulixPagerModule::parseAckCommand(const char *msg)
{
    msg = skipSpaces(msg);
    if (!msg) {
        return false;
    }
    // Local phone ACK — same effect as center / user button (acknowledgeAlert).
    return strncmp(msg, "#ack", 4) == 0 && (msg[4] == '\0' || std::isspace(static_cast<unsigned char>(msg[4])));
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

bool GaulixPagerModule::parseInfoCommand(const char *msg, const char **outText)
{
    msg = skipSpaces(msg);
    if (!msg || !outText) {
        return false;
    }

    if (strncmp(msg, "#Info", 5) != 0 && strncmp(msg, "#info", 5) != 0) {
        return false;
    }

    if (msg[5] != '\0' && !std::isspace(static_cast<unsigned char>(msg[5]))) {
        return false;
    }

    const char *text = skipSpaces(msg + 5);
    *outText = text ? text : "";
    return true;
}

void GaulixPagerModule::setServiceTagValue(uint8_t tag, const char *value)
{
    if (tag < 1 || tag > SERVICE_TAG_SLOT_COUNT) {
        return;
    }
    char *dest = configuredServiceTagValues[tag - 1];
    if (!value || !value[0]) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, value, SERVICE_TAG_VALUE_LEN - 1);
    dest[SERVICE_TAG_VALUE_LEN - 1] = '\0';
}

const char *GaulixPagerModule::getServiceTagValue(uint8_t tag)
{
    if (tag < 1 || tag > SERVICE_TAG_SLOT_COUNT) {
        return "";
    }
    return configuredServiceTagValues[tag - 1];
}

const char *GaulixPagerModule::pagerKindLabel(PagerCommandKind kind)
{
    switch (kind) {
    case PagerCommandKind::Secours:
        return "SECOURS";
    case PagerCommandKind::Vigilance:
        return "VIGILANCE";
    case PagerCommandKind::Info:
        return "INFO";
    case PagerCommandKind::Alerte:
    default:
        return "ALERTE";
    }
}

void GaulixPagerModule::formatEmitterName(NodeNum from, char *buf, size_t len)
{
    if (!buf || len == 0) {
        return;
    }
    buf[0] = '\0';
    meshtastic_NodeInfoLite *node = nodeDB ? nodeDB->getMeshNode(from) : nullptr;
    if (node && node->long_name[0]) {
        strncpy(buf, node->long_name, len - 1);
        buf[len - 1] = '\0';
        return;
    }
    if (node && node->short_name[0]) {
        strncpy(buf, node->short_name, len - 1);
        buf[len - 1] = '\0';
        return;
    }
    snprintf(buf, len, "!%08x", static_cast<unsigned int>(from));
}

bool GaulixPagerModule::parseTagValueSetCommand(const char *msg, uint8_t *outTag, char *outValue, size_t valueLen)
{
    msg = skipSpaces(msg);
    if (!msg || strncmp(msg, "#tagval", 7) != 0 ||
        (msg[7] != '\0' && !std::isspace(static_cast<unsigned char>(msg[7])))) {
        return false;
    }

    const char *arg = skipSpaces(msg + 7);
    if (!arg || !arg[0]) {
        return false;
    }

    char *end = nullptr;
    const long tagNum = strtol(arg, &end, 10);
    if (end == arg || tagNum < 1 || tagNum > SERVICE_TAG_SLOT_COUNT) {
        return false;
    }

    const char *value = skipSpaces(end);
    if (outTag) {
        *outTag = static_cast<uint8_t>(tagNum);
    }
    if (outValue && valueLen > 0) {
        if (!value || !value[0]) {
            outValue[0] = '\0';
        } else {
            strncpy(outValue, value, valueLen - 1);
            outValue[valueLen - 1] = '\0';
        }
    }
    return true;
}

bool GaulixPagerModule::parseTagSetBulkCommand(const char *msg, bool applyChanges)
{
    msg = skipSpaces(msg);
    if (!msg || strncmp(msg, "#tagset", 7) != 0 ||
        (msg[7] != '\0' && !std::isspace(static_cast<unsigned char>(msg[7])))) {
        return false;
    }

    const char *arg = skipSpaces(msg + 7);
    if (!arg || !arg[0]) {
        return false;
    }

    bool any = false;
    const char *cursor = arg;
    while (cursor && cursor[0]) {
        if (cursor[0] == ',') {
            cursor++;
            continue;
        }
        if (cursor[0] != 'T' && cursor[0] != 't') {
            return false;
        }
        char *endNum = nullptr;
        const long tagNum = strtol(cursor + 1, &endNum, 10);
        if (endNum == cursor + 1 || tagNum < 1 || tagNum > SERVICE_TAG_SLOT_COUNT || *endNum != '=') {
            return false;
        }

        const uint8_t tag = static_cast<uint8_t>(tagNum);
        const char *valueStart = endNum + 1;
        const char *valueEnd = valueStart;
        while (valueEnd[0] && valueEnd[0] != ',') {
            valueEnd++;
        }

        char value[SERVICE_TAG_VALUE_LEN];
        size_t valueLen = static_cast<size_t>(valueEnd - valueStart);
        if (valueLen >= sizeof(value)) {
            valueLen = sizeof(value) - 1;
        }
        memcpy(value, valueStart, valueLen);
        value[valueLen] = '\0';

        if (applyChanges) {
            setServiceTagValue(tag, value);
        }
        any = true;
        cursor = valueEnd[0] ? valueEnd + 1 : valueEnd;
    }

    return any;
}

bool GaulixPagerModule::isInvalidServiceTagAlert(const char *msg)
{
    msg = skipSpaces(msg);
    if (!msg || msg[0] != '#' || msg[1] != 'T') {
        return false;
    }
    char validator[SERVICE_TAG_VALUE_LEN];
    return !parseServiceTagAlert(msg, nullptr, validator, sizeof(validator), nullptr);
}

bool GaulixPagerModule::parseServiceTagAlert(const char *msg, uint8_t *outTag, char *outValidator, size_t validatorLen,
                                             const char **outText)
{
    msg = skipSpaces(msg);
    if (!msg || msg[0] != '#' || msg[1] != 'T') {
        return false;
    }

    char *end = nullptr;
    const long tagNum = strtol(msg + 2, &end, 10);
    if (end == msg + 2 || tagNum < 1 || tagNum > SERVICE_TAG_SLOT_COUNT) {
        return false;
    }
    if (*end != '\0' && !std::isspace(static_cast<unsigned char>(*end))) {
        return false;
    }

    const char *rest = skipSpaces(end);
    if (!rest || !rest[0]) {
        return false;
    }

    char validator[SERVICE_TAG_VALUE_LEN];
    size_t vi = 0;
    while (rest[vi] && !std::isspace(static_cast<unsigned char>(rest[vi])) && vi < sizeof(validator) - 1) {
        validator[vi] = rest[vi];
        vi++;
    }
    validator[vi] = '\0';
    if (vi == 0) {
        return false;
    }

    const char *text = skipSpaces(rest + vi);
    if (!text || !text[0]) {
        text = validator;
    }

    if (outTag) {
        *outTag = static_cast<uint8_t>(tagNum);
    }
    if (outValidator && validatorLen > 0) {
        strncpy(outValidator, validator, validatorLen - 1);
        outValidator[validatorLen - 1] = '\0';
    }
    if (outText) {
        *outText = text;
    }
    return true;
}

bool GaulixPagerModule::serviceTagMatches(const char *entity)
{
    if (!entity || !entity[0]) {
        return false;
    }
    for (uint8_t tag = 1; tag <= SERVICE_TAG_SLOT_COUNT; tag++) {
        const char *configured = getServiceTagValue(tag);
        if (!configured[0]) {
            continue;
        }
        if (strcasecmp(configured, GAULIX_DEFAULT_SERVICE_TAG_T1) == 0) {
            return true;
        }
        if (strcasecmp(entity, configured) == 0) {
            return true;
        }
    }
    return false;
}

bool GaulixPagerModule::isReservedEntityHashtag(const char *name)
{
    if (!name || !name[0]) {
        return true;
    }
    static const char *const kReserved[] = {
        "alerte", "secours", "info", "vigilance", "fin", "b", "code", "status", "ack", "tagval", "tagset", "tag",
        "T1",     "T2",      "T3",   "T4",        "T5",  "T6", "T7",   "T8",     "T9",     "T10"};
    for (const char *r : kReserved) {
        if (strcasecmp(name, r) == 0) {
            return true;
        }
    }
    return false;
}

bool GaulixPagerModule::entityTagsMatchMembership(const char entities[][SERVICE_TAG_VALUE_LEN], size_t entityCount)
{
    // Aucun tag entité → broadcast : tous les Bippers réagissent.
    if (entityCount == 0) {
        return true;
    }
    for (size_t i = 0; i < entityCount; i++) {
        if (serviceTagMatches(entities[i])) {
            return true;
        }
    }
    return false;
}

bool GaulixPagerModule::parseAlertCommandWithEntities(const char *msg, PagerCommandKind *outKind, uint32_t *outAlertId,
                                                      char *outText, size_t textLen, char entities[][SERVICE_TAG_VALUE_LEN],
                                                      size_t maxEntities, size_t *outEntityCount)
{
    msg = skipSpaces(msg);
    if (!msg || !outKind || !outText || textLen == 0 || !outEntityCount) {
        return false;
    }
    if (outAlertId) {
        *outAlertId = 0;
    }

    size_t cmdLen = 0;
    if (strncmp(msg, "#alerte", 7) == 0) {
        *outKind = PagerCommandKind::Alerte;
        cmdLen = 7;
    } else if (strncmp(msg, "#secours", 8) == 0) {
        *outKind = PagerCommandKind::Secours;
        cmdLen = 8;
    } else if (strncmp(msg, "#vigilance", 10) == 0) {
        *outKind = PagerCommandKind::Vigilance;
        cmdLen = 10;
    } else if (strncmp(msg, "#info", 5) == 0 || strncmp(msg, "#Info", 5) == 0) {
        *outKind = PagerCommandKind::Info;
        cmdLen = 5;
    } else {
        return false;
    }

    if (msg[cmdLen] != '\0' && !std::isspace(static_cast<unsigned char>(msg[cmdLen]))) {
        return false;
    }

    outText[0] = '\0';
    *outEntityCount = 0;

    // Format: #cmd [N] <texte...> [#entité…]
    const char *tokens[48];
    size_t tokenLens[48];
    size_t tokenCount = 0;
    const char *cursor = skipSpaces(msg + cmdLen);
    while (cursor && cursor[0] && tokenCount < 48) {
        const char *tokenEnd = cursor;
        while (tokenEnd[0] && !std::isspace(static_cast<unsigned char>(tokenEnd[0]))) {
            tokenEnd++;
        }
        const size_t tokenLen = static_cast<size_t>(tokenEnd - cursor);
        if (tokenLen == 0) {
            break;
        }
        tokens[tokenCount] = cursor;
        tokenLens[tokenCount] = tokenLen;
        tokenCount++;
        cursor = skipSpaces(tokenEnd);
    }

    size_t textStart = 0;
    if (tokenCount > 0 && tokens[0][0] != '#') {
        bool allDigits = true;
        for (size_t i = 0; i < tokenLens[0]; i++) {
            if (!std::isdigit(static_cast<unsigned char>(tokens[0][i]))) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            char numBuf[16];
            size_t copyLen = tokenLens[0] < sizeof(numBuf) - 1 ? tokenLens[0] : sizeof(numBuf) - 1;
            memcpy(numBuf, tokens[0], copyLen);
            numBuf[copyLen] = '\0';
            if (outAlertId) {
                *outAlertId = static_cast<uint32_t>(strtoul(numBuf, nullptr, 10));
            }
            textStart = 1;
        }
    }

    size_t textEnd = tokenCount;
    while (textEnd > textStart && tokens[textEnd - 1][0] == '#' && tokenLens[textEnd - 1] > 1) {
        char name[SERVICE_TAG_VALUE_LEN];
        size_t nameLen = tokenLens[textEnd - 1] - 1;
        if (nameLen >= sizeof(name)) {
            nameLen = sizeof(name) - 1;
        }
        memcpy(name, tokens[textEnd - 1] + 1, nameLen);
        name[nameLen] = '\0';
        if (isReservedEntityHashtag(name)) {
            break;
        }
        if (entities && maxEntities > 0 && *outEntityCount < maxEntities) {
            // Insérer en tête pour conserver l'ordre d'apparition après reverse scan.
            for (size_t i = *outEntityCount; i > 0; i--) {
                memcpy(entities[i], entities[i - 1], SERVICE_TAG_VALUE_LEN);
            }
            strncpy(entities[0], name, SERVICE_TAG_VALUE_LEN - 1);
            entities[0][SERVICE_TAG_VALUE_LEN - 1] = '\0';
            (*outEntityCount)++;
        }
        textEnd--;
    }

    size_t textOffset = 0;
    for (size_t i = textStart; i < textEnd; i++) {
        if (tokens[i][0] == '#') {
            continue;
        }
        if (textOffset > 0 && textOffset + 1 < textLen) {
            outText[textOffset++] = ' ';
        }
        const size_t copyLen =
            (tokenLens[i] < textLen - textOffset - 1) ? tokenLens[i] : (textLen - textOffset - 1);
        if (copyLen > 0) {
            memcpy(outText + textOffset, tokens[i], copyLen);
            textOffset += copyLen;
            outText[textOffset] = '\0';
        }
    }

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

void GaulixPagerModule::playPimPoms(uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        playGaulixPagerPimPom();
        if (i + 1 < count) {
            delay(GAULIX_BUZZER_PULSE_GAP_MS);
        }
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
    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    const int16_t lineH = FONT_HEIGHT_SMALL;
    const int16_t maxW = display->getWidth() - 4;
    int16_t ly = y;

    // L1 — type d'alerte (+ nº)
    char line1[48];
    if (activeAlertId != 0) {
        snprintf(line1, sizeof(line1), "%s #%lu", pagerKindLabel(activeAlertKind),
                 static_cast<unsigned long>(activeAlertId));
    } else {
        snprintf(line1, sizeof(line1), "%s", pagerKindLabel(activeAlertKind));
    }
    display->drawString(x + 2, ly, line1);
    ly += lineH;

    // L2 — texte
    if (alertText[0]) {
        display->drawStringMaxWidth(x + 2, ly, maxW, alertText);
    }
    ly += lineH;

    // L3 — émetteur
    if (alertEmitterName[0]) {
        display->drawStringMaxWidth(x + 2, ly, maxW, alertEmitterName);
    }
    ly += lineH;

    // L4 — réservé
    ly += lineH;

    // L5 — date / heure de réception
    if (lastAlertTime != 0) {
        time_t t = lastAlertTime;
        struct tm *tmInfo = localtime(&t);
        if (tmInfo) {
            char timeBuf[24];
            strftime(timeBuf, sizeof(timeBuf), "%d/%m/%Y %H:%M", tmInfo);
            display->drawString(x + 2, ly, timeBuf);
        }
    }
    ly += lineH;

    // L6 — confirmation lecture
    display->drawStringMaxWidth(x + 2, ly, maxW, "Appuyer pour confirmer lecture");
}

void GaulixPagerModule::showAlertScreen()
{
    if (!screen) {
        return;
    }
    screen->startAlert(drawAlertFrame);
}

void GaulixPagerModule::triggerAlert(const char *text, const meshtastic_MeshPacket &mp, PagerCommandKind kind,
                                     uint32_t alertId)
{
    memset(alertText, 0, sizeof(alertText));
    if (text && text[0]) {
        strncpy(alertText, text, sizeof(alertText) - 1);
    } else {
        strncpy(alertText, pagerKindLabel(kind), sizeof(alertText) - 1);
    }

    activeAlertKind = kind;
    activeAlertId = alertId;
    formatEmitterName(getFrom(&mp), alertEmitterName, sizeof(alertEmitterName));

    alertSourceNode = mp.from;
    alertSourceChannel = mp.channel;
    alertSourcePacket = mp;
    hasAlertSourcePacket = true;

    // Remplacement d'une alerte encore ouverte : éviter Nb AL. fantôme.
    if (alertActive) {
        unrecordAlert();
    }

    recordAlert();
    addAlertHistoryEntry(alertText, mp);
    alertActive = true;
    ledBlinkState = false;
    alertStartedMs = millis();
    lastContinuousBeepMs = alertStartedMs;

    const char *via = isBroadcast(mp.to) ? "broadcast canal Alerte" : "DM bipper";
    LOG_INFO("GaulixPager: alerte #%lu '%s' via %s de 0x%08x ch %u", static_cast<unsigned long>(alertId), alertText, via,
             alertSourceNode, alertSourceChannel);

    prepareBuzzerForAlert();
    playGaulixPagerPimPom();
    showAlertScreen();

    const int8_t ledPin = alertLedPin();
    if (ledPin >= 0) {
        pinMode(ledPin, OUTPUT);
        digitalWrite(ledPin, HIGH);
    }

    scheduleAlertMaintenance();
}

void GaulixPagerModule::triggerInfo(const char *text, const meshtastic_MeshPacket &mp)
{
    const char *display = (text && text[0]) ? text : "Info";
    LOG_INFO("GaulixPager: info '%s' de 0x%08x", display, getFrom(&mp));
    addAlertHistoryEntry(display, mp, true);
    prepareBuzzerForAlert();
    playPimPoms(INFO_PIM_POM_COUNT);
}

void GaulixPagerModule::sendReplyDm(const meshtastic_MeshPacket &rx, const char *text)
{
    const NodeNum dest = getFrom(&rx);
    if (!text || !dest || dest == NODENUM_BROADCAST) {
        LOG_WARN("GaulixPager: pas de destinataire pour reponse DM");
        return;
    }

    meshtastic_MeshPacket *p = allocDataPacket();
    setReplyTo(p, rx);
    p->want_ack = false;
    p->decoded.want_response = false;

    meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(dest);
    const NodeNum myNodeNum = nodeDB->getNodeNum();
    if (node && node->num != myNodeNum && nodeInfoLiteHasUser(node) && node->public_key.size == 32) {
        p->pki_encrypted = true;
        p->channel = 0;
    }

    const size_t len = strnlen(text, sizeof(p->decoded.payload.bytes));
    p->decoded.payload.size = len;
    memcpy(p->decoded.payload.bytes, text, len);

    service->sendToMesh(p, RX_SRC_LOCAL, true);
    LOG_INFO("GaulixPager: DM vers 0x%08x ch %u (source ch %u)", dest, p->channel, rx.channel);
}

void GaulixPagerModule::sendAckDm()
{
    char timeBuf[20] = "--/-- --:--";
    time_t t = getTime();
    struct tm *tmInfo = localtime(&t);
    if (tmInfo) {
        strftime(timeBuf, sizeof(timeBuf), "%d/%m %H:%M", tmInfo);
    }

    char reply[200];
    int replyLen;
    if (activeAlertId != 0) {
        replyLen = snprintf(reply, sizeof(reply), "Pager ACK alerte #%lu %s", static_cast<unsigned long>(activeAlertId),
                            timeBuf);
    } else {
        replyLen = snprintf(reply, sizeof(reply), "Pager ACK alerte %s", timeBuf);
    }
    if (replyLen < 0) {
        return;
    }
    if (alertText[0]) {
        snprintf(reply + replyLen, sizeof(reply) - replyLen, " — %s", alertText);
    }
    replyLen = strnlen(reply, sizeof(reply));

#if !MESHTASTIC_EXCLUDE_GPS
    if (config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED) {
        int32_t latI = localPosition.latitude_i;
        int32_t lonI = localPosition.longitude_i;
        if (latI == 0 && lonI == 0 && gpsStatus) {
            latI = gpsStatus->getLatitude();
            lonI = gpsStatus->getLongitude();
        }
        if (latI != 0 || lonI != 0) {
            const double lat = latI * 1e-7;
            const double lon = lonI * 1e-7;
            snprintf(reply + replyLen, sizeof(reply) - replyLen, " | %.5f%c %.5f%c", fabs(lat), lat >= 0 ? 'N' : 'S',
                     fabs(lon), lon >= 0 ? 'E' : 'W');
            replyLen = strnlen(reply, sizeof(reply));
        }
    }
#endif

    // 1) Broadcast canal Alerte — visible des coordinateurs / Gestion des alertes.
    const int alerteCh = findAlerteChannelIndex();
    if (alerteCh >= 0) {
        meshtastic_MeshPacket *p = allocDataPacket();
        if (p) {
            p->to = NODENUM_BROADCAST;
            p->channel = static_cast<uint8_t>(alerteCh);
            p->want_ack = false;
            p->decoded.want_response = false;
            p->priority = meshtastic_MeshPacket_Priority_RELIABLE;
            const size_t len = strnlen(reply, sizeof(p->decoded.payload.bytes));
            p->decoded.payload.size = len;
            memcpy(p->decoded.payload.bytes, reply, len);
            service->sendToMesh(p, RX_SRC_LOCAL, true);
            LOG_INFO("GaulixPager: ACK lecture sur canal Alerte ch %d", alerteCh);
        }
    }

    // 2) DM PKI vers le lanceur — remontée fiable (clients masquent Pager ACK des chats).
    if (alertSourceNode && alertSourceNode != NODENUM_BROADCAST) {
        if (hasAlertSourcePacket) {
            sendReplyDm(alertSourcePacket, reply);
        } else {
            meshtastic_MeshPacket ackTarget = meshtastic_MeshPacket_init_zero;
            ackTarget.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
            ackTarget.from = alertSourceNode;
            ackTarget.channel = alertSourceChannel;
            sendReplyDm(ackTarget, reply);
        }
        LOG_INFO("GaulixPager: ACK DM vers lanceur 0x%08x", alertSourceNode);
    } else if (alerteCh < 0) {
        LOG_WARN("GaulixPager: ACK ignore — ni canal Alerte ni source");
    }
}

void GaulixPagerModule::sendAckPositionOnBalise()
{
#if MESHTASTIC_EXCLUDE_GPS
    return;
#else
    if (!positionModule) {
        return;
    }
    if (config.position.gps_mode != meshtastic_Config_PositionConfig_GpsMode_ENABLED) {
        LOG_DEBUG("GaulixPager: position ACK ignoree — GPS desactive");
        return;
    }

    service->refreshLocalMeshNode();

#if HAS_GPS
    if (gps && gpsStatus && gpsStatus->getHasLock()) {
        meshtastic_Position pos = gps->p;
        pos.time = getValidTime(RTCQualityFromNet);
        nodeDB->updatePosition(nodeDB->getNodeNum(), pos, RX_SRC_LOCAL);
    }
#endif

    int32_t latI = localPosition.latitude_i;
    int32_t lonI = localPosition.longitude_i;
    if (latI == 0 && lonI == 0 && gpsStatus) {
        latI = gpsStatus->getLatitude();
        lonI = gpsStatus->getLongitude();
        if (latI != 0 || lonI != 0) {
            meshtastic_Position pos = meshtastic_Position_init_default;
            pos.latitude_i = latI;
            pos.longitude_i = lonI;
            pos.has_latitude_i = true;
            pos.has_longitude_i = true;
            pos.time = getValidTime(RTCQualityFromNet);
            nodeDB->updatePosition(nodeDB->getNodeNum(), pos, RX_SRC_LOCAL);
        }
    }

    if (localPosition.latitude_i == 0 && localPosition.longitude_i == 0) {
        LOG_WARN("GaulixPager: position ACK ignoree — pas de fix GPS");
        return;
    }

    const int baliseCh = findBaliseChannelIndex();
    if (baliseCh < 0) {
        LOG_WARN("GaulixPager: canal Fr_Balise introuvable");
        return;
    }
    if (getPositionPrecisionForChannel(static_cast<uint8_t>(baliseCh)) == 0) {
        LOG_WARN("GaulixPager: Fr_Balise sans precision position");
        return;
    }

    LOG_INFO("GaulixPager: broadcast position ACK sur Fr_Balise ch %d", baliseCh);
    positionModule->sendOurPosition(NODENUM_BROADCAST, false, static_cast<uint8_t>(baliseCh));
#endif
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

    char reply[400];
    char tagLine[SERVICE_TAG_LINE_LEN];
    formatServiceTagLine(tagLine, sizeof(tagLine));
    snprintf(reply, sizeof(reply), "Pager Gaulix - %s | Alertes: %lu | %s | Bips: %s | %s | Code: %s",
             alertActive ? "En alerte" : "En ecoute", static_cast<unsigned long>(alertCount), batteryLine, bipsLine, tagLine,
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
    sendAckPositionOnBalise();
    markCurrentAlertHistoryAcknowledged();
    clearAlert(false);
    playGaulixPagerFinBeep();
}

void GaulixPagerModule::clearAlert(bool playFinMelody)
{
    if (!alertActive && !playFinMelody) {
        return;
    }

    // #fin sans alerte active : ignorer (évite bip + décompte fantôme).
    if (!alertActive && playFinMelody) {
        LOG_DEBUG("GaulixPager: #fin ignore (aucune alerte active)");
        return;
    }

    LOG_INFO("GaulixPager: fin d'alerte");

    if (alertActive) {
        if (playFinMelody) {
            markCurrentAlertHistoryClosedByFin();
        } else {
            markCurrentAlertHistoryTimedOut();
        }
        // Nb AL. = alertes non clôturées ; -1 saturé à 0 (ACK, #fin ou timeout).
        unrecordAlert();
    }

    alertActive = false;
    activeAlertId = 0;
    alertEmitterName[0] = '\0';
    activeAlertKind = PagerCommandKind::Alerte;
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

    uint32_t finAlertId = 0;
    char finAffiliation[SERVICE_TAG_VALUE_LEN];
    if (parseFinCommandEx(buf, &finAlertId, finAffiliation, sizeof(finAffiliation))) {
        if (finAffiliation[0]) {
            char entities[1][SERVICE_TAG_VALUE_LEN];
            strncpy(entities[0], finAffiliation, SERVICE_TAG_VALUE_LEN - 1);
            entities[0][SERVICE_TAG_VALUE_LEN - 1] = '\0';
            if (!entityTagsMatchMembership(entities, 1)) {
                LOG_DEBUG("GaulixPager: #fin ignore (appartenance %s)", finAffiliation);
                return ProcessMessage::CONTINUE;
            }
        }
        // #fin N : ne clôture que si le nº actif correspond (ou #fin sans Nº = toutes).
        if (finAlertId != 0) {
            if (!alertActive || activeAlertId == 0 || activeAlertId != finAlertId) {
                LOG_DEBUG("GaulixPager: #fin %lu ignore (actif #%lu)", static_cast<unsigned long>(finAlertId),
                          static_cast<unsigned long>(activeAlertId));
                return ProcessMessage::CONTINUE;
            }
        }
        clearAlert(true);
        return ProcessMessage::STOP;
    }

    if (parseStatusCommand(buf)) {
        sendStatusReply(mp);
        return ProcessMessage::STOP;
    }

    if (parseAckCommand(buf)) {
        // Phone "J'ai pris connaissance" → same path as physical button.
        if (alertActive) {
            acknowledgeAlert();
            LOG_INFO("GaulixPager: #ack local — simule appui bouton");
        } else {
            LOG_DEBUG("GaulixPager: #ack ignore (aucune alerte active)");
        }
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

    PagerCommandKind kind = PagerCommandKind::Alerte;
    uint32_t alertId = 0;
    char alertBody[96];
    char entityTags[MAX_ALERT_ENTITIES][SERVICE_TAG_VALUE_LEN];
    size_t entityCount = 0;
    if (parseAlertCommandWithEntities(buf, &kind, &alertId, alertBody, sizeof(alertBody), entityTags, MAX_ALERT_ENTITIES,
                                      &entityCount)) {
        if (!entityTagsMatchMembership(entityTags, entityCount)) {
            LOG_DEBUG("GaulixPager: ignore (aucune entité locale parmi les tags)");
            return ProcessMessage::CONTINUE;
        }
        if (kind == PagerCommandKind::Info) {
            triggerInfo(alertBody, mp);
        } else {
            const char *fallback = pagerKindLabel(kind);
            triggerAlert(alertBody[0] ? alertBody : fallback, mp, kind, alertId);
        }
        // STOP would swallow the packet before TextMessageModule — keep chat history too.
        return ProcessMessage::CONTINUE;
    }

    if (parseTagSetBulkCommand(buf, true)) {
        if (!saveConfig()) {
            LOG_WARN("GaulixPager: echec sauvegarde tags");
        }
        char tagLine[SERVICE_TAG_LINE_LEN];
        formatServiceTagLine(tagLine, sizeof(tagLine));
        char reply[192];
        snprintf(reply, sizeof(reply), "Pager OK — %s", tagLine);
        sendReplyDm(mp, reply);
        return ProcessMessage::STOP;
    }

    uint8_t tagNum = 0;
    char tagValue[SERVICE_TAG_VALUE_LEN];
    if (parseTagValueSetCommand(buf, &tagNum, tagValue, sizeof(tagValue))) {
        setServiceTagValue(tagNum, tagValue);
        if (!saveConfig()) {
            LOG_WARN("GaulixPager: echec sauvegarde tag T%u", tagNum);
        }
        char tagLine[SERVICE_TAG_LINE_LEN];
        formatServiceTagLine(tagLine, sizeof(tagLine));
        char reply[192];
        snprintf(reply, sizeof(reply), "Pager OK — %s", tagLine);
        sendReplyDm(mp, reply);
        return ProcessMessage::STOP;
    }

    uint8_t serviceTag = 0;
    char validator[SERVICE_TAG_VALUE_LEN];
    const char *tagAlertText = nullptr;
    if (parseServiceTagAlert(buf, &serviceTag, validator, sizeof(validator), &tagAlertText)) {
        // Membership is by entity name across any slot (T1–T10), not by alert slot index.
        if (!serviceTagMatches(validator)) {
            LOG_DEBUG("GaulixPager: #T%u %s ignore (pas membre de cette entite)", serviceTag, validator);
            return ProcessMessage::CONTINUE;
        }
        triggerAlert(tagAlertText, mp, PagerCommandKind::Alerte, 0);
        return ProcessMessage::CONTINUE;
    }

    if (isInvalidServiceTagAlert(buf)) {
        LOG_DEBUG("GaulixPager: tag T inconnu ou invalide, ignore");
        return ProcessMessage::CONTINUE;
    }

    // Not a pager command — do NOT STOP: GaulixPager is registered before TextMessageModule,
    // so STOP made channel/DM chat from PC crise (M2) invisible on the bipper.
    LOG_DEBUG("GaulixPager: message non reconnu, passe au chat");
    return ProcessMessage::CONTINUE;
}

int GaulixPagerModule::handleInputEvent(const InputEvent *event)
{
    if (!alertActive || !event) {
        return 0;
    }

    // Center click (short or long) / user button — ignore stick directions.
    if (event->inputEvent == INPUT_BROKER_SELECT || event->inputEvent == INPUT_BROKER_SELECT_LONG ||
        event->inputEvent == INPUT_BROKER_USER_PRESS) {
        acknowledgeAlert();
        return 1;
    }

    return 0;
}

int32_t GaulixPagerModule::maintainBatteryWarning()
{
    if (!powerStatus || !powerStatus->getHasBattery() || powerStatus->getIsCharging() || powerStatus->getHasUSB()) {
        lowBatteryWarningActive = false;
        return LOW_BATTERY_CHECK_MS;
    }

    const uint8_t pct = powerStatus->getBatteryChargePercent();
    if (pct == 0 || pct > LOW_BATTERY_THRESHOLD_PCT) {
        lowBatteryWarningActive = false;
        return LOW_BATTERY_CHECK_MS;
    }

    const bool firstEntry = !lowBatteryWarningActive;
    const bool repeatDue = !Throttle::isWithinTimespanMs(lastLowBatteryBeepMs, LOW_BATTERY_REPEAT_MS);
    if (firstEntry || repeatDue) {
        lowBatteryWarningActive = true;
        lastLowBatteryBeepMs = millis();
        playGaulixLowBatteryBeep();
        LOG_INFO("GaulixPager: batterie faible (%u%%)", pct);
    }

    return LOW_BATTERY_CHECK_MS;
}

int32_t GaulixPagerModule::runOnce()
{
    const int8_t ledPin = alertLedPin();

    if (!alertActive) {
        if (ledPin >= 0) {
            digitalWrite(ledPin, LOW);
        }
        return maintainBatteryWarning();
    }

    if (!Throttle::isWithinTimespanMs(alertStartedMs, ALERT_MAX_DURATION_MS)) {
        LOG_INFO("GaulixPager: alerte expiree apres 30 min sans acquittement");
        clearAlert(false);
        return LOW_BATTERY_CHECK_MS;
    }

    static uint32_t lastAlertScreenRefreshMs = 0;
    if (!Throttle::isWithinTimespanMs(lastAlertScreenRefreshMs, 3000)) {
        lastAlertScreenRefreshMs = millis();
        showAlertScreen();
    }

    if (!Throttle::isWithinTimespanMs(lastContinuousBeepMs, CONTINUOUS_BEEP_INTERVAL_MS)) {
        lastContinuousBeepMs = millis();
        prepareBuzzerForAlert();
        playGaulixPagerPimPom();
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

void GaulixPagerModule::formatServiceTagLine(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return;
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, len - offset, "Tag: ");
    bool first = true;
    bool any = false;
    for (uint8_t tag = 1; tag <= SERVICE_TAG_SLOT_COUNT; tag++) {
        const char *value = getServiceTagValue(tag);
        if (!value[0]) {
            continue;
        }
        any = true;
        offset += snprintf(buf + offset, len - offset, "%sT%u=%s", first ? "" : ",", tag, value);
        first = false;
        if (offset >= len) {
            break;
        }
    }
    if (!any) {
        snprintf(buf, len, "Tag: aucun");
    }
}

static bool gaulixOwnerNameFitsOnLine(OLEDDisplay *display)
{
    if (!owner.long_name[0] || !display) {
        return false;
    }
    display->setFont(FONT_SMALL);
    return display->getStringWidth(owner.long_name) <= display->getWidth() - 4;
}

void GaulixPagerModule::formatStatusLine(char *buf, size_t len, OLEDDisplay *display)
{
    if (!owner.long_name[0]) {
        snprintf(buf, len, "%s", alertActive ? "En alerte" : "En \u00e9coute");
        return;
    }
    if (display && !gaulixOwnerNameFitsOnLine(display)) {
        snprintf(buf, len, "Nom long : !!!trop long");
        return;
    }
    snprintf(buf, len, "%s", owner.long_name);
}

void GaulixPagerModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    if (state) {
        pagerFrameIndex = state->currentFrame;
    }

    display->clear();

    display->setFont(FONT_SMALL);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    const int16_t lineStep = FONT_HEIGHT_SMALL + 2;
    int16_t lineY = y + 2;
    char lineBuf[48];

    // Ligne 1 : nom long du Bipper (ou avertissement si trop large)
    if (!owner.long_name[0]) {
        display->drawString(x + 2, lineY, "En \u00e9coute");
    } else if (gaulixOwnerNameFitsOnLine(display)) {
        display->drawString(x + 2, lineY, owner.long_name);
    } else {
        display->drawString(x + 2, lineY, "Nom long : !!!trop long");
    }
    lineY += lineStep;

    // Ligne 2 : nombre d'alertes | dernière alerte
    char timeBuf[8] = "--:--";
    if (lastAlertTime != 0) {
        time_t t = lastAlertTime;
        struct tm *tmInfo = localtime(&t);
        if (tmInfo) {
            strftime(timeBuf, sizeof(timeBuf), "%H:%M", tmInfo);
        }
    }
    // uint32 saturé via unrecordAlert() — affichage toujours >= 0.
    snprintf(lineBuf, sizeof(lineBuf), "Nb AL. : %u | Der. : %s", static_cast<unsigned>(alertCount), timeBuf);
    display->drawStringMaxWidth(x + 2, lineY, display->getWidth() - 4, lineBuf);
    lineY += lineStep;

    // Ligne 3 : Bipper Gaulix + version
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(x + display->getWidth() / 2, lineY, GAULIX_PAGER_TITLE);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    lineY += lineStep;

    // Ligne 4 : batterie + acces historique
    formatBatteryLine(lineBuf, sizeof(lineBuf));
    display->drawString(x + 2, lineY, lineBuf);
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(x + display->getWidth() - 2, lineY, "Trackball >");
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

#endif
