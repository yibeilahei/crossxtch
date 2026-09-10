#include "screens/ClockScreen.h"

#include <Gfx.h>
#include <HalClock.h>

#include <algorithm>

#include "core/Settings.h"

namespace {
//    A
//  F   B
//    G
//  E   C
//    D
constexpr uint8_t kSegA = 1 << 0;
constexpr uint8_t kSegB = 1 << 1;
constexpr uint8_t kSegC = 1 << 2;
constexpr uint8_t kSegD = 1 << 3;
constexpr uint8_t kSegE = 1 << 4;
constexpr uint8_t kSegF = 1 << 5;
constexpr uint8_t kSegG = 1 << 6;

constexpr uint8_t kDigitSegs[10] = {
    static_cast<uint8_t>(kSegA | kSegB | kSegC | kSegD | kSegE | kSegF),              // 0
    static_cast<uint8_t>(kSegB | kSegC),                                              // 1
    static_cast<uint8_t>(kSegA | kSegB | kSegD | kSegE | kSegG),                       // 2
    static_cast<uint8_t>(kSegA | kSegB | kSegC | kSegD | kSegG),                       // 3
    static_cast<uint8_t>(kSegB | kSegC | kSegF | kSegG),                               // 4
    static_cast<uint8_t>(kSegA | kSegC | kSegD | kSegF | kSegG),                       // 5
    static_cast<uint8_t>(kSegA | kSegC | kSegD | kSegE | kSegF | kSegG),               // 6
    static_cast<uint8_t>(kSegA | kSegB | kSegC),                                       // 7
    static_cast<uint8_t>(kSegA | kSegB | kSegC | kSegD | kSegE | kSegF | kSegG),       // 8
    static_cast<uint8_t>(kSegA | kSegB | kSegC | kSegD | kSegF | kSegG),               // 9
};

void drawHSeg(Gfx& gfx, const int x, const int y, const int w, const int t) {
  gfx.fillRect(x, y, w, t, true);
}

void drawVSeg(Gfx& gfx, const int x, const int y, const int t, const int h) {
  gfx.fillRect(x, y, t, h, true);
}

void drawDigit(Gfx& gfx, const int x, const int y, const int w, const int h, const int t, const int digit) {
  const uint8_t segs = (digit >= 0 && digit <= 9) ? kDigitSegs[digit] : kSegG;
  const int innerW = w - 2 * t;
  const int half = (h - t) / 2;
  if (segs & kSegA) {
    drawHSeg(gfx, x + t, y, innerW, t);
  }
  if (segs & kSegB) {
    drawVSeg(gfx, x + w - t, y + t, t, half - t);
  }
  if (segs & kSegC) {
    drawVSeg(gfx, x + w - t, y + half + t, t, half - t);
  }
  if (segs & kSegD) {
    drawHSeg(gfx, x + t, y + h - t, innerW, t);
  }
  if (segs & kSegE) {
    drawVSeg(gfx, x, y + half + t, t, half - t);
  }
  if (segs & kSegF) {
    drawVSeg(gfx, x, y + t, t, half - t);
  }
  if (segs & kSegG) {
    drawHSeg(gfx, x + t, y + half, innerW, t);
  }
}

void drawColon(Gfx& gfx, const int x, const int y, const int h, const int t) {
  const int dot = std::max(t, 8);
  gfx.fillRect(x, y + h / 3 - dot / 2, dot, dot, true);
  gfx.fillRect(x, y + (h * 2) / 3 - dot / 2, dot, dot, true);
}
}  // namespace

void ClockScreen::loop() {
  // Any key, including taps that landed during a blocking refresh.
  if (input.wasAnyReleased() || input.wasReleased(MappedInput::Button::Back) ||
      input.wasReleased(MappedInput::Button::Confirm) || input.consumeNavigationDelta() != 0) {
    finish();
    return;
  }
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (halClock.getLocalTime(hour, minute, settings.clockUtcOffsetQ) && minute != shownMinute) {
    requestUpdate();
  }
}

void ClockScreen::render() {
  gfx.clear(false);

  int hour = -1;
  int minute = -1;
  uint8_t rtcHour = 0;
  uint8_t rtcMinute = 0;
  if (halClock.getLocalTime(rtcHour, rtcMinute, settings.clockUtcOffsetQ)) {
    hour = rtcHour;
    minute = rtcMinute;
    shownMinute = rtcMinute;
  }

  const int availW = gfx.width() - 32;
  const int digitW = availW * 20 / 100;
  const int digitH = digitW * 18 / 10;
  const int thick = std::max(digitW / 6, 8);
  const int gap = std::max(digitW / 6, 8);
  const int colonW = thick;
  const int totalW = 4 * digitW + 3 * gap + colonW;
  const int x0 = (gfx.width() - totalW) / 2;
  const int y0 = (gfx.height() - digitH) / 2;

  const int d0 = hour >= 0 ? hour / 10 : -1;
  const int d1 = hour >= 0 ? hour % 10 : -1;
  const int d2 = minute >= 0 ? minute / 10 : -1;
  const int d3 = minute >= 0 ? minute % 10 : -1;

  int x = x0;
  drawDigit(gfx, x, y0, digitW, digitH, thick, d0);
  x += digitW + gap;
  drawDigit(gfx, x, y0, digitW, digitH, thick, d1);
  x += digitW + gap;
  drawColon(gfx, x, y0, digitH, thick);
  x += colonW + gap;
  drawDigit(gfx, x, y0, digitW, digitH, thick, d2);
  x += digitW + gap;
  drawDigit(gfx, x, y0, digitW, digitH, thick, d3);

  presentUi();
}
