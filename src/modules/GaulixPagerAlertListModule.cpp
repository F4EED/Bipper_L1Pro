#include "configuration.h"

#if defined(GAULIX_PAGER) && HAS_SCREEN

#include "GaulixPagerAlertListModule.h"
#include "GaulixPagerModule.h"
#include "NodeDB.h"
#include "graphics/Screen.h"
#include "graphics/ScreenFonts.h"
#include "main.h"
#include <ctime>

GaulixPagerAlertListModule *gaulixPagerAlertListModule = nullptr;
uint8_t GaulixPagerAlertListModule::frameIndex = 255;

GaulixPagerAlertListModule::GaulixPagerAlertListModule() : MeshModule("gaulixalerthist")
{
#if !defined(MESHTASTIC_EXCLUDE_INPUTBROKER)
    if (inputBroker) {
        inputObserver.observe(inputBroker);
    }
#endif
}

bool GaulixPagerAlertListModule::isOnOurFrame()
{
    if (!screen || !screen->isScreenOn() || !screen->isNormalScreenActive() || GaulixPagerModule::isAlertActive()) {
        return false;
    }
    if (frameIndex == 255) {
        return false;
    }
    return screen->getCurrentFrameIndex() == frameIndex;
}

static void formatHistorySourceLine(char *buf, size_t len, NodeNum from)
{
    const meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(from);
    if (node && nodeInfoLiteHasUser(node) && node->long_name[0]) {
        snprintf(buf, len, "De: %s", node->long_name);
        return;
    }
    if (node && nodeInfoLiteHasUser(node) && node->short_name[0]) {
        snprintf(buf, len, "De: %s", node->short_name);
        return;
    }
    snprintf(buf, len, "De: %04x", static_cast<unsigned>(from & 0xffff));
}

static const char *historyTypeLabel(const GaulixPagerModule::AlertHistoryEntry &entry)
{
    if (entry.isInfo) {
        return entry.viaDm ? "Info DM" : "Info";
    }
    return entry.viaDm ? "Direct" : "Alerte";
}

static const char *historyStatusLabel(const GaulixPagerModule::AlertHistoryEntry &entry, bool isNewest)
{
    if (entry.isInfo) {
        return "Info";
    }
    if (entry.acknowledged) {
        return "Acquittee";
    }
    if (entry.closedByFin) {
        return "Cloturee";
    }
    if (entry.timedOut) {
        return "Expiree";
    }
    if (GaulixPagerModule::isAlertActive() && isNewest) {
        return "En cours";
    }
    return "En attente";
}

void GaulixPagerAlertListModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    if (state) {
        frameIndex = state->currentFrame;
    }

    display->clear();
    display->setFont(FONT_SMALL);
    const int16_t lineStep = FONT_HEIGHT_SMALL + 2;
    int16_t lineY = y + 2;

    const size_t total = GaulixPagerModule::getAlertHistoryCount();
    char lineBuf[48];

    display->setTextAlignment(TEXT_ALIGN_CENTER);
    if (total == 0) {
        display->drawString(x + display->getWidth() / 2, lineY, "Historique alertes");
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->drawString(x + 2, y + 24, "Aucune alerte recue");
        display->drawString(x + 2, y + 40, "Trackball: defiler");
        return;
    }

    const size_t scrollIdx = GaulixPagerModule::getAlertHistoryScrollIndex();
    snprintf(lineBuf, sizeof(lineBuf), "Alertes %u/%u", static_cast<unsigned>(scrollIdx + 1), static_cast<unsigned>(total));
    display->drawString(x + display->getWidth() / 2, lineY, lineBuf);
    lineY += lineStep;
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    GaulixPagerModule::AlertHistoryEntry entry;
    if (!GaulixPagerModule::getAlertHistoryEntry(scrollIdx, entry)) {
        display->drawString(x + 2, lineY, "Entree invalide");
        return;
    }

    char timeBuf[20] = "--/-- --:--";
    if (entry.time != 0) {
        time_t t = entry.time;
        struct tm *tmInfo = localtime(&t);
        if (tmInfo) {
            strftime(timeBuf, sizeof(timeBuf), "%d/%m %H:%M", tmInfo);
        }
    }

    snprintf(lineBuf, sizeof(lineBuf), "%s | %s", timeBuf, historyTypeLabel(entry));
    display->drawStringMaxWidth(x + 2, lineY, display->getWidth() - 4, lineBuf);
    lineY += lineStep;

    formatHistorySourceLine(lineBuf, sizeof(lineBuf), entry.from);
    display->drawStringMaxWidth(x + 2, lineY, display->getWidth() - 4, lineBuf);
    lineY += lineStep;

    if (entry.text[0]) {
        display->drawStringMaxWidth(x + 2, lineY, display->getWidth() - 4, entry.text);
    }

    display->setTextAlignment(TEXT_ALIGN_CENTER);
    snprintf(lineBuf, sizeof(lineBuf), "%s | Haut/Bas", historyStatusLabel(entry, scrollIdx == 0));
    display->drawString(x + display->getWidth() / 2, y + display->getHeight() - FONT_HEIGHT_SMALL - 2, lineBuf);
}

int GaulixPagerAlertListModule::handleInputEvent(const InputEvent *event)
{
    if (!event || GaulixPagerModule::isAlertActive() || !isOnOurFrame()) {
        return 0;
    }

    if (event->inputEvent == INPUT_BROKER_UP) {
        GaulixPagerModule::scrollAlertHistory(-1);
        if (screen) {
            screen->runNow();
        }
        return 1;
    }

    if (event->inputEvent == INPUT_BROKER_DOWN) {
        GaulixPagerModule::scrollAlertHistory(1);
        if (screen) {
            screen->runNow();
        }
        return 1;
    }

    return 0;
}

#endif
