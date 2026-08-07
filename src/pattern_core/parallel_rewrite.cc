#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <vector>

#include <pattern_core/parallel_rewrite.hh>

namespace pattern_core {

namespace {

using atom_core::Atom;
using atom_core::AtomStore;
using atom_core::AtomTag;
using atom_core::AtomView;

// Structurally copies `node` (which may belong to any store) into `store`,
// so a term originally interned elsewhere can be rewritten inside a fresh,
// private store without any cross-thread sharing.
Atom clone_into(AtomStore& store, AtomView node) {
  switch (node.tag()) {
    case AtomTag::Num:
      return store.num(node.as_num());
    case AtomTag::Var:
      return store.var(node.as_var());
    case AtomTag::Pow: {
      Atom base = clone_into(store, node.child(0));
      Atom exponent = clone_into(store, node.child(1));
      return store.pow(base, exponent);
    }
    case AtomTag::Add:
    case AtomTag::Mul:
    case AtomTag::Fun: {
      std::vector<Atom> children;
      children.reserve(node.child_count());
      for (std::size_t i = 0; i < node.child_count(); ++i) {
        children.push_back(clone_into(store, node.child(i)));
      }
      if (node.tag() == AtomTag::Fun) {
        return store.fun(node.function_head(), children);
      }
      return node.tag() == AtomTag::Add ? store.add(children) : store.mul(children);
    }
  }
  throw std::logic_error("pattern_core::clone_into: unreachable AtomTag");
}

ParallelRewriteResult rewrite_one(const Rule& rule, AtomView term) {
  ParallelRewriteResult result;
  result.store = std::make_unique<AtomStore>();
  Atom local_term = clone_into(*result.store, term);
  result.result.emplace(replace_all(*result.store, rule, local_term.view()));
  return result;
}

} // namespace

std::vector<ParallelRewriteResult>
parallel_replace_all(const Rule& rule, std::span<const AtomView> terms, unsigned int num_threads) {
  std::vector<ParallelRewriteResult> results(terms.size());
  if (terms.empty()) {
    return results;
  }

  unsigned int worker_count =
      std::max(1u, std::min(num_threads, static_cast<unsigned int>(terms.size())));
  std::size_t chunk_size = (terms.size() + worker_count - 1) / worker_count;

  std::vector<std::thread> workers;
  workers.reserve(worker_count);
  for (unsigned int w = 0; w < worker_count; ++w) {
    std::size_t begin = static_cast<std::size_t>(w) * chunk_size;
    std::size_t end = std::min(terms.size(), begin + chunk_size);
    if (begin >= end) {
      continue;
    }
    // Each worker only ever writes to results[begin, end) - disjoint index
    // ranges across threads - and only reads `rule`/`terms`, never
    // mutating them, so this has no data race (see parallel_rewrite.hh's
    // file comment for the full argument).
    workers.emplace_back([&rule, &terms, &results, begin, end]() {
      for (std::size_t i = begin; i < end; ++i) {
        results[i] = rewrite_one(rule, terms[i]);
      }
    });
  }
  for (std::thread& worker : workers) {
    worker.join();
  }
  return results;
}

} // namespace pattern_core
