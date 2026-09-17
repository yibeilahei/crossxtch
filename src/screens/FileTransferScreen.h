#pragma once

#include <string>

#include "core/Screen.h"
#include "network/FileTransferServer.h"

// Shown once Wi-Fi is connected: runs the HTTP file transfer server and
// displays the connection URL. Back ends the Wi-Fi session (restart).
class FileTransferScreen final : public Screen {
  std::string ssid;
  FileTransferServer server;
  bool started = false;

 public:
  FileTransferScreen(Gfx& gfx, MappedInput& input, std::string ssid)
      : Screen("FileTransfer", gfx, input), ssid(std::move(ssid)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render() override;
  bool blocksSleep() const override { return true; }
};
