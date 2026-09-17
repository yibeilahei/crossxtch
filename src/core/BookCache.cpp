#include "core/BookCache.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/Settings.h"

namespace BookCache {

uint32_t key(const char* path) {
  uint32_t h = 2166136261u;
  if (!path) {
    return h;
  }
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(path); *p; ++p) {
    h ^= *p;
    h *= 16777619u;
  }
  return h;
}

void removeFor(const char* bookPath) {
  if (!bookPath || bookPath[0] == '\0') {
    return;
  }
  const uint32_t h = key(bookPath);
  char p[64];
  auto drop = [&](const char* prefix) {
    snprintf(p, sizeof(p), "%s/%s_%08lx.bin", Settings::kDir, prefix, static_cast<unsigned long>(h));
    if (Storage.exists(p)) {
      Storage.remove(p);
      LOG_INF("CACHE", "Removed %s", p);
    }
  };
  drop("a");
  drop("t");
  drop("c");
  drop("p");
  if (settings.lastBookPath[0] && strcmp(settings.lastBookPath, bookPath) == 0) {
    settings.lastBookPath[0] = '\0';
    settings.save();
  }
}

unsigned clearAll() {
  HalFile dir = Storage.open(Settings::kDir);
  if (!dir || !dir.isDirectory()) {
    return 0;
  }
  std::vector<std::string> doomed;
  char name[HalFile::kMaxNameBytes];
  for (HalFile file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory() || file.getName(name, sizeof(name)) == 0) {
      continue;
    }
    const bool sidecar = (name[0] == 'a' || name[0] == 't' || name[0] == 'c' || name[0] == 'p') &&
                         name[1] == '_' && strstr(name, ".bin") != nullptr;
    if (sidecar || strcmp(name, "work.xhtml") == 0) {
      doomed.emplace_back(name);
    }
  }
  dir = HalFile();
  unsigned n = 0;
  char full[96];
  for (const auto& leaf : doomed) {
    snprintf(full, sizeof(full), "%s/%s", Settings::kDir, leaf.c_str());
    if (Storage.remove(full)) {
      ++n;
      LOG_INF("CACHE", "Cleared %s", full);
    }
  }
  LOG_INF("CACHE", "Cleared %u files", n);
  return n;
}

}  // namespace BookCache
