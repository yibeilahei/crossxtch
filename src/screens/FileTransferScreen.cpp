#include "screens/FileTransferScreen.h"

#include <Gfx.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>

#include "core/UiText.h"
#include "core/fontIds.h"
#include "network/ClockSync.h"
#include "network/WifiSession.h"

void FileTransferScreen::onEnter() {
  Screen::onEnter();
  // Keep the radio awake and allow the AP to come back without a reboot.
  // NTP (monthly) / timezone HTTP must finish before WebServer binds sockets.
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  ClockSync::onWifiConnected();
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
  if (started && WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
  if (input.wasReleased(MappedInput::Button::Back)) {
    finish();
  }
}

void FileTransferScreen::render() {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, 24, uiText::fileTransfer);

  if (!started) {
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2, uiText::serverStartFailed);
  } else {
    char line[80];
    snprintf(line, sizeof(line), uiText::networkSsid, ssid.c_str());
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2 - 60, line);
    if (!server.hostname().empty()) {
      snprintf(line, sizeof(line), "http://%s.local/", server.hostname().c_str());
      gfx.drawCenteredText(FONT_UI, gfx.height() / 2 - 20, line);
    }
    snprintf(line, sizeof(line), "http://%s/", WiFi.localIP().toString().c_str());
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2 + 20, line);
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2 + 60, uiText::browserHint);
  }

  gfx.drawCenteredText(FONT_UI, gfx.height() - 40, uiText::backToStop);
  presentUi();
}
