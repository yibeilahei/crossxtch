#pragma once

#include <TypesetBook.h>
#include <Xtch.h>

#include <memory>
#include <vector>

#include "core/Screen.h"

class ReaderScreen final : public Screen {
  char bookPath[256]{};
  XtchBook xtch;
  // Heap-allocated only for .txt/.epub so .xtch open does not need a contiguous
  // TypesetBook (~24 KB of glyph-run arrays) while BrowserScreen is still alive.
  std::unique_ptr<TypesetBook> typed;
  bool typesetMode = false;
  uint32_t page = 0;
  int pagesUntilFull = 0;
  bool loaded = false;

  unsigned long lastOpenProgressMs = 0;

  void loadProgress();
  void saveProgress() const;
  void showStatus(const char* title, const char* detail = nullptr);
  void showOpenProgress(uint16_t done, uint16_t total);
  static void onOpenProgress(void* ctx, uint16_t done, uint16_t total);
  uint16_t bookPageCount() const;
  const char* bookError() const;
  const std::vector<xtch::ChapterInfo>& bookChapters();

 public:
  ReaderScreen(Gfx& gfx, MappedInput& input, const char* path);
  void onEnter() override;
  void onExit() override;
  void onResume() override;
  void loop() override;
  void render() override;
  bool isReader() const override { return true; }

  void jumpToPage(uint32_t targetPage);
  XgfFont* cjkFont();
};
