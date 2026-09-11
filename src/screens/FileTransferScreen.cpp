#include "screens/FileTransferScreen.h"

#include <Gfx.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>

#include "core/fontIds.h"
#include "network/ClockSync.h"
#include "network/WifiSession.h"

void FileTransferScreen::onEnter() {
  Screen::onEnter();
  started = server.begin();
  if (!started) {
    LOG_ERR("XFER", "Failed to start file transfer server");
  }
  requestUpdate();
}

void FileTransferScreen::onExit() {
  server.stop();
  Screen::onExit();
  WifiSession::end(gfx);
}

void FileTransferScreen::loop() {
  if (started) {
    server.handleClient();
  }
  // After the URL is on screen and HTTP has had a tick, sync RTC. Do not run
  // this in goToFileTransfer() — NTP/timezone blocked the file manager.
  if (started && painted && !clockTried) {
    clockTried = true;
    ClockSync::onWifiConnected();
  }
  if (input.wasReleased(MappedInput::Button::Back)) {
    finish();
  }
}

void FileTransferScreen::render() {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, 24, "File Transfer");

  if (!started) {
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2, "Failed to start server");
  } else {
    char line[80];
    snprintf(line, sizeof(line), "Network: %s", ssid.c_str());
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2 - 60, line);
    if (!server.hostname().empty()) {
      snprintf(line, sizeof(line), "http://%s.local/", server.hostname().c_str());
      gfx.drawCenteredText(FONT_UI, gfx.height() / 2 - 20, line);
    }
    snprintf(line, sizeof(line), "http://%s/", WiFi.localIP().toString().c_str());
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2 + 20, line);
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2 + 60, "Open one of these in a browser");
  }

  gfx.drawCenteredText(FONT_UI, gfx.height() - 40, "Back to stop");
  presentUi();
  painted = true;
}
