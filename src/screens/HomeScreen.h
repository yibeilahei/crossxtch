#pragma once

#include "core/Screen.h"

class HomeScreen final : public Screen {
  int index = 0;
  int itemCount = 0;
  uint8_t shownMinute = 255;

  void refreshMenu();

 public:
  HomeScreen(Gfx& gfx, MappedInput& input) : Screen("Home", gfx, input) {}
  void onEnter() override;
  void onResume() override;
  void loop() override;
  void render() override;
};
