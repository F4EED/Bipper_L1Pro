#pragma once

#include "configuration.h"

#if defined(GAULIX_PAGER) && HAS_SCREEN

#include "MeshModule.h"
#include "Observer.h"
#include "input/InputBroker.h"

class GaulixPagerAlertListModule : public MeshModule
{
  public:
    GaulixPagerAlertListModule();

  protected:
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override { return false; }
    virtual bool wantUIFrame() override { return true; }
    virtual void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;

  private:
    CallbackObserver<GaulixPagerAlertListModule, const InputEvent *> inputObserver =
        CallbackObserver<GaulixPagerAlertListModule, const InputEvent *>(this, &GaulixPagerAlertListModule::handleInputEvent);

    static uint8_t frameIndex;
    static bool isOnOurFrame();

    int handleInputEvent(const InputEvent *event);
};

extern GaulixPagerAlertListModule *gaulixPagerAlertListModule;

#endif
