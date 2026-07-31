# M3-T0 spike: variant-of-arena-offsets vs. tagged-tree `Atom` representation

Time-boxed spike per `symbolica-cpp-port-tasks.yaml`'s `M3-T0`. Not
production code — throwaway prototypes live in
`benchmarks/atom_core_spike/spike_bench.cc`, standalone, not wired into the
main CMake build. This document is the artifact M3-T1 must reference before
implementing the real `Atom`/`AtomView` types.

## Question

Should `Atom` storage be:

- **A (offsets)**: nodes live in a single contiguous arena (`Workspace`,
  M2-T6); a composite node's children are indices/offsets into that arena
  (or into a CSR-style child-index pool), never raw pointers.
- **B (tagged tree)**: nodes are individually heap-allocated
  (`std::unique_ptr`-owned), a composite node's children are raw/smart
  pointers scattered across the heap in allocation order.

## Method

Two synthetic workloads, built and traversed identically for both
representations:

- **deep**: a right-leaning chain of 20,000 nested binary `Add` nodes
  (`Add(leaf, Add(leaf, Add(leaf, ...)))`) — stresses pointer-chasing depth
  and, for representation B, recursive-destructor stack depth.
- **wide**: a single `Add` node with 200,000 `Leaf` children — stresses
  large contiguous-vs-scattered child arrays.

For each, three things were measured (`std::chrono::steady_clock`, `-O2`,
both GCC and Clang, macOS native): (1) construction + destruction cost, (2)
repeated post-construction traversal cost (sum of all leaf values), (3) same
for the wide case. Numbers below are Clang; GCC was materially identical
(see raw run logs in the PR).

## Results

```
deep  offsets build+destroy: 23.5 ms total / 20 cycles = 1.18 ms/cycle
deep  tree    build+destroy: 78.5 ms total / 20 cycles = 3.92 ms/cycle
deep  offsets traversal    : 0.184 ms total / 50 = 0.0037 ms/traversal
deep  tree    traversal    : 0.181 ms total / 50 = 0.0036 ms/traversal
wide  offsets traversal    : 0.861 ms total / 50 = 0.0172 ms/traversal
wide  tree    traversal    : 0.888 ms total / 50 = 0.0178 ms/traversal
```

- **Traversal**: no meaningful difference between A and B at this scale on
  either compiler — both workloads' working sets are small enough (a few MB)
  that the traversal itself doesn't clearly separate the two on wall-clock
  time alone. This is a real, if modest, result, not an artifact worth
  re-running longer given the time box.
- **Construction + destruction**: representation A is **~3.3x faster**
  end-to-end for the deep workload, because it does one bulk allocation
  (bump-allocator arena) and no per-node `delete`, versus B's 20,001
  individual `new`/`delete` calls per cycle plus a recursive destructor
  chain.
- **Stack safety**: representation B's destructor chain for the deep
  workload recurses to a depth proportional to expression nesting depth
  (20,000 here). This is a latent stack-overflow risk for pathologically
  deep expressions that A does not share — A's "destructor" is just
  `Workspace::reset()`, O(1) in call depth.

## Recommendation

**Adopt representation A: variant-of-arena-offsets, backed by M2-T6's
`Workspace`.**

Traversal speed alone doesn't decide this at the tested scale, but every
other axis favors A and none favor B:

- Matches roadmap.md's guiding principle 4 ("arena/interning over
  `shared_ptr` soup") and reuses the already-built, already-ASan-fuzzed
  `Workspace` allocator from M2-T6 directly — no new allocator needed.
- Construction/destruction is the dominant cost in real CAS workloads
  (expressions are built and torn down constantly during
  simplification/rewriting), and A wins there by >3x.
- No recursive-destructor stack-depth risk for deep expressions — directly
  relevant since `pattern_core`/`calculus_core` will generate deeply nested
  intermediate expressions during rewriting.
- Offsets (not pointers) are exactly what a structural-hash-keyed
  hash-consing/interning table (M2-T6's deferred-to-M3 note) needs to key
  and deduplicate on: two structurally identical subexpressions intern to
  the same arena slot by construction, which is far more natural with
  index-based identity than with per-node heap addresses.

M3-T1 (`Atom` representation + `AtomView`) should implement `Atom` as a
tagged variant whose composite-node children are `Workspace`-arena offsets
(via a CSR-style child-index pool, as prototyped in `spike_bench.cc`'s
`offsets_repr` namespace — not the incorrect "contiguous siblings" shortcut
that was tried and rejected during this spike, see the PR history), not raw
or smart pointers.
