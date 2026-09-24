#pragma once

#include <Xtch.h>

#include "core/Screen.h"

class ReaderScreen final : public Screen {
  char bookPath[256]{};
  XtchBook xtch;
  uint32_t page = 0;
  int pagesUntilFull = 0;
  bool loaded = false;

  void loadProgress();
  void saveProgress() const;
  void showStatus(const char* title, const char* detail = nullptr);

 public:
  ReaderScreen(Gfx& gfx, MappedInput& input, const char* path);
  void onEnter() override;
  void onExit() override;
  void onResume() override;
  void loop() override;
  void render() override;
  bool isReader() const override { return true; }

  void jumpToPage(uint32_t targetPage);
};
