#include "HomeScreen.h"

#include <Gfx.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>

#include "core/Settings.h"
#include "core/UiList.h"
#include "core/fontIds.h"

void HomeScreen::refreshMenu() {
  const bool hasContinue = settings.lastBookPath[0] != '\0' && Storage.exists(settings.lastBookPath);
  itemCount = hasContinue ? 4 : 3;
  if (index >= itemCount) {
    index = 0;
  }
}

void HomeScreen::onEnter() {
  Screen::onEnter();
  refreshMenu();
  LOG_INF("HOME", "Continue %s last='%s'", itemCount == 4 ? "yes" : "no", settings.lastBookPath);
  requestUpdate();
}

void HomeScreen::onResume() {
  Screen::onResume();
  refreshMenu();
}

void HomeScreen::loop() {
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (halClock.getLocalTime(hour, minute, settings.clockUtcOffsetQ) && minute != shownMinute) {
    requestUpdate();
  }
  if (ui::applyDelta(index, input.consumeNavigationDelta(), itemCount)) {
    requestUpdate();
  } else if (input.wasReleased(MappedInput::Button::Confirm)) {
    const bool hasContinue = itemCount == 4;
    // Item order: [Continue?], Browse, File Transfer, Settings.
    int i = index - (hasContinue ? 1 : 0);
    if (hasContinue && index == 0) {
      LOG_DBG("HOME", "Continue");
      goToReader(settings.lastBookPath);
    } else if (i == 0) {
      LOG_DBG("HOME", "Browse");
      goToBrowser();
    } else if (i == 1) {
      LOG_DBG("HOME", "File Transfer");
      goToWifiFileTransfer();
    } else {
      LOG_DBG("HOME", "Settings");
      goToSettings();
    }
  }
}

void HomeScreen::render() {
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

  const bool hasContinue = itemCount == 4;
  const char* labels[4];
  int n = 0;
  if (hasContinue) {
    labels[n++] = "Continue";
  }
  labels[n++] = "Browse";
  labels[n++] = "File Transfer";
  labels[n++] = "Settings";

  const int rowH = gfx.lineHeight(FONT_UI) + 10;
  const int startY = 120;
  for (int i = 0; i < n; ++i) {
    ui::drawMenuRow(gfx, startY + i * rowH, rowH, labels[i], i == index);
  }

  if (hasContinue) {
    gfx.drawText(FONT_UI, 24, gfx.height() - 48, settings.lastBookPath);
  }
  presentUi();
}
