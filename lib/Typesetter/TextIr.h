#pragma once

#include <HalStorage.h>

#include <cstdint>

#include "Layout.h"

namespace ts {

// Streaming UTF-8 → Atom. Supports aozora-lite ｜base《ruby》.
class Utf8AtomReader {
 public:
  void bind(HalFile* file);
  void seek(uint32_t byteOffset);
  uint32_t position() const { return pos; }
  bool atEnd() const { return eof && !havePending && queueCount == 0; }

  // Fills `out` and records the starting byte offset of that atom in `atomPos`.
  bool next(Atom& out, uint32_t& atomPos);

 private:
  static constexpr uint8_t kQueueCap = 8;

  HalFile* file = nullptr;
  uint32_t pos = 0;
  bool eof = false;
  bool havePending = false;
  uint32_t pendingCp = 0;
  uint32_t pendingPos = 0;

  Atom queue[kQueueCap]{};
  uint32_t queuePos[kQueueCap]{};
  uint8_t queueCount = 0;
  uint8_t queueHead = 0;

  bool readCp(uint32_t& cp, uint32_t& at);
  bool unread(uint32_t cp, uint32_t at);
  bool popQueue(Atom& out, uint32_t& atomPos);
  void pushQueue(const Atom& a, uint32_t atomPos);
};

}  // namespace ts
