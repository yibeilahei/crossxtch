#include "screens/PageJumpScreen.h"

#include <Gfx.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/UiText.h"
#include "core/fontIds.h"
#include "screens/ReaderScreen.h"

namespace {
constexpr const char* kDigits = "1234567890";
constexpr int kDigitCols = 10;  // row 0
constexpr int kSpecialCols = 2;  // row 1: Del, Go
constexpr int kGridRows = 2;
}  // namespace

PageJumpScreen::PageJumpScreen(Gfx& gfx, MappedInput& input, ReaderScreen& reader, const uint32_t currentPage,
                               const uint16_t pageCount)
    : Screen("Page jump", gfx, input),
      reader(reader),
      currentPage(currentPage),
      pageCount(pageCount) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", pageCount);
  maxDigits = strlen(buf);
}

int PageJumpScreen::rowLength(const int row) const { return row == 0 ? kDigitCols : kSpecialCols; }

void PageJumpScreen::activateKey() {
  if (selRow == 0) {
    if (digits.size() < maxDigits) {
      digits.push_back(kDigits[selCol]);
      requestUpdate();
    }
    return;
  }
  if (selCol == 0) {
    // Del
    if (!digits.empty()) {
      digits.pop_back();
      requestUpdate();
    }
    return;
  }
  // Go: commit, clamped to [1, pageCount]. Empty input just returns to the
  // reader without jumping. Either way, skip back past ChapterSelectionScreen
  // straight to the reader (2 levels) instead of leaving the menu open.
  if (digits.empty()) {
    finish(2);
    return;
  }
  long value = strtol(digits.c_str(), nullptr, 10);
  value = std::max<long>(1, std::min<long>(value, pageCount));
  reader.jumpToPage(static_cast<uint32_t>(value - 1));
  finish(2);
}

void PageJumpScreen::loop() {
  if (input.wasReleased(MappedInput::Button::Back)) {
    finish();
    return;
  }

  bool moved = false;
  if (input.wasReleased(MappedInput::Button::Up)) {
    selRow = (selRow + kGridRows - 1) % kGridRows;
    selCol = std::min(selCol, rowLength(selRow) - 1);
    moved = true;
  } else if (input.wasReleased(MappedInput::Button::Down)) {
    selRow = (selRow + 1) % kGridRows;
    selCol = std::min(selCol, rowLength(selRow) - 1);
    moved = true;
  } else if (input.wasReleased(MappedInput::Button::Left)) {
    const int len = rowLength(selRow);
    selCol = (selCol - 1 + len) % len;
    moved = true;
  } else if (input.wasReleased(MappedInput::Button::Right)) {
    const int len = rowLength(selRow);
    selCol = (selCol + 1) % len;
    moved = true;
  } else if (input.wasReleased(MappedInput::Button::Confirm)) {
    activateKey();
    return;
  }

  if (moved) {
    requestUpdate();
  }
}

void PageJumpScreen::render() {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, 8, uiText::goToPage);

  char field[32];
  snprintf(field, sizeof(field), "%s / %u", digits.empty() ? "_" : digits.c_str(), pageCount);
  gfx.fillRect(24, 40, gfx.width() - 48, gfx.lineHeight(FONT_UI) + 12, false);
  gfx.drawText(FONT_UI, 32, 48, field);

  char currentLabel[32];
  snprintf(currentLabel, sizeof(currentLabel), uiText::currentlyOnPage, static_cast<unsigned long>(currentPage + 1));
  gfx.drawText(FONT_UI, 32, 40 + gfx.lineHeight(FONT_UI) + 20, currentLabel);

  const int gridTop = 40 + 2 * gfx.lineHeight(FONT_UI) + 44;
  const int rowH = (gfx.height() - gridTop - 16) / kGridRows;
  for (int row = 0; row < kGridRows; ++row) {
    const int cols = rowLength(row);
    const int cellW = gfx.width() / cols;
    const int y = gridTop + row * rowH;
    for (int col = 0; col < cols; ++col) {
      const bool selected = (row == selRow && col == selCol);
      char label[16];
      if (row == 0) {
        snprintf(label, sizeof(label), "%c", kDigits[col]);
      } else {
        snprintf(label, sizeof(label), "%s", col == 0 ? uiText::keyDel : uiText::keyGo);
      }
      const int x = col * cellW;
      if (selected) {
        gfx.fillRect(x + 2, y, cellW - 4, rowH - 4, true);
      }
      const int textX = x + (cellW - gfx.textWidth(FONT_UI, label)) / 2;
      gfx.drawText(FONT_UI, textX, y + (rowH - gfx.lineHeight(FONT_UI)) / 2, label, !selected);
    }
  }

  presentUi();
}
