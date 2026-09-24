#pragma once

#include <cstdint>

// Sidecar files under /.crossxtch keyed by FNV-1a of the book path.
// p_*.bin is reading progress. a_*/t_*/c_*.bin and work.xhtml are leftover
// typesetter files; clear still deletes them.
namespace BookCache {

uint32_t key(const char* path);
void removeFor(const char* bookPath);
// Drops a_*/t_*/c_*/p_*.bin and work.xhtml. Keeps settings.bin and fonts/.
unsigned clearAll();

}  // namespace BookCache
