#include <algorithm>
#include <cstddef>

#include <symbol_table/workspace.hh>

// Portable ASan detection (see symbol_table.cc's identical block) - used to
// poison/unpoison memory across reset()/allocate() so a stale pointer used
// after reset() is caught immediately instead of silently reading reused
// memory.
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define STOCAS_WORKSPACE_HAVE_ASAN 1
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define STOCAS_WORKSPACE_HAVE_ASAN 1
#endif

#ifdef STOCAS_WORKSPACE_HAVE_ASAN
#include <sanitizer/asan_interface.h>
#endif

namespace symbol_table {

namespace {

constexpr std::size_t kChunkSize = 64 * 1024;

std::size_t align_up(std::size_t offset, std::size_t alignment) {
  return (offset + alignment - 1) & ~(alignment - 1);
}

} // namespace

Workspace& Workspace::current() noexcept {
  thread_local Workspace instance;
  return instance;
}

Workspace::Workspace() = default;
Workspace::~Workspace() = default;

Workspace::Chunk& Workspace::current_chunk_for(std::size_t size, std::size_t alignment) {
  if (active_chunk_ < chunks_.size()) {
    Chunk& chunk = chunks_[active_chunk_];
    const std::size_t aligned = align_up(chunk.used, alignment);
    if (aligned + size <= chunk.capacity) {
      return chunk;
    }
    ++active_chunk_;
  }

  if (active_chunk_ < chunks_.size()) {
    return chunks_[active_chunk_];
  }

  const std::size_t capacity = std::max(kChunkSize, size + alignment);
  Chunk chunk;
  chunk.data = std::make_unique<std::byte[]>(capacity);
  chunk.capacity = capacity;
  chunk.used = 0;
#ifdef STOCAS_WORKSPACE_HAVE_ASAN
  // Freshly allocated memory starts fully addressable; poison it up front so
  // the invariant "unused arena memory is poisoned" holds uniformly, mirrored
  // by unpoisoning exactly the slice handed out below.
  __asan_poison_memory_region(chunk.data.get(), chunk.capacity);
#endif
  chunks_.push_back(std::move(chunk));
  return chunks_.back();
}

void* Workspace::allocate(std::size_t size, std::size_t alignment) {
  Chunk& chunk = current_chunk_for(size, alignment);
  const std::size_t aligned = align_up(chunk.used, alignment);
  std::byte* result = chunk.data.get() + aligned;
  chunk.used = aligned + size;
#ifdef STOCAS_WORKSPACE_HAVE_ASAN
  __asan_unpoison_memory_region(result, size);
#endif
  return result;
}

void Workspace::reset() noexcept {
  for (Chunk& chunk : chunks_) {
#ifdef STOCAS_WORKSPACE_HAVE_ASAN
    __asan_poison_memory_region(chunk.data.get(), chunk.capacity);
#endif
    chunk.used = 0;
  }
  active_chunk_ = 0;
}

} // namespace symbol_table
