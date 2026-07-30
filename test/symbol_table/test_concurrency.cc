#include <atomic>
#include <cstddef>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using symbol_table::get_symbol;
using symbol_table::Symbol;

namespace {

// Spins until `start` flips, so every thread's registration/lookup work
// begins as close to simultaneously as possible - maximizing the chance
// TSan actually observes the races these tests exist to rule out.
void wait_for_start(const std::atomic<bool>& start) {
  while (!start.load(std::memory_order_acquire)) {
  }
}

} // namespace

// This is THE test for M2-T4's concurrency contract (symbol.hh's Doxygen
// comment): a green run proves nothing here, only a green *TSan* report
// does - hence this file is a blocking CI leg specifically on the
// sanitizers-tsan job (see ci.yml), not opt-in like the other sanitizer
// legs.

TEST_CASE("Concurrent registration races for one new name yield exactly one consistent handle",
          "[symbol_table][symbol][concurrency]") {
  constexpr int kRacers = 16;

  std::vector<std::optional<Symbol>> results(kRacers, std::nullopt);
  std::atomic<bool> start{false};
  std::vector<std::thread> threads;
  threads.reserve(kRacers);

  for (int t = 0; t < kRacers; ++t) {
    threads.emplace_back([&, t] {
      wait_for_start(start);
      results[static_cast<std::size_t>(t)] = get_symbol("test_ns_concurrency_race", "contested");
    });
  }
  start.store(true, std::memory_order_release);
  for (std::thread& thread : threads) {
    thread.join();
  }

  for (const std::optional<Symbol>& result : results) {
    REQUIRE(result.has_value());
    REQUIRE(*result == *results[0]);
  }
}

TEST_CASE("Concurrent distinct registrations never collide, alongside concurrent lookups of an "
          "already-registered symbol",
          "[symbol_table][symbol][concurrency]") {
  const Symbol preexisting = get_symbol("test_ns_concurrency_mixed", "preexisting");

  constexpr int kWriterThreads = 8;
  constexpr int kSymbolsPerWriter = 200;
  constexpr int kReaderThreads = 8;
  constexpr int kLookupsPerReader = 500;

  std::atomic<bool> start{false};
  std::vector<std::thread> threads;
  std::vector<std::vector<Symbol>> writer_results(kWriterThreads);
  // Deliberately not std::vector<bool>: its bit-packed storage means
  // concurrent writes to *different* indices by different threads race at
  // the byte level - a real TSan-flagged data race that has nothing to do
  // with symbol_table's own concurrency contract under test here.
  std::vector<int> reader_saw_only_preexisting(kReaderThreads, 0);

  for (int t = 0; t < kWriterThreads; ++t) {
    threads.emplace_back([&, t] {
      wait_for_start(start);
      std::vector<Symbol>& results = writer_results[static_cast<std::size_t>(t)];
      results.reserve(kSymbolsPerWriter);
      for (int i = 0; i < kSymbolsPerWriter; ++i) {
        results.push_back(get_symbol("test_ns_concurrency_mixed",
                                     "writer_" + std::to_string(t) + "_" + std::to_string(i)));
      }
    });
  }
  for (int t = 0; t < kReaderThreads; ++t) {
    threads.emplace_back([&, t] {
      wait_for_start(start);
      bool consistent = true;
      for (int i = 0; i < kLookupsPerReader; ++i) {
        consistent =
            consistent && (get_symbol("test_ns_concurrency_mixed", "preexisting") == preexisting);
      }
      reader_saw_only_preexisting[static_cast<std::size_t>(t)] = consistent ? 1 : 0;
    });
  }

  start.store(true, std::memory_order_release);
  for (std::thread& thread : threads) {
    thread.join();
  }

  for (int consistent : reader_saw_only_preexisting) {
    REQUIRE(consistent != 0);
  }

  std::unordered_set<std::size_t> seen_ids;
  for (const std::vector<Symbol>& results : writer_results) {
    REQUIRE(results.size() == kSymbolsPerWriter);
    for (const Symbol& symbol : results) {
      REQUIRE(seen_ids.insert(symbol.id()).second); // false => a ID collision (a real bug)
    }
  }
}
