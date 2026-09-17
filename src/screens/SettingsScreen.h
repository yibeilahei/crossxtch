#pragma once

#include "core/Screen.h"

class SettingsScreen final : public Screen {
  int index = 0;
  uint8_t shownMinute = 255;
  bool cacheArmed = false;
  bool cacheCleared = false;

 public:
  SettingsScreen(Gfx& gfx, MappedInput& input) : Screen("Settings", gfx, input) {}
  void loop() override;
  void render() override;
};
