#pragma once

#include "configuration.h"

#if defined(GAULIX_PAGER) && HAS_SCREEN

#include "Observer.h"
#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "input/InputBroker.h"
#include "mesh/MeshTypes.h"

#define GAULIX_PAGER_VERSION "v1.2"
#define GAULIX_PAGER_TITLE "Bipper Gaulix " GAULIX_PAGER_VERSION
#define GAULIX_DEFAULT_ACTIVATION_CODE "GAULIX"

class GaulixPagerModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    GaulixPagerModule();

    static uint32_t getAlertCount() { return alertCount; }
    static uint32_t getLastAlertTime() { return lastAlertTime; }
    static bool isAlertActive() { return alertActive; }

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
    static constexpr uint32_t LED_BLINK_MS = 300;
    static constexpr uint32_t CONTINUOUS_BEEP_INTERVAL_MS = 3000;
    static constexpr uint32_t ALERT_TIMEOUT_MS = 30UL * 60UL * 1000UL;

    static uint32_t alertCount;
    static uint32_t lastAlertTime;
    static bool alertActive;
    static char alertText[96];
    static NodeNum alertSourceNode;
    static uint8_t alertSourceChannel;
    static char activationCode[32];
    static uint8_t configuredBeepCount;
    static uint32_t alertStartedMs;
    static uint32_t lastContinuousBeepMs;

    static bool ledBlinkState;

    static const char *skipSpaces(const char *msg);
    static bool parseFinCommand(const char *msg);
    static bool parseStatusCommand(const char *msg);
    static bool parseBeepCommand(const char *msg, uint8_t *outCount);
    static bool parseCodeCommand(const char *msg, char *oldCode, size_t oldLen, char *newCode, size_t newLen);
    static bool parseAlertCommand(const char *msg, const char *keyword, char *code, size_t codeLen, const char **outText);

    static bool isAcceptedPacket(const meshtastic_MeshPacket &mp);
    static int findAlerteChannelIndex();
    static bool activationCodeMatches(const char *code);

    void loadConfig();
    bool saveConfig();

    void triggerAlert(const char *text, const meshtastic_MeshPacket &mp);
    void acknowledgeAlert();
    void clearAlert(bool playFinMelody);
    void sendReplyDm(const meshtastic_MeshPacket &rx, const char *text);
    void sendAckDm();
    void sendStatusReply(const meshtastic_MeshPacket &mp);
    bool handleCodeCommand(const char *msg, const meshtastic_MeshPacket &mp);
    void showAlertScreen();
    static void drawAlertFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

    int handleInputEvent(const InputEvent *event);

    static void playBeeps(uint8_t count);
    static void playFinBeeps();

    static void formatBatteryLine(char *buf, size_t len);
    static void formatLastAlertLine(char *buf, size_t len);
    static void recordAlert();
};

extern GaulixPagerModule *gaulixPagerModule;

#endif
