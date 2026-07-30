#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <utility>
#include <vector>

/// @file
/// @brief Thread-local bump-allocator arena (roadmap.md guiding principle 4:
/// "arena/interning over `shared_ptr` soup") backing later `Atom` storage
/// (M3). Reset between top-level operations instead of freeing piecemeal.

namespace symbol_table {

/// @brief A per-thread bump allocator: `allocate()` hands out
/// monotonically-increasing offsets into a growing set of chunks, and
/// `reset()` rewinds all of them at once rather than freeing individually.
///
/// Ownership/lifetime contract: `reset()` does **not** run destructors on
/// anything previously allocated - only trivially-destructible data (or data
/// that owns no external resources) may be placed here, mirroring the
/// bump-allocator convention this class exists to provide (roadmap.md
/// guiding principle 4). Callers needing destructors run must do so
/// themselves before calling `reset()`.
///
/// Concurrency: `current()` returns a distinct instance per thread
/// (`thread_local`) - there is no cross-thread state and therefore nothing
/// to lock, unlike `symbol_table`'s process-wide global table.
///
/// Use-after-reset is a real bug class this class exists to catch, not just
/// avoid: under AddressSanitizer, `reset()` poisons every byte it rewinds
/// past, and `allocate()` unpoisons only the slice it hands out next, so a
/// stale pointer obtained before a `reset()` call and dereferenced after it
/// triggers an immediate ASan use-after-poison report instead of silently
/// reading/corrupting reused memory. See test_workspace.cc's ASan-checked
/// fuzz test (M2-T6's blocking CI leg).
class Workspace {
public:
  /// @return This thread's Workspace instance (created on first use).
  [[nodiscard]] static Workspace& current() noexcept;

  Workspace();
  ~Workspace();
  Workspace(const Workspace&) = delete;
  Workspace& operator=(const Workspace&) = delete;

  /// @brief Allocates @p size bytes aligned to @p alignment out of the
  /// current chunk, growing the arena with a new chunk if necessary.
  /// @return Never null.
  [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment);

  /// @brief Convenience wrapper: allocates storage for a `T` and
  /// placement-`new`-constructs it in place.
  /// @return A pointer to the constructed object, alive until the next
  ///         `reset()` (see the class-level ownership contract - `T`'s
  ///         destructor is never invoked by this class).
  template <typename T, typename... Args> [[nodiscard]] T* create(Args&&... args) {
    void* memory = allocate(sizeof(T), alignof(T));
    return ::new (memory) T(std::forward<Args>(args)...);
  }

  /// @brief Rewinds every chunk to empty. Does not run destructors (see the
  /// class-level ownership contract) and does not release chunk memory back
  /// to the OS - chunks are reused by subsequent `allocate()` calls.
  void reset() noexcept;

private:
  struct Chunk {
    std::unique_ptr<std::byte[]> data;
    std::size_t capacity = 0;
    std::size_t used = 0;
  };

  Chunk& current_chunk_for(std::size_t size, std::size_t alignment);

  std::vector<Chunk> chunks_;
  std::size_t active_chunk_ = 0;
};

} // namespace symbol_table
