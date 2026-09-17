#include "Gfx.h"

#include <Logging.h>
#include <Utf8.h>

#include <cassert>
#include <cstring>

Gfx::Gfx(HalDisplay& display) : panel(display) {}

void Gfx::begin() {
  fb = panel.getFrameBuffer();
  assert(fb != nullptr && "framebuffer missing");
  panelWidth = panel.getDisplayWidth();
  panelHeight = panel.getDisplayHeight();
  panelWidthBytes = panel.getDisplayWidthBytes();
  // Panel is landscape; logical coordinates are portrait.
  logicalWidth = panelHeight;
  logicalHeight = panelWidth;
  LOG_INF("GFX", "Panel %ux%u, logical %dx%d", panelWidth, panelHeight, logicalWidth, logicalHeight);
}

void Gfx::insertFont(const int id, const EpdFontFamily* family) {
  for (auto& slot : fonts) {
    if (slot.family == nullptr || slot.id == id) {
      slot.id = id;
      slot.family = family;
      return;
    }
  }
  LOG_ERR("GFX", "Font table full, dropped id %d", id);
}

const EpdFontFamily* Gfx::font(const int id) const {
  for (const auto& slot : fonts) {
    if (slot.family && slot.id == id) {
      return slot.family;
    }
  }
  return nullptr;
}

void Gfx::setFallbackFont(const EpdFontFamily* family) { fallbackFont = family; }

const EpdFontFamily* Gfx::faceFor(const int fontId, const uint32_t cp, const EpdFontFamily::Style style) const {
  const EpdFontFamily* family = font(fontId);
  if (family && family->hasCodepoint(cp, style)) {
    return family;
  }
  if (fallbackFont && fallbackFont->hasCodepoint(cp, style)) {
    return fallbackFont;
  }
  return family;
}

void Gfx::blitGlyph(const EpdFontData* data, const EpdGlyph* glyph, const int gx0, const int gy0, const bool black) {
  if (!data || !glyph || glyph->width <= 0 || glyph->height <= 0) {
    return;
  }
  const uint8_t* bitmap = &data->bitmap[glyph->dataOffset];
  int pixelPosition = 0;
  for (int gy = 0; gy < glyph->height; ++gy) {
    for (int gx = 0; gx < glyph->width; ++gx, ++pixelPosition) {
      const uint8_t byte = bitmap[pixelPosition >> 3];
      const uint8_t bit = static_cast<uint8_t>(7 - (pixelPosition & 7));
      if ((byte >> bit) & 1) {
        drawPixel(gx0 + gx, gy0 + gy, black);
      }
    }
  }
}

void Gfx::toPanel(const int x, const int y, int& phyX, int& phyY) const {
  // Portrait logical (W=panelH, H=panelW) → panel, 90° clockwise.
  phyX = y;
  phyY = static_cast<int>(panelHeight) - 1 - x;
}

void Gfx::clear(const bool black) { panel.clearScreen(black ? 0x00 : 0xFF); }

void Gfx::drawPixel(const int x, const int y, const bool black) {
  if (!fb) {
    return;
  }
  int phyX = 0;
  int phyY = 0;
  toPanel(x, y, phyX, phyY);
  if (phyX < 0 || phyX >= panelWidth || phyY < 0 || phyY >= panelHeight) {
    return;
  }
  const uint32_t byteIndex = static_cast<uint32_t>(phyY) * panelWidthBytes + static_cast<uint32_t>(phyX / 8);
  const uint8_t bit = static_cast<uint8_t>(7 - (phyX % 8));
  if (black) {
    fb[byteIndex] &= static_cast<uint8_t>(~(1u << bit));
  } else {
    fb[byteIndex] |= static_cast<uint8_t>(1u << bit);
  }
}

void Gfx::fillRect(int x, int y, int w, int h, const bool black) {
  if (w <= 0 || h <= 0) {
    return;
  }
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > logicalWidth) {
    w = logicalWidth - x;
  }
  if (y + h > logicalHeight) {
    h = logicalHeight - y;
  }
  for (int yy = y; yy < y + h; ++yy) {
    for (int xx = x; xx < x + w; ++xx) {
      drawPixel(xx, yy, black);
    }
  }
}

