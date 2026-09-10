#pragma once

#include <string>
#include <vector>

#include "core/Screen.h"
#include "network/WifiManager.h"

class WifiListScreen final : public Screen {
  enum class State { Scanning, NetworkList, Connecting, Failed, ClearPassword };

  State state = State::Scanning;
  std::vector<WifiManager::Network> networks;
  int index = 0;
  int window = 0;
  std::string pendingSsid;
  std::string enteredPassword;
  bool usedSavedPassword = false;
  bool autoConnecting = false;
  bool skipAutoJoin = false;

  void selectNetwork(bool fromAutoJoin = false);
  bool tryAutoJoin();
  void startConnecting(const char* password);
  bool promptPassword();
  void showNetworkList();
  void onConnectFailed();
  void goToFileTransfer();

 public:
  WifiListScreen(Gfx& gfx, MappedInput& input) : Screen("WifiList", gfx, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render() override;
  bool blocksSleep() const override { return true; }

  // Called directly by KeyboardScreen (this screen is inactive on the stack
  // while the keyboard is up), mirroring ReaderScreen/ChapterSelectionScreen.
  void onPasswordEntered(const std::string& password);
  void onPasswordCancelled();
};
