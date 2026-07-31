// M3-T0 spike: variant-of-arena-offsets vs tagged-tree representation for
// Atom storage. Throwaway prototype code, not linked into any shipped
// library or wired into the main CMake build (see benchmarks/atom_core_spike
// README / docs/atom-representation-spike.md) - built standalone via the
// run_spike.sh script.
//
// Both prototypes represent the same minimal expression shape: a node is
// either a Leaf (numeric value) or an N-ary Op (Add/Mul) with children.
//
// Prototype A (offsets): every node lives in one contiguous
// std::vector<Node>, "pointers" to children are indices (offsets) into that
// vector. Traversal never leaves the vector's backing storage.
//
// Prototype B (tagged tree): every node is individually allocated (new),
// children are raw pointers scattered across the heap in allocation order,
// which for a workload built depth-first interleaved with siblings is
// representative of the fragmentation a plain new-per-node tree exhibits in
// practice.
//
// Two synthetic workloads: "deep" (long right-leaning chain of binary Add
// nodes) and "wide" (a shallow Add node with many thousands of Leaf
// children), each traversed repeatedly summing all Leaf values.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <vector>

namespace offsets_repr {

enum class Tag : std::uint8_t { Leaf, Add, Mul };

struct Node {
  Tag tag;
  double value = 0.0;            // valid iff tag == Leaf
  std::uint32_t child_start = 0; // valid iff tag != Leaf: index into child_pool
  std::uint32_t child_count = 0;
};

// A node's children are arena node-indices (offsets), not pointers. They are
// NOT required to be contiguous with their parent or with each other in
// `nodes` (arbitrary construction order, e.g. deep recursive builds, makes
// that impossible in general) - only the child-index *entries themselves*
// are contiguous, in a separate CSR-style pool, since each node's entries
// are always pushed together at construction time.
struct Arena {
  std::vector<Node> nodes;
  std::vector<std::uint32_t> child_pool;
};

std::uint32_t make_leaf(Arena& arena, double value) {
  arena.nodes.push_back(Node{Tag::Leaf, value, 0, 0});
  return static_cast<std::uint32_t>(arena.nodes.size() - 1);
}

std::uint32_t make_op(Arena& arena, Tag tag, const std::vector<std::uint32_t>& children) {
  std::uint32_t start = static_cast<std::uint32_t>(arena.child_pool.size());
  arena.child_pool.insert(arena.child_pool.end(), children.begin(), children.end());
  arena.nodes.push_back(Node{tag, 0.0, start, static_cast<std::uint32_t>(children.size())});
  return static_cast<std::uint32_t>(arena.nodes.size() - 1);
}

double sum_leaves(const Arena& arena, std::uint32_t index) {
  const Node& node = arena.nodes[index];
  if (node.tag == Tag::Leaf) {
    return node.value;
  }
  double total = 0.0;
  for (std::uint32_t i = 0; i < node.child_count; ++i) {
    total += sum_leaves(arena, arena.child_pool[node.child_start + i]);
  }
  return total;
}

// Builds a right-leaning chain: Add(leaf, Add(leaf, Add(leaf, ...))).
std::uint32_t build_deep(Arena& arena, int depth) {
  if (depth == 0) {
    return make_leaf(arena, 1.0);
  }
  std::uint32_t leaf = make_leaf(arena, 1.0);
  std::uint32_t rest = build_deep(arena, depth - 1);
  return make_op(arena, Tag::Add, {leaf, rest});
}

std::uint32_t build_wide(Arena& arena, int width) {
  std::vector<std::uint32_t> children;
  children.reserve(static_cast<std::size_t>(width));
  for (int i = 0; i < width; ++i) {
    children.push_back(make_leaf(arena, 1.0));
  }
  return make_op(arena, Tag::Add, children);
}

} // namespace offsets_repr

