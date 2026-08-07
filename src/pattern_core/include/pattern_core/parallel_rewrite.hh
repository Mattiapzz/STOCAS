#pragma once

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <atom_core/atom.hh>
#include <pattern_core/rewrite.hh>

/// @file
/// @brief Parallel rewriting over a "term stream" (M5-T4, the milestone's
/// last subtask): applies replace_all() to a list of independent terms
/// concurrently across a worker-thread pool.
///
/// Thread-safety design: `atom_core::AtomStore` has no internal
/// synchronization - concurrent calls into the *same* store from multiple
/// threads would be a data race. Rather than adding locking (which would
/// serialize every store mutation and only let the read-only matching step
/// run in parallel), every term gets its own private `AtomStore`: the term
/// is first structurally copied out of whatever store it originally
/// belonged to, then replace_all() runs entirely within that private
/// store. No two threads ever touch the same `AtomStore`, so the result is
/// safe *by construction*, not merely "believed safe" - the only
/// genuinely shared state across threads is `rule.pattern`/
/// `rule.replacement`'s own store(s), which every thread only *reads*
/// (match()/substitute() never mutate the pattern/replacement's store),
/// and concurrent reads of unsynchronized memory are not a data race in
/// C++'s memory model. See test_parallel_rewrite.cc's TSan-checked test
/// (run as part of the existing blocking `linux-tsan` CI job, which runs
/// the full suite - see ci.yml) for the proof this holds.
namespace pattern_core {

/// @brief One term's rewrite result: the private store it was rewritten
/// in, and the rewritten atom (which belongs to that store). `result` is
/// `std::optional` only to allow default-constructing the results vector
/// up front (`atom_core::Atom` has no default constructor); every element
/// returned by parallel_replace_all() has `result` populated.
struct ParallelRewriteResult {
  std::unique_ptr<atom_core::AtomStore> store;
  std::optional<atom_core::Atom> result;
};

/// @brief Rewrites every term in @p terms independently, applying
/// `replace_all(rule, term)` to each, spread across @p num_threads worker
/// threads (terms are statically partitioned into contiguous chunks, one
/// chunk per thread - term rewriting has no cross-term dependencies, so
/// this is an embarrassingly parallel workload). Each input term may
/// belong to any `AtomStore` (it's copied into its own fresh, private
/// store before rewriting).
/// @param rule The rule to apply to every term; read-only for the
///        duration of this call.
/// @param terms The independent terms to rewrite.
/// @param num_threads Worker thread count; clamped to at least 1 and at
///        most `terms.size()` (no point spawning idle threads).
/// @return One result per input term, in input order.
[[nodiscard]] std::vector<ParallelRewriteResult> parallel_replace_all(
    const Rule& rule, std::span<const atom_core::AtomView> terms, unsigned int num_threads);

} // namespace pattern_core
