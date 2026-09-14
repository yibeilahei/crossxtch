#include "core/Screen.h"

#include <Gfx.h>
#include <Logging.h>

#include "core/ScreenManager.h"

void Screen::onEnter() {
  LOG_DBG("SCR", "Enter %s", name);
  needsCleanRefresh = true;
}

void Screen::onExit() { LOG_DBG("SCR", "Exit %s", name); }

void Screen::onResume() { needsCleanRefresh = true; }

void Screen::presentUi() {
  gfx.present(needsCleanRefresh ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  needsCleanRefresh = false;
}

void Screen::requestUpdate() { screenManager.requestUpdate(); }
void Screen::finish(const int levels) { screenManager.pop(levels); }
void Screen::push(std::unique_ptr<Screen> screen) { screenManager.push(std::move(screen)); }
void Screen::goHome() { screenManager.goHome(); }
void Screen::goToReader(const char* path) { screenManager.goToReader(path); }
void Screen::goToBrowser() { screenManager.goToBrowser(); }
void Screen::goToSettings() { screenManager.goToSettings(); }
void Screen::goToFirmwareUpdate(const bool recovery) { screenManager.goToFirmwareUpdate(recovery); }
void Screen::goToWifiFileTransfer() { screenManager.goToWifiFileTransfer(); }
