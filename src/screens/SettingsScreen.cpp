#include "SettingsScreen.h"

#include <Gfx.h>
#include <HalClock.h>
#include <HalDisplay.h>
#include <HalPowerManager.h>
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

void formatTrueSleep(char* out, size_t outSize) {
  switch (settings.trueSleepMinutes) {
    case Settings::kSleep15Min:
      snprintf(out, outSize, "Sleep: 15 min");
      break;
    case Settings::kSleep1Hour:
      snprintf(out, outSize, "Sleep: 1 hour");
      break;
    case Settings::kSleep6Hours:
      snprintf(out, outSize, "Sleep: 6 hours");
      break;
    default:
      snprintf(out, outSize, "Sleep: none");
      break;
  }
}

void formatClockMode(char* out, size_t outSize) {
  if (settings.clockModeSeconds == 0) {
    snprintf(out, outSize, "Clock mode: none");
  } else {
    snprintf(out, outSize, "Clock mode: %u sec", settings.clockModeSeconds);
  }
}

void bumpClockMode() {
  switch (settings.clockModeSeconds) {
    case 0:
      settings.clockModeSeconds = 30;
      break;
    case 30:
      settings.clockModeSeconds = 45;
      break;
    case 45:
      settings.clockModeSeconds = 60;
      break;
    default:
      settings.clockModeSeconds = 0;
      break;
  }
}

void bumpTrueSleep() {
  switch (settings.trueSleepMinutes) {
    case Settings::kSleepNone:
      settings.trueSleepMinutes = Settings::kSleep15Min;
      break;
    case Settings::kSleep15Min:
      settings.trueSleepMinutes = Settings::kSleep1Hour;
      break;
    case Settings::kSleep1Hour:
      settings.trueSleepMinutes = Settings::kSleep6Hours;
      break;
    default:
      settings.trueSleepMinutes = Settings::kSleepNone;
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
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (halClock.getLocalTime(hour, minute, settings.clockUtcOffsetQ) && minute != shownMinute) {
    requestUpdate();
  }
  if (ui::applyDelta(index, input.consumeNavigationDelta(), itemCount())) {
    requestUpdate();
  } else if (input.wasReleased(MappedInput::Button::Confirm)) {
    if (index == 0) {
      bumpClockMode();
      LOG_INF("SET", "Clock mode %u sec", settings.clockModeSeconds);
    } else if (index == 1) {
      bumpTrueSleep();
      char sleepLog[32];
      formatTrueSleep(sleepLog, sizeof(sleepLog));
      LOG_INF("SET", "%s", sleepLog);
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

  char clock[8];
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (halClock.getLocalTime(hour, minute, settings.clockUtcOffsetQ)) {
    snprintf(clock, sizeof(clock), "%02u:%02u", hour, minute);
    gfx.drawCenteredText(FONT_UI_BOLD, 8, clock);
    shownMinute = minute;
  }

  char bat[16];
  snprintf(bat, sizeof(bat), "%u%%", static_cast<unsigned>(powerManager.getBatteryPercentage()));
  gfx.drawText(FONT_UI, gfx.width() - gfx.textWidth(FONT_UI, bat) - 12, 8, bat);

  char clockMode[32];
  char deep[32];
  char night[32];
  char tilt[32];
  formatClockMode(clockMode, sizeof(clockMode));
  formatTrueSleep(deep, sizeof(deep));
  snprintf(night, sizeof(night), "Night mode: %s", settings.nightMode ? "on" : "off");
  snprintf(tilt, sizeof(tilt), "Tilt page turn: %s", settings.tiltPageTurn ? "on" : "off");

  const char* labels[7];
  labels[0] = clockMode;
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
