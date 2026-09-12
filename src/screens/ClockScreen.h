#pragma once

#include "core/Screen.h"

class ClockScreen final : public Screen {
  uint8_t shownMinute = 255;

 public:
  ClockScreen(Gfx& gfx, MappedInput& input) : Screen("Clock", gfx, input) {}
  void loop() override;
  void render() override;
  bool isClock() const override { return true; }
};
