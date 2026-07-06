#include "configuration.h"

#if defined(GAULIX_PAGER) && HAS_SCREEN

#include "GaulixPagerModule.h"
#include "gps/RTC.h"
#include "graphics/ScreenFonts.h"
#include "main.h"
#include <ctime>

GaulixPagerModule *gaulixPagerModule = nullptr;

uint32_t GaulixPagerModule::alertCount = 0;
uint32_t GaulixPagerModule::lastAlertTime = 0;

GaulixPagerModule::GaulixPagerModule() : MeshModule("gaulixpager") {}

void GaulixPagerModule::recordAlert()
{
    alertCount++;
    lastAlertTime = getTime();
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
    display->drawString(statusX + 10, lineY, "En écoute");
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
