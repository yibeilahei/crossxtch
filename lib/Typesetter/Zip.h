#pragma once

#include <HalStorage.h>

#include <cstddef>
#include <cstdint>

namespace ts {

class ZipArchive {
 public:
  static constexpr uint16_t kMaxEntries = 192;

  struct Entry {
    char name[96]{};
    uint32_t localOff = 0;
    uint32_t compSize = 0;
    uint32_t uncompSize = 0;
    uint16_t method = 0;
  };

  ZipArchive() = default;
  ~ZipArchive() { close(); }
  ZipArchive(const ZipArchive&) = delete;
  ZipArchive& operator=(const ZipArchive&) = delete;

  bool open(const char* path);
  void close();
  const Entry* find(const char* name) const;
  const Entry* findSuffix(const char* suffix) const;

  // Inflate into malloc'd buffer (caller free()). nullptr on failure.
  uint8_t* extract(const Entry& e, size_t* outLen);
  bool extractToFile(const Entry& e, const char* destPath);

  uint16_t count() const { return n; }
  const Entry& at(uint16_t i) const { return entries[i]; }
  const char* lastError() const { return error; }

 private:
  HalFile file;
  Entry* entries = nullptr;
  uint16_t n = 0;
  const char* error = "closed";
  uint32_t fileSize = 0;

  bool parseCentral();
  bool dataStart(const Entry& e, uint32_t& off);
};

}  // namespace ts
