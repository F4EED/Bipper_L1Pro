#pragma once

#include "configuration.h"

#if defined(GAULIX_PAGER) && HAS_SCREEN

#include "Observer.h"
#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "input/InputBroker.h"
#include "mesh/MeshTypes.h"

#define GAULIX_PAGER_VERSION "v1.10.0"
#define GAULIX_PAGER_TITLE "Bipper Gaulix " GAULIX_PAGER_VERSION
#define GAULIX_DEFAULT_ACTIVATION_CODE "GAULIX"
#define GAULIX_DEFAULT_SERVICE_TAG_T1 "all"

// Buzzer tuning constants live in buzz.h (GAULIX_BUZZER_*).
#include "buzz.h"

class GaulixPagerModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    GaulixPagerModule();

    static uint32_t getAlertCount() { return alertCount; }
    static uint32_t getLastAlertTime() { return lastAlertTime; }
    static bool isAlertActive() { return alertActive; }
    static uint8_t getPagerFrameIndex() { return pagerFrameIndex; }
    void userAcknowledgeAlert() { acknowledgeAlert(); }

    struct AlertHistoryEntry {
        uint32_t time = 0;
        NodeNum from = 0;
        bool viaDm = false;
        bool isInfo = false;
        bool acknowledged = false;
        bool timedOut = false;
        char text[64] = {};
    };

    static constexpr size_t ALERT_HISTORY_MAX = 20;
    static size_t getAlertHistoryCount();
    static bool getAlertHistoryEntry(size_t index, AlertHistoryEntry &out);
    static size_t getAlertHistoryScrollIndex();
    static void scrollAlertHistory(int delta);

  protected:
    virtual bool wantUIFrame() override { return true; }
    virtual bool interceptingKeyboardInput() override { return alertActive; }
    virtual void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual int32_t runOnce() override;

  private:
    CallbackObserver<GaulixPagerModule, const InputEvent *> inputObserver =
        CallbackObserver<GaulixPagerModule, const InputEvent *>(this, &GaulixPagerModule::handleInputEvent);

    static constexpr uint8_t DEFAULT_BEEP_COUNT = 3;
    static constexpr uint8_t INFO_PIM_POM_COUNT = 3;
    static constexpr size_t SERVICE_TAG_VALUE_LEN = 24;
    static constexpr size_t SERVICE_TAG_LINE_LEN = 128;
    static constexpr size_t MAX_ALERT_ENTITIES = 8;
    static constexpr uint32_t LED_BLINK_MS = 300;
    static constexpr uint32_t ALERT_MAX_DURATION_MS = 30UL * 60UL * 1000UL;
    static constexpr uint32_t CONTINUOUS_BEEP_INTERVAL_MS = GAULIX_CONTINUOUS_BEEP_INTERVAL_MS;
    static constexpr uint32_t LOW_BATTERY_CHECK_MS = GAULIX_LOW_BATTERY_CHECK_MS;
    static constexpr uint32_t LOW_BATTERY_REPEAT_MS = GAULIX_LOW_BATTERY_REPEAT_MS;
    static constexpr uint8_t LOW_BATTERY_THRESHOLD_PCT = 10;

    static uint32_t lastLowBatteryBeepMs;
    static bool lowBatteryWarningActive;

    static int32_t maintainBatteryWarning();
    static constexpr size_t ALERT_PACKET_DEDUP_SIZE = 32;

    struct SeenPacket {
        NodeNum from = 0;
        uint32_t id = 0;
    };

    static SeenPacket seenPackets[ALERT_PACKET_DEDUP_SIZE];
    static size_t seenPacketIndex;

    static uint32_t alertCount;
    static uint32_t lastAlertTime;
    static bool alertActive;
    static char alertText[96];
    static NodeNum alertSourceNode;
    static uint8_t alertSourceChannel;
    static meshtastic_MeshPacket alertSourcePacket;
    static bool hasAlertSourcePacket;
    static char activationCode[32];
    static uint8_t configuredBeepCount;
    static char configuredServiceTagValues[4][SERVICE_TAG_VALUE_LEN];
    static uint32_t alertStartedMs;
    static uint32_t lastContinuousBeepMs;

    static uint8_t pagerFrameIndex;

    static bool ledBlinkState;

    static AlertHistoryEntry alertHistory[ALERT_HISTORY_MAX];
    static size_t alertHistoryCount;
    static size_t alertHistoryHead;
    static size_t alertHistoryScrollIndex;
    static size_t currentAlertHistoryPhysIdx;

    static const char *skipSpaces(const char *msg);
    static bool parseFinCommand(const char *msg);
    static bool parseFinCommandWithAffiliation(const char *msg, char *outAffiliation, size_t affiliationLen);
    static bool parseStatusCommand(const char *msg);
    static bool parseBeepCommand(const char *msg, uint8_t *outCount);
    static bool parseCodeCommand(const char *msg, char *oldCode, size_t oldLen, char *newCode, size_t newLen);
    static bool parseAlertWithText(const char *msg, const char *keyword, const char **outText);
    static bool parseInfoCommand(const char *msg, const char **outText);
    enum class PagerCommandKind : uint8_t { Alerte = 0, Secours = 1, Info = 2, Vigilance = 3 };

    static bool parseAlertCommandWithEntities(const char *msg, PagerCommandKind *outKind, char *outText, size_t textLen,
                                              char entities[][SERVICE_TAG_VALUE_LEN], size_t maxEntities,
                                              size_t *outEntityCount);
    static bool isReservedEntityHashtag(const char *name);
    static bool entityTagsMatchMembership(const char entities[][SERVICE_TAG_VALUE_LEN], size_t entityCount);
    static bool parseTagValueSetCommand(const char *msg, uint8_t *outTag, char *outValue, size_t valueLen);
    static bool parseTagSetBulkCommand(const char *msg, bool applyChanges);
    static bool parseServiceTagAlert(const char *msg, uint8_t *outTag, char *outValidator, size_t validatorLen,
                                     const char **outText);
    static bool isInvalidServiceTagAlert(const char *msg);
    /** True if entity name matches any configured membership slot T1–T4. */
    static bool serviceTagMatches(const char *entity);
    static void setServiceTagValue(uint8_t tag, const char *value);
    static const char *getServiceTagValue(uint8_t tag);

    static bool isAcceptedPacket(const meshtastic_MeshPacket &mp);
    static bool isLocalConfigCommand(const char *msg);
    static bool isPagerInternalReply(const char *msg);
    static void prepareBuzzerForAlert();
    void scheduleAlertMaintenance();
    static bool recentlySeenPacket(NodeNum from, uint32_t id);
    static void recordSeenPacket(NodeNum from, uint32_t id);
    static int findAlerteChannelIndex();
    static int findBaliseChannelIndex();
    static bool activationCodeMatches(const char *code);

    void loadConfig();
    bool saveConfig();
    void applyChannelMuteDefaults();
    void ensureGaulixChannelsInstalled();

    void triggerAlert(const char *text, const meshtastic_MeshPacket &mp);
    void triggerInfo(const char *text, const meshtastic_MeshPacket &mp);
    void acknowledgeAlert();
    void clearAlert(bool playFinMelody);
    void sendReplyDm(const meshtastic_MeshPacket &rx, const char *text);
    void sendAckDm();
    void sendAckPositionOnBalise();
    void sendStatusReply(const meshtastic_MeshPacket &mp);
    bool handleCodeCommand(const char *msg, const meshtastic_MeshPacket &mp);
    void showAlertScreen();
    static void drawAlertFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

    int handleInputEvent(const InputEvent *event);

    static void playPimPoms(uint8_t count);
    static void playFinBeeps();
    static void ensureBuzzerReady();
    static int8_t alertLedPin();

    static void formatBatteryLine(char *buf, size_t len);
    static void formatLastAlertLine(char *buf, size_t len);
    static void formatServiceTagLine(char *buf, size_t len);
    static void formatStatusLine(char *buf, size_t len, OLEDDisplay *display);
    static void recordAlert();
    static void addAlertHistoryEntry(const char *text, const meshtastic_MeshPacket &mp, bool isInfo = false);
    static void markCurrentAlertHistoryAcknowledged();
    static void markCurrentAlertHistoryTimedOut();
};

extern GaulixPagerModule *gaulixPagerModule;

#endif
