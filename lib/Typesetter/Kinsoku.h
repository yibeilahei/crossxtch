#pragma once

#include <cstdint>

namespace ts {

bool kinsokuCanStartColumn(uint32_t cp);
bool kinsokuCanEndColumn(uint32_t cp);
bool shouldRotate(uint32_t cp);

inline bool isHiragana(const uint32_t cp) { return cp >= 0x3040 && cp <= 0x309F; }
inline bool isKatakana(const uint32_t cp) { return cp >= 0x30A0 && cp <= 0x30FF; }
inline bool isKanji(const uint32_t cp) {
  return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0xF900 && cp <= 0xFAFF);
}

}  // namespace ts
