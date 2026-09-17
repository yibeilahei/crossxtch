#include "screens/KeyboardScreen.h"

#include <Gfx.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/UiText.h"
#include "core/fontIds.h"
#include "screens/WifiListScreen.h"

namespace {
constexpr const char* kDigits = "1234567890";
constexpr const char* kLettersRow1 = "qwertyuiop";
constexpr const char* kLettersRow2 = "asdfghjkl";
constexpr const char* kLettersRow3 = "zxcvbnm";
constexpr const char* kSymbolsRow1 = "!@#$%^&*()";
constexpr const char* kSymbolsRow2 = "-_=+[]{}:";
constexpr const char* kSymbolsRow3 = ";'\",./<>?";
constexpr int kGridRows = 5;  // 4 character rows + 1 special-key row
constexpr int kSpecialCols = 5;
}  // namespace

KeyboardScreen::KeyboardScreen(Gfx& gfx, MappedInput& input, WifiListScreen& parent, const char* title,
                               const size_t maxLength, const bool passwordMode)
    : Screen("Keyboard", gfx, input),
      parent(parent),
      title(title ? title : ""),
      maxLength(maxLength),
      passwordMode(passwordMode) {}

int KeyboardScreen::rowLength(int row) const {
  switch (row) {
    case 0:
      return static_cast<int>(strlen(kDigits));
    case 1:
      return static_cast<int>(strlen(symbolsMode ? kSymbolsRow1 : kLettersRow1));
    case 2:
      return static_cast<int>(strlen(symbolsMode ? kSymbolsRow2 : kLettersRow2));
    case 3:
      return static_cast<int>(strlen(symbolsMode ? kSymbolsRow3 : kLettersRow3));
    case 4:
      return kSpecialCols;
    default:
      return 0;
  }
}

char KeyboardScreen::charAt(int row, int col) const {
  const char* set = nullptr;
  switch (row) {
    case 0:
      set = kDigits;
      break;
    case 1:
      set = symbolsMode ? kSymbolsRow1 : kLettersRow1;
      break;
    case 2:
      set = symbolsMode ? kSymbolsRow2 : kLettersRow2;
      break;
    case 3:
      set = symbolsMode ? kSymbolsRow3 : kLettersRow3;
      break;
    default:
      return '\0';
  }
  if (col < 0 || col >= static_cast<int>(strlen(set))) {
    return '\0';
  }
  char c = set[col];
  if (!symbolsMode && c >= 'a' && c <= 'z' && shifted) {
    c = static_cast<char>(c - 'a' + 'A');
  }
  return c;
}

void KeyboardScreen::activateKey() {
  if (selRow == 4) {
    switch (selCol) {
      case 0:
        shifted = !shifted;
        break;
      case 1:
        symbolsMode = !symbolsMode;
        break;
      case 2:
        if (text.size() < maxLength) {
          text.push_back(' ');
        }
        break;
      case 3:
        if (!text.empty()) {
          text.pop_back();
        }
        break;
      case 4:
        parent.onPasswordEntered(text);
        finish();
        return;
      default:
        break;
    }
    requestUpdate();
    return;
  }

  const char c = charAt(selRow, selCol);
  if (c != '\0' && text.size() < maxLength) {
    text.push_back(c);
    requestUpdate();
  }
}

void KeyboardScreen::loop() {
  if (input.wasReleased(MappedInput::Button::Back)) {
    parent.onPasswordCancelled();
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

void KeyboardScreen::render() {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, 8, title.c_str());

  char field[80];
  if (passwordMode) {
    const size_t n = std::min(text.size(), sizeof(field) - 1);
    memset(field, '*', n);
    field[n] = '\0';
  } else {
    snprintf(field, sizeof(field), "%s", text.c_str());
  }
  gfx.fillRect(24, 40, gfx.width() - 48, gfx.lineHeight(FONT_UI) + 12, false);
  gfx.drawText(FONT_UI, 32, 48, field[0] != '\0' ? field : "_");

  const int gridTop = 40 + gfx.lineHeight(FONT_UI) + 28;
  const int rowH = (gfx.height() - gridTop - 16) / kGridRows;
  for (int row = 0; row < kGridRows; ++row) {
    const int cols = rowLength(row);
    const int cellW = gfx.width() / cols;
    const int y = gridTop + row * rowH;
    for (int col = 0; col < cols; ++col) {
      const bool selected = (row == selRow && col == selCol);
      char label[16];
      if (row == 4) {
        switch (col) {
          case 0:
            snprintf(label, sizeof(label), "%s", uiText::keyShift);
            break;
          case 1:
            snprintf(label, sizeof(label), "%s", symbolsMode ? "ABC" : "123");
            break;
          case 2:
            snprintf(label, sizeof(label), "%s", uiText::keySpace);
            break;
          case 3:
            snprintf(label, sizeof(label), "%s", uiText::keyDel);
            break;
          default:
            snprintf(label, sizeof(label), "%s", uiText::keyDone);
            break;
        }
      } else {
        snprintf(label, sizeof(label), "%c", charAt(row, col));
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