void Gfx::drawText(const int fontId, const int x, const int y, const char* text, const bool black,
                   const EpdFontFamily::Style style) {
  if (!text || *text == '\0') {
    return;
  }
  const EpdFontFamily* family = font(fontId);
  if (!family) {
    LOG_ERR("GFX", "Font %d missing", fontId);
    return;
  }
  const int yPos = y + family->getData(style)->ascender;
  int cursorX = x;
  int32_t prevAdvanceFP = 0;
  uint32_t prevCp = 0;
  const EpdFontFamily* prevFace = nullptr;
  const char* cursor = text;
  uint32_t cp = 0;
  while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&cursor)))) {
    if (utf8IsCombiningMark(cp)) {
      continue;
    }
    cp = family->applyLigatures(cp, cursor, style);
    const EpdFontFamily* face = faceFor(fontId, cp, style);
    if (prevCp != 0) {
      const int8_t kern = (face == prevFace && face) ? face->getKerning(prevCp, cp, style) : 0;
      cursorX += fp4::toPixel(prevAdvanceFP + kern);
    }
    const EpdGlyph* glyph = face ? face->getGlyph(cp, style) : nullptr;
    prevAdvanceFP = glyph ? glyph->advanceX : 0;
    if (glyph) {
      blitGlyph(face->getData(style), glyph, cursorX + glyph->left, yPos - glyph->top, black);
    }
    prevCp = cp;
    prevFace = face;
  }
}

int Gfx::drawCodepoint(const int fontId, const int x, const int y, const uint32_t cp, const bool black,
                       const EpdFontFamily::Style style) {
  const EpdFontFamily* family = font(fontId);
  const EpdFontFamily* face = faceFor(fontId, cp, style);
  if (!family || !face || !face->hasCodepoint(cp, style)) {
    return 0;
  }
  const EpdGlyph* glyph = face->getGlyph(cp, style);
  if (!glyph) {
    return 0;
  }
  blitGlyph(face->getData(style), glyph, x + glyph->left, y + family->getData(style)->ascender - glyph->top, black);
  return fp4::toPixel(glyph->advanceX);
}

void Gfx::drawCenteredText(const int fontId, const int y, const char* text, const bool black,
                           const EpdFontFamily::Style style) {
  const int w = textWidth(fontId, text, style);
  drawText(fontId, (logicalWidth - w) / 2, y, text, black, style);
}

int Gfx::textWidth(const int fontId, const char* text, const EpdFontFamily::Style style) const {
  const EpdFontFamily* family = font(fontId);
  if (!family || !text) {
    return 0;
  }
  int minX = 0;
  int maxX = 0;
  bool any = false;
  int lastBaseX = 0;
  int32_t prevAdvanceFP = 0;
  uint32_t prevCp = 0;
  const EpdFontFamily* prevFace = nullptr;
  const char* cursor = text;
  uint32_t cp = 0;
  while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&cursor)))) {
    if (utf8IsCombiningMark(cp)) {
      continue;
    }
    cp = family->applyLigatures(cp, cursor, style);
    const EpdFontFamily* face = faceFor(fontId, cp, style);
    if (prevCp != 0) {
      const int8_t kern = (face == prevFace && face) ? face->getKerning(prevCp, cp, style) : 0;
      lastBaseX += fp4::toPixel(prevAdvanceFP + kern);
    }
    const EpdGlyph* glyph = face ? face->getGlyph(cp, style) : nullptr;
    if (glyph) {
      const int gx0 = lastBaseX + glyph->left;
      const int gx1 = gx0 + glyph->width;
      if (!any) {
        minX = gx0;
        maxX = gx1;
        any = true;
      } else {
        if (gx0 < minX) {
          minX = gx0;
        }
        if (gx1 > maxX) {
          maxX = gx1;
        }
      }
      prevAdvanceFP = glyph->advanceX;
    } else {
      prevAdvanceFP = 0;
    }
    prevCp = cp;
    prevFace = face;
  }
  return any ? maxX - minX : 0;
}

int Gfx::lineHeight(const int fontId) const {
  const EpdFontFamily* family = font(fontId);
  if (!family) {
    return 16;
  }
  return family->getData()->advanceY;
}

void Gfx::present(const HalDisplay::RefreshMode mode) { panel.displayBuffer(mode); }

void Gfx::displayGrayscaleBase(const HalDisplay::RefreshMode fallback) { panel.displayGrayscaleBase(fallback); }

void Gfx::startGrayscaleBase(const HalDisplay::RefreshMode fallback) { panel.startGrayscaleBase(fallback); }

void Gfx::finishGrayscaleBase() { panel.finishGrayscaleBase(); }

void Gfx::preconditionGrayscale() { panel.preconditionGrayscale(); }

void Gfx::copyGrayscaleLsbBuffers() { panel.copyGrayscaleLsbBuffers(fb); }

void Gfx::copyGrayscaleMsbBuffers() { panel.copyGrayscaleMsbBuffers(fb); }

void Gfx::displayGrayBuffer() { panel.displayGrayBuffer(); }

void Gfx::startGrayBuffer() { panel.startGrayBuffer(); }

void Gfx::finishGrayBuffer() { panel.finishGrayBuffer(); }

void Gfx::cleanupGrayscaleBuffers() { panel.cleanupGrayscaleBuffers(fb); }

bool Gfx::combinesGrayscaleBase() const { return panel.combinesGrayscaleBase(); }
