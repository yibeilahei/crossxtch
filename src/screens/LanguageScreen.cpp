#include "LanguageScreen.h"

#include <Gfx.h>
#include <Logging.h>

#include "core/Settings.h"
#include "core/UiList.h"
#include "core/UiText.h"
#include "core/fontIds.h"

namespace {
constexpr int kCount = 3;
constexpr const char* kTitle = "Language / 言語 / 语言";
constexpr const char* kNames[kCount] = {"English", "日本語", "中文"};
}  // namespace

LanguageScreen::LanguageScreen(Gfx& gfx, MappedInput& input, const bool required)
    : Screen("Language", gfx, input), required(required) {
  if (settings.languageChosen()) {
    index = static_cast<int>(settings.language);
  }
}

void LanguageScreen::loop() {
  if (!required && input.wasReleased(MappedInput::Button::Back)) {
    finish();
    return;
  }
  if (ui::applyDelta(index, input.consumeNavigationDelta(), kCount)) {
    requestUpdate();
  } else if (input.wasReleased(MappedInput::Button::Confirm)) {
    settings.language = static_cast<uint8_t>(index);
    uiText::apply();
    settings.save();
    LOG_INF("LANG", "Language %u", settings.language);
    finish();
  }
}

void LanguageScreen::render() {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, 8, kTitle);
  const int rowH = gfx.lineHeight(FONT_UI) + 10;
  const int startY = 120;
  for (int i = 0; i < kCount; ++i) {
    ui::drawMenuRow(gfx, startY + i * rowH, rowH, kNames[i], i == index);
  }
  presentUi();
}
