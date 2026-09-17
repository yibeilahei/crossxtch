#include "AtomFile.h"

#include <cstring>

namespace ts {
namespace {

bool writeU8(HalFile& f, const uint8_t v) { return f.write(&v, 1) == 1; }
bool writeU32(HalFile& f, const uint32_t v) {
  const uint8_t b[4] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v >> 16),
                        static_cast<uint8_t>(v >> 24)};
  return f.write(b, 4) == 4;
}
bool readU8(HalFile& f, uint8_t& v) { return f.read(&v, 1) == 1; }
bool readU32(HalFile& f, uint32_t& v) {
  uint8_t b[4];
  if (f.read(b, 4) != 4) {
    return false;
  }
  v = static_cast<uint32_t>(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
  return true;
}

}  // namespace

bool AtomWriter::open(const char* path) {
  pos = 0;
  return Storage.openFileForWrite("ATM", path, file);
}

void AtomWriter::close() {
  if (file.isOpen()) {
    file.close();
  }
}

bool AtomWriter::write(const Atom& a) {
  if (!writeU8(file, static_cast<uint8_t>(a.kind))) {
    return false;
  }
  pos += 1;
  switch (a.kind) {
    case AtomKind::Ch:
      if (!writeU32(file, a.cp) || !writeU8(file, a.rubyCount)) {
        return false;
      }
      pos += 5;
      for (uint8_t i = 0; i < a.rubyCount && i < 4; ++i) {
        if (!writeU32(file, a.ruby[i])) {
          return false;
        }
        pos += 4;
      }
      break;
    case AtomKind::Tcy:
      if (!writeU8(file, a.tcyCount)) {
        return false;
      }
      pos += 1;
      for (uint8_t i = 0; i < a.tcyCount && i < 4; ++i) {
        if (!writeU32(file, i == 0 ? a.cp : a.tcy[i])) {
          return false;
        }
        pos += 4;
      }
      break;
    case AtomKind::ColumnBreak:
      if (!writeU8(file, a.startEm)) {
        return false;
      }
      pos += 1;
      break;
    default:
      break;
  }
  return true;
}

bool AtomReader::open(const char* path) {
  pos = 0;
  return Storage.openFileForRead("ATM", path, file);
}

void AtomReader::close() {
  if (file.isOpen()) {
    file.close();
  }
}

bool AtomReader::seek(const uint32_t byteOff) {
  pos = byteOff;
  return file.seekSet(byteOff);
}

bool AtomReader::next(Atom& a) {
  a = Atom{};
  uint8_t kind = 0;
  if (!readU8(file, kind)) {
    return false;
  }
  pos += 1;
  a.kind = static_cast<AtomKind>(kind);
  switch (a.kind) {
    case AtomKind::Ch:
      if (!readU32(file, a.cp) || !readU8(file, a.rubyCount)) {
        return false;
      }
      pos += 5;
      if (a.rubyCount > 4) {
        a.rubyCount = 4;
      }
      for (uint8_t i = 0; i < a.rubyCount; ++i) {
        if (!readU32(file, a.ruby[i])) {
          return false;
        }
        pos += 4;
      }
      break;
    case AtomKind::Tcy: {
      if (!readU8(file, a.tcyCount)) {
        return false;
      }
      pos += 1;
      if (a.tcyCount > 4) {
        a.tcyCount = 4;
      }
      for (uint8_t i = 0; i < a.tcyCount; ++i) {
        uint32_t cp = 0;
        if (!readU32(file, cp)) {
          return false;
        }
        pos += 4;
        a.tcy[i] = cp;
        if (i == 0) {
          a.cp = cp;
        }
      }
      break;
    }
    case AtomKind::ColumnBreak:
      if (!readU8(file, a.startEm)) {
        return false;
      }
      pos += 1;
      break;
    default:
      break;
  }
  return true;
}

}  // namespace ts
