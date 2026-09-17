#include "TextIr.h"

#include "Kinsoku.h"

namespace ts {
namespace {

constexpr uint32_t kBar = 0xFF5C;        // ｜
constexpr uint32_t kRubyOpen = 0x300A;   // 《
constexpr uint32_t kRubyClose = 0x300B;  // 》

bool isRubyBase(const uint32_t cp) { return isKanji(cp) || isHiragana(cp) || isKatakana(cp); }

void splitRuby(const uint32_t* bases, const uint8_t baseN, const uint32_t* ruby, const uint8_t rubyN, Atom* out) {
  uint8_t used = 0;
  for (uint8_t i = 0; i < baseN; ++i) {
    out[i] = Atom{};
    out[i].kind = AtomKind::Ch;
    out[i].cp = bases[i];
    const uint8_t remainBases = static_cast<uint8_t>(baseN - i);
    const uint8_t remainRuby = rubyN > used ? static_cast<uint8_t>(rubyN - used) : 0;
    const uint8_t take = remainBases > 0 ? static_cast<uint8_t>((remainRuby + remainBases - 1) / remainBases) : 0;
    const uint8_t n = take > 4 ? 4 : take;
    out[i].rubyCount = n;
    for (uint8_t r = 0; r < n && used < rubyN; ++r) {
      out[i].ruby[r] = ruby[used++];
    }
  }
}

}  // namespace

void Utf8AtomReader::bind(HalFile* f) {
  file = f;
  pos = 0;
  eof = false;
  havePending = false;
  pendingCp = 0;
  pendingPos = 0;
  queueCount = 0;
  queueHead = 0;
}

void Utf8AtomReader::seek(const uint32_t byteOffset) {
  pos = byteOffset;
  eof = false;
  havePending = false;
  queueCount = 0;
  queueHead = 0;
  if (file) {
    file->seekSet(byteOffset);
  }
}

bool Utf8AtomReader::unread(const uint32_t cp, const uint32_t at) {
  havePending = true;
  pendingCp = cp;
  pendingPos = at;
  return true;
}

bool Utf8AtomReader::popQueue(Atom& out, uint32_t& atomPos) {
  if (queueCount == 0) {
    return false;
  }
  out = queue[queueHead];
  atomPos = queuePos[queueHead];
  queueHead = static_cast<uint8_t>((queueHead + 1) % kQueueCap);
  --queueCount;
  return true;
}

void Utf8AtomReader::pushQueue(const Atom& a, const uint32_t atomPos) {
  if (queueCount >= kQueueCap) {
    return;
  }
  const uint8_t i = static_cast<uint8_t>((queueHead + queueCount) % kQueueCap);
  queue[i] = a;
  queuePos[i] = atomPos;
  ++queueCount;
}

bool Utf8AtomReader::readCp(uint32_t& cp, uint32_t& at) {
  if (havePending) {
    cp = pendingCp;
    at = pendingPos;
    havePending = false;
    return true;
  }
  if (!file || eof) {
    eof = true;
    return false;
  }
  at = pos;
  uint8_t buf[4];
  const int n = file->read(buf, 1);
  if (n != 1) {
    eof = true;
    return false;
  }
  pos += 1;
  const uint8_t b0 = buf[0];
  int extra = 0;
  uint32_t v = b0;
  if ((b0 & 0x80) == 0) {
    cp = b0;
    return true;
  }
  if ((b0 & 0xE0) == 0xC0) {
    extra = 1;
    v = b0 & 0x1F;
  } else if ((b0 & 0xF0) == 0xE0) {
    extra = 2;
    v = b0 & 0x0F;
  } else if ((b0 & 0xF8) == 0xF0) {
    extra = 3;
    v = b0 & 0x07;
  } else {
    cp = 0xFFFD;
    return true;
  }
  if (file->read(buf, static_cast<size_t>(extra)) != extra) {
    eof = true;
    cp = 0xFFFD;
    return true;
  }
  pos += static_cast<uint32_t>(extra);
  for (int i = 0; i < extra; ++i) {
    if ((buf[i] & 0xC0) != 0x80) {
      cp = 0xFFFD;
      return true;
    }
    v = (v << 6) | (buf[i] & 0x3F);
  }
  cp = v;
  return true;
}

bool Utf8AtomReader::next(Atom& out, uint32_t& atomPos) {
  if (popQueue(out, atomPos)) {
    return true;
  }
  out = Atom{};
  uint32_t cp = 0;
  uint32_t at = 0;
  while (readCp(cp, at)) {
    if (cp == '\r') {
      continue;
    }
    if (cp == '\f') {
      atomPos = at;
      out.kind = AtomKind::PageBreak;
      return true;
    }
    if (cp == '\n') {
      atomPos = at;
      out.kind = AtomKind::ColumnBreak;
      out.startEm = 1;
      uint32_t look = 0;
      uint32_t lookAt = 0;
      while (readCp(look, lookAt)) {
        if (look == '\r') {
          continue;
        }
        if (look == '\n') {
          continue;
        }
        unread(look, lookAt);
        break;
      }
      return true;
    }
    if (cp == ' ' || cp == 0x3000) {
      atomPos = at;
      out.kind = AtomKind::Space;
      return true;
    }

    if (cp == kBar) {
      uint32_t bases[kQueueCap]{};
      uint8_t baseN = 0;
      uint32_t baseAt[kQueueCap]{};
      uint32_t look = 0;
      uint32_t lookAt = 0;
      while (baseN < kQueueCap && readCp(look, lookAt)) {
        if (look == kRubyOpen) {
          break;
        }
        if (!isRubyBase(look)) {
          unread(look, lookAt);
          look = 0;
          break;
        }
        bases[baseN] = look;
        baseAt[baseN] = lookAt;
        ++baseN;
      }
      uint32_t ruby[16]{};
      uint8_t rubyN = 0;
      if (look == kRubyOpen) {
        while (rubyN < 16 && readCp(look, lookAt)) {
          if (look == kRubyClose) {
            break;
          }
          if (look == '\n') {
            unread(look, lookAt);
            break;
          }
          ruby[rubyN++] = look;
        }
      }
      if (baseN == 0) {
        continue;
      }
      Atom group[kQueueCap]{};
      splitRuby(bases, baseN, ruby, rubyN, group);
      atomPos = baseAt[0];
      out = group[0];
      for (uint8_t i = 1; i < baseN; ++i) {
        pushQueue(group[i], baseAt[i]);
      }
      return true;
    }

    uint32_t rubyBuf[4]{};
    uint8_t rubyN = 0;
    uint32_t look = 0;
    uint32_t lookAt = 0;
    if (readCp(look, lookAt)) {
      if (look == kRubyOpen) {
        while (rubyN < 4 && readCp(look, lookAt)) {
          if (look == kRubyClose) {
            break;
          }
          if (look == '\n') {
            unread(look, lookAt);
            break;
          }
          rubyBuf[rubyN++] = look;
        }
      } else {
        unread(look, lookAt);
      }
    }

    atomPos = at;
    out.kind = AtomKind::Ch;
    out.cp = cp;
    out.rubyCount = rubyN;
    for (uint8_t i = 0; i < rubyN; ++i) {
      out.ruby[i] = rubyBuf[i];
    }
    return true;
  }
  return false;
}

}  // namespace ts
