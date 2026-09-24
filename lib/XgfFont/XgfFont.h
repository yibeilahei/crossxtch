#pragma once

#include <HalStorage.h>

#include <cstdint>

#include "XgfFormat.h"

class Gfx;

// 1-bit UI face. Body glyphs only; ruby and grayscale page blits are not loaded.
class XgfFont {
 public:
  static constexpr uint32_t kUiLruBytes = 16 * 1024;
  static constexpr const char* kDefaultPath = "/.crossxtch/reading.xgf2";

  XgfFont() = default;
  ~XgfFont() { close(); }

  XgfFont(const XgfFont&) = delete;
  XgfFont& operator=(const XgfFont&) = delete;

  bool load(const char* path);
  void close();
  bool loaded() const { return opened; }

  uint8_t emPx() const { return header.emPx; }
  uint16_t bodyCount() const { return header.bodyCount; }
  const char* lastError() const { return error; }

  // Unicode → glyph id. 0xFFFF = missing.
  uint16_t glyphId(uint32_t cp) const;
  bool hasGlyph(const uint32_t cp) const { return glyphId(cp) != 0xFFFF; }

  // Load misses in glyphId order (coalesced SD). ids are body ids.
  void prewarm(const uint16_t* bodyIds, uint16_t count);

  // 1-bit UI: Latin from Gfx fontId, other glyphs from this face, scaled to the
  // UI ascender. y matches Gfx::drawText. Stops before maxX (0 = no clip).
  int drawUtf8(Gfx& gfx, int fontId, int x, int y, const char* text, bool black, int maxX = 0);

 private:
  struct LruEntry {
    uint16_t id = 0xFFFF;  // 0xFFFF = empty
    uint16_t recency = 0;
  };
  struct Lru {
    uint8_t* pixels = nullptr;
    LruEntry* entries = nullptr;
    uint16_t cap = 0;
    uint16_t used = 0;
    uint16_t clock = 1;
    uint16_t stride = 0;
  };

  char filepath[256]{};
  const char* error = "not loaded";
  HalFile file;
  xgf::Header header{};
  bool opened = false;

  xgf::Interval* intervals = nullptr;
  struct Hi {
    uint16_t begin = 0;
    uint16_t count = 0;
  };
  Hi hi[256]{};

  Lru bodyLru;

  bool readHeader();
  bool loadTables();
  void buildHi();
  void freeTables();
  bool allocLru();
  void freeLru();
  bool allocTable(Lru& t, uint16_t cap, uint16_t stride);
  void freeTable(Lru& t);
  const uint8_t* cacheIn(Lru& t, uint16_t bodyId);
  const uint8_t* cacheSlot(uint16_t bodyId);
  bool readSlot(uint16_t bodyId, uint8_t* dest);
  bool blitUi(Gfx& gfx, int x, int y, uint16_t bodyId, int size, bool black);
};
