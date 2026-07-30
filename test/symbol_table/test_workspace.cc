#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include <symbol_table/workspace.hh>

#include <catch2/catch_test_macros.hpp>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define STOCAS_WORKSPACE_TEST_HAVE_ASAN 1
#endif
#elif defined(__SANITIZE_ADDRESS__)
#define STOCAS_WORKSPACE_TEST_HAVE_ASAN 1
#endif

#ifdef STOCAS_WORKSPACE_TEST_HAVE_ASAN
#include <sanitizer/asan_interface.h>
#endif

using symbol_table::Workspace;

TEST_CASE("Workspace::allocate returns distinct, correctly aligned regions",
          "[symbol_table][workspace]") {
  Workspace& workspace = Workspace::current();
  workspace.reset();

  void* first = workspace.allocate(16, 8);
  void* second = workspace.allocate(32, 16);
  REQUIRE(first != second);
  REQUIRE(reinterpret_cast<std::uintptr_t>(first) % 8 == 0);
  REQUIRE(reinterpret_cast<std::uintptr_t>(second) % 16 == 0);
}

TEST_CASE("Workspace::create placement-constructs and reset() reuses the memory",
          "[symbol_table][workspace]") {
  Workspace& workspace = Workspace::current();
  workspace.reset();

  struct Point {
    int x;
    int y;
  };
  Point* p = workspace.create<Point>(Point{3, 4});
  REQUIRE(p->x == 3);
  REQUIRE(p->y == 4);

  workspace.reset();
  Point* q = workspace.create<Point>(Point{5, 6});
  REQUIRE(q->x == 5);
  REQUIRE(q->y == 6);
}

TEST_CASE("Workspace grows across multiple chunks without corrupting earlier allocations",
          "[symbol_table][workspace]") {
  Workspace& workspace = Workspace::current();
  workspace.reset();

  constexpr int kCount = 20000;
  std::vector<int*> values;
  values.reserve(kCount);
  for (int i = 0; i < kCount; ++i) {
    int* value = workspace.create<int>(i);
    values.push_back(value);
  }
  for (int i = 0; i < kCount; ++i) {
    REQUIRE(*values[static_cast<std::size_t>(i)] == i);
  }
  workspace.reset();
}

#ifdef STOCAS_WORKSPACE_TEST_HAVE_ASAN
TEST_CASE("Workspace::reset() poisons the rewound region under ASan",
          "[symbol_table][workspace][asan]") {
  Workspace& workspace = Workspace::current();
  workspace.reset();

  auto* value = static_cast<unsigned char*>(workspace.allocate(64, 1));
  // Freshly handed-out memory must be addressable (not poisoned).
  REQUIRE(__asan_region_is_poisoned(value, 64) == nullptr);

  workspace.reset();
  // The exact same bytes, after reset(), must be poisoned - this is the
  // mechanism that turns a use-after-reset bug into an immediate ASan
  // report instead of silent corruption (see workspace.hh's contract).
  REQUIRE(__asan_region_is_poisoned(value, 64) != nullptr);
}
#endif

// This is THE test for M2-T6's exit criteria: a fixed CPU-time/iteration
// budget of randomized allocate/create/reset cycles, ASan-clean. A green
// run here (under linux-asan-ubsan / macos-asan-ubsan) is the only real
// evidence the poisoning contract holds across realistic usage patterns,
// not just the single-shot cases above.
TEST_CASE("Fuzz: randomized allocate/reset cycles stay ASan-clean",
          "[symbol_table][workspace][fuzz]") {
  Workspace& workspace = Workspace::current();
  workspace.reset();

  std::mt19937 rng(1234567u);
  std::uniform_int_distribution<int> size_dist(1, 512);
  std::uniform_int_distribution<int> align_dist(0, 4);
  std::uniform_int_distribution<int> reset_dist(0, 49);
  constexpr std::size_t kAlignments[] = {1, 2, 4, 8, 16};

  constexpr int kIterations = 5000;
  std::vector<std::pair<unsigned char*, std::size_t>> live;
  for (int i = 0; i < kIterations; ++i) {
    const std::size_t size = static_cast<std::size_t>(size_dist(rng));
    const std::size_t alignment = kAlignments[static_cast<std::size_t>(align_dist(rng))];
    auto* memory = static_cast<unsigned char*>(workspace.allocate(size, alignment));
    memory[0] = 0xAB;
    memory[size - 1] = 0xCD;
    live.emplace_back(memory, size);

    if (reset_dist(rng) == 0) {
      workspace.reset();
      live.clear();
    }
  }
  // Touch everything still considered live to confirm it's genuinely
  // unpoisoned (would abort under ASan otherwise).
  for (auto& [memory, size] : live) {
    REQUIRE(memory[0] == 0xAB);
    REQUIRE(memory[size - 1] == 0xCD);
  }
  workspace.reset();
}
