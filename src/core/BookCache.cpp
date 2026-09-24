#include "core/BookCache.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

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

}  // namespace BookCache
