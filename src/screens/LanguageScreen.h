#pragma once

#include "core/Screen.h"

class LanguageScreen final : public Screen {
  int index = 0;
  bool required = false;

 public:
  LanguageScreen(Gfx& gfx, MappedInput& input, bool required);
  void loop() override;
  void render() override;
};
