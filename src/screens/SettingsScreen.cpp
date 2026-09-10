#include "SettingsScreen.h"

#include <Gfx.h>
#include <HalDisplay.h>
#include <HalTiltSensor.h>
#include <Logging.h>

#include <cstdio>

#include "core/Settings.h"
#include "core/UiList.h"
#include "core/fontIds.h"

#ifndef CROSSXTCH_VERSION
#define CROSSXTCH_VERSION "dev"
#endif

namespace {
// Tilt page-turn is only offered on boards with the QMI8658 IMU (X3).
bool hasTilt() { return halTiltSensor.isAvailable(); }
int itemCount() { return hasTilt() ? 7 : 6; }
int tiltIndex() { return 4; }
int firmwareIndex() { return hasTilt() ? 5 : 4; }

void formatTimeout(char* out, size_t outSize, const char* prefix, const uint8_t minutes) {
  if (minutes == 0) {
    snprintf(out, outSize, "%s: never", prefix);
  } else {
    snprintf(out, outSize, "%s: %u min", prefix, minutes);
  }
}

void formatLightSleep(char* out, size_t outSize) {
  if (settings.sleepTimeoutSeconds == 0) {
    snprintf(out, outSize, "Light sleep: none");
  } else {
    snprintf(out, outSize, "Light sleep: %u sec", settings.sleepTimeoutSeconds);
  }
}

void bumpLightSleep() {
  switch (settings.sleepTimeoutSeconds) {
    case 0:
      settings.sleepTimeoutSeconds = 30;
      break;
    case 30:
      settings.sleepTimeoutSeconds = 45;
      break;
    case 45:
      settings.sleepTimeoutSeconds = 60;
      break;
    default:
      settings.sleepTimeoutSeconds = 0;
      break;
  }
}

void bumpTrueSleep() {
  switch (settings.trueSleepMinutes) {
    case 0:
      settings.trueSleepMinutes = 10;
      break;
    case 10:
      settings.trueSleepMinutes = 20;
      break;
    case 20:
      settings.trueSleepMinutes = 30;
      break;
    default:
      settings.trueSleepMinutes = 0;
      break;
  }
}

const char* refreshLabel() {
  switch (settings.refreshEveryNPages) {
    case 1:
      return "Refresh: every page";
    case 10:
      return "Refresh: every 10 pages";
    case 15:
      return "Refresh: every 15 pages";
    case 20:
      return "Refresh: every 20 pages";
    default:
      return "Refresh: every 5 pages";
  }
}

void bumpRefresh() {
  switch (settings.refreshEveryNPages) {
    case 1:
      settings.refreshEveryNPages = 5;
      break;
    case 5:
      settings.refreshEveryNPages = 10;
      break;
    case 10:
      settings.refreshEveryNPages = 15;
      break;
    case 15:
      settings.refreshEveryNPages = 20;
      break;
    default:
      settings.refreshEveryNPages = 1;
      break;
  }
}
}  // namespace

void SettingsScreen::loop() {
  if (input.wasReleased(MappedInput::Button::Back)) {
    settings.save();
    finish();
    return;
  }
  if (ui::applyDelta(index, input.consumeNavigationDelta(), itemCount())) {
    requestUpdate();
  } else if (input.wasReleased(MappedInput::Button::Confirm)) {
    if (index == 0) {
      bumpLightSleep();
      LOG_INF("SET", "Light sleep %u sec", settings.sleepTimeoutSeconds);
    } else if (index == 1) {
      bumpTrueSleep();
      LOG_INF("SET", "Sleep %u min", settings.trueSleepMinutes);
    } else if (index == 2) {
      bumpRefresh();
      LOG_INF("SET", "%s", refreshLabel());
    } else if (index == 3) {
      settings.nightMode = settings.nightMode ? 0 : 1;
      display.setInverted(settings.nightMode != 0);
      LOG_INF("SET", "Night mode %s", settings.nightMode ? "on" : "off");
    } else if (hasTilt() && index == tiltIndex()) {
      settings.tiltPageTurn = settings.tiltPageTurn ? 0 : 1;
      LOG_INF("SET", "Tilt page turn %s", settings.tiltPageTurn ? "on" : "off");
    } else if (index == firmwareIndex()) {
      settings.save();
      goToFirmwareUpdate();
      return;
    } else {
      settings.save();
      finish();
      return;
    }
    settings.save();
    requestUpdate();
  }
}

void SettingsScreen::render() {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, 24, "Settings");

  char light[32];
  char deep[32];
  char night[32];
  char tilt[32];
  formatLightSleep(light, sizeof(light));
  formatTimeout(deep, sizeof(deep), "Sleep", settings.trueSleepMinutes);
  snprintf(night, sizeof(night), "Night mode: %s", settings.nightMode ? "on" : "off");
  snprintf(tilt, sizeof(tilt), "Tilt page turn: %s", settings.tiltPageTurn ? "on" : "off");

  const char* labels[7];
  labels[0] = light;
  labels[1] = deep;
  labels[2] = refreshLabel();
  labels[3] = night;
  if (hasTilt()) {
    labels[4] = tilt;
    labels[5] = "Update firmware";
    labels[6] = "Back";
  } else {
    labels[4] = "Update firmware";
    labels[5] = "Back";
  }

  const int rowH = gfx.lineHeight(FONT_UI) + 10;
  const int startY = 90;
  for (int i = 0; i < itemCount(); ++i) {
    ui::drawMenuRow(gfx, startY + i * rowH, rowH, labels[i], i == index);
  }

  gfx.drawCenteredText(FONT_UI, gfx.height() - 40, "crossxtch " CROSSXTCH_VERSION);
  presentUi();
}
