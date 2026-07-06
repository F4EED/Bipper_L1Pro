#pragma once

#include "configuration.h"

#if defined(GAULIX_PAGER) && HAS_SCREEN

#include "MeshModule.h"

#define GAULIX_PAGER_VERSION "v1.1"
#define GAULIX_PAGER_TITLE "Bipper Gaulix " GAULIX_PAGER_VERSION

class GaulixPagerModule : public MeshModule
{
  public:
    GaulixPagerModule();

    static uint32_t getAlertCount() { return alertCount; }
    static uint32_t getLastAlertTime() { return lastAlertTime; }
    static void recordAlert();

  protected:
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override { return false; }
    virtual bool wantUIFrame() override { return true; }
    virtual void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;

  private:
    static uint32_t alertCount;
    static uint32_t lastAlertTime;

    static void formatBatteryLine(char *buf, size_t len);
    static void formatLastAlertLine(char *buf, size_t len);
};

extern GaulixPagerModule *gaulixPagerModule;

#endif
