#pragma once

#include "AtomFile.h"
#include "HtmlIR.h"
#include "Zip.h"

#include <cstdint>

namespace ts {

class EpubBook {
 public:
  static constexpr uint16_t kMaxSpine = 192;

  EpubBook() = default;
  ~EpubBook() { close(); }
  EpubBook(const EpubBook&) = delete;
  EpubBook& operator=(const EpubBook&) = delete;

  using ProgressFn = void (*)(void* ctx, uint16_t done, uint16_t total);

  bool open(const char* epubPath, const char* atomPath, ProgressFn progress = nullptr, void* progressCtx = nullptr);
  void close();

  const char* title() const { return bookTitle; }
  const char* author() const { return bookAuthor; }
  WritingMode bookMode() const { return mode; }
  uint16_t spineCount() const { return nSpine; }
  const char* lastError() const { return error; }

  struct Spine {
    char href[96]{};
    char title[64]{};
    char type[40]{};
    uint8_t compact = 0;
    uint8_t hasMode = 0;
    WritingMode mode = WritingMode::VerticalRl;
    uint32_t atomOff = 0;
    uint16_t chCount = 0;
  };
  const Spine& spine(uint16_t i) const { return items[i]; }

  struct TocEntry {
    char title[80]{};
    char href[96]{};
    uint32_t atomOff = 0xFFFFFFFFu;
  };
  uint16_t tocCount() const { return nToc; }
  const TocEntry& toc(uint16_t i) const { return tocs[i]; }

 private:
  ZipArchive zip;
  Spine* items = nullptr;
  TocEntry* tocs = nullptr;
  uint16_t nSpine = 0;
  uint16_t nToc = 0;
  uint16_t tocCap = 0;
  char bookTitle[128]{};
  char bookAuthor[64]{};
  char ncxHref[96]{};
  char navHref[96]{};
  WritingMode mode = WritingMode::VerticalRl;
  const char* error = "closed";

  bool parseContainer(char* opfName, size_t cap);
  bool parseOpf(const char* xml, size_t n);
  bool parseNcx(const char* xml, size_t n);
  bool parseNavDoc(const char* xml, size_t n);
  bool loadTocDoc(const char* opfDir);
  void addToc(const char* title, const char* href);
  void bindTocToSpine();
};

}  // namespace ts
