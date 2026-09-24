#pragma once

#include <cstdint>

// Sidecar files under /.crossxtch keyed by FNV-1a of the book path.
// p_*.bin is reading progress. a_*/t_*/c_*.bin are leftover typesetter files.
// removeFor deletes them when a book is removed.
namespace BookCache {

uint32_t key(const char* path);
void removeFor(const char* bookPath);

}  // namespace BookCache