namespace tree_repr {

enum class Tag : std::uint8_t { Leaf, Add, Mul };

struct Node {
  Tag tag;
  double value = 0.0;
  std::vector<std::unique_ptr<Node>> children;
};

std::unique_ptr<Node> make_leaf(double value) {
  auto node = std::make_unique<Node>();
  node->tag = Tag::Leaf;
  node->value = value;
  return node;
}

double sum_leaves(const Node& node) {
  if (node.tag == Tag::Leaf) {
    return node.value;
  }
  double total = 0.0;
  for (const auto& child : node.children) {
    total += sum_leaves(*child);
  }
  return total;
}

std::unique_ptr<Node> build_deep(int depth) {
  if (depth == 0) {
    return make_leaf(1.0);
  }
  auto node = std::make_unique<Node>();
  node->tag = Tag::Add;
  node->children.push_back(make_leaf(1.0));
  node->children.push_back(build_deep(depth - 1));
  return node;
}

std::unique_ptr<Node> build_wide(int width) {
  auto node = std::make_unique<Node>();
  node->tag = Tag::Add;
  for (int i = 0; i < width; ++i) {
    node->children.push_back(make_leaf(1.0));
  }
  return node;
}

} // namespace tree_repr

namespace {

template <typename Fn> double time_ms(Fn&& fn, int repeats) {
  using clock = std::chrono::steady_clock;
  auto start = clock::now();
  double sink = 0.0;
  for (int i = 0; i < repeats; ++i) {
    sink += fn();
  }
  auto end = clock::now();
  // Prevent the loop from being optimized away.
  if (sink == -1.0) {
    std::fprintf(stderr, "unreachable\n");
  }
  return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

int main() {
  constexpr int kDeepDepth = 20000;
  constexpr int kWideWidth = 200000;
  constexpr int kTraversalRepeats = 50;
  constexpr int kBuildRepeats = 20;

  // --- construction + destruction cost ---
  {
    double ms = time_ms(
        [&] {
          offsets_repr::Arena arena;
          std::uint32_t root = offsets_repr::build_deep(arena, kDeepDepth);
          return static_cast<double>(root);
        },
        kBuildRepeats);
    std::printf(
        "deep  offsets build+destroy: %.3f ms total, %.4f ms/cycle\n", ms, ms / kBuildRepeats);
  }
  {
    double ms = time_ms(
        [&] {
          auto root = tree_repr::build_deep(kDeepDepth);
          return static_cast<double>(root != nullptr);
        },
        kBuildRepeats);
    std::printf("deep  tree    build+destroy: %.3f ms total, %.4f ms/cycle "
                "(recursive unique_ptr destructor chain - risks stack depth "
                "proportional to expression depth)\n",
                ms,
                ms / kBuildRepeats);
  }

  // --- deep workload ---
  {
    offsets_repr::Arena arena;
    arena.nodes.reserve(static_cast<std::size_t>(kDeepDepth) * 2 + 2);
    arena.child_pool.reserve(static_cast<std::size_t>(kDeepDepth) * 2 + 2);
    std::uint32_t root = offsets_repr::build_deep(arena, kDeepDepth);
    double ms = time_ms([&] { return offsets_repr::sum_leaves(arena, root); }, kTraversalRepeats);
    std::printf("deep  offsets : %.3f ms total, %.4f ms/traversal\n", ms, ms / kTraversalRepeats);
  }
  {
    auto root = tree_repr::build_deep(kDeepDepth);
    double ms = time_ms([&] { return tree_repr::sum_leaves(*root); }, kTraversalRepeats);
    std::printf("deep  tree    : %.3f ms total, %.4f ms/traversal\n", ms, ms / kTraversalRepeats);
  }

  // --- wide workload ---
  {
    offsets_repr::Arena arena;
    arena.nodes.reserve(static_cast<std::size_t>(kWideWidth) + 2);
    arena.child_pool.reserve(static_cast<std::size_t>(kWideWidth) + 2);
    std::uint32_t root = offsets_repr::build_wide(arena, kWideWidth);
    double ms = time_ms([&] { return offsets_repr::sum_leaves(arena, root); }, kTraversalRepeats);
    std::printf("wide  offsets : %.3f ms total, %.4f ms/traversal\n", ms, ms / kTraversalRepeats);
  }
  {
    auto root = tree_repr::build_wide(kWideWidth);
    double ms = time_ms([&] { return tree_repr::sum_leaves(*root); }, kTraversalRepeats);
    std::printf("wide  tree    : %.3f ms total, %.4f ms/traversal\n", ms, ms / kTraversalRepeats);
  }

  return 0;
}
