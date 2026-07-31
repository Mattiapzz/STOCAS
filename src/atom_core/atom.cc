#include <algorithm>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include <atom_core/atom.hh>

namespace atom_core {

namespace {

// A node's children (Add/Mul/Pow/Fun) are indices into the store's
// child_pool, contiguous per-node (CSR-style), never pointers - the
// production form of benchmarks/atom_core_spike/spike_bench.cc's
// offsets_repr prototype (see docs/atom-representation-spike.md).
struct Node {
  AtomTag tag;
  numerica_core::Rational num_value;                // Num only
  std::optional<symbol_table::Symbol> symbol_value; // Var (value) or Fun (head)
  std::uint32_t child_start = 0;
  std::uint32_t child_count = 0;
};

std::size_t hash_combine(std::size_t seed, std::size_t value) {
  return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

std::size_t hash_rational(const numerica_core::Rational& value) {
  return std::hash<std::string>{}(value.to_string());
}

std::size_t compute_hash(const Node& node, const std::vector<std::uint32_t>& child_pool) {
  std::size_t h = hash_combine(0, static_cast<std::size_t>(node.tag));
  if (node.tag == AtomTag::Num) {
    h = hash_combine(h, hash_rational(node.num_value));
  }
  if (node.tag == AtomTag::Var || node.tag == AtomTag::Fun) {
    h = hash_combine(h, node.symbol_value->id());
  }
  for (std::uint32_t i = 0; i < node.child_count; ++i) {
    h = hash_combine(h, child_pool[node.child_start + i]);
  }
  return h;
}

bool nodes_equal(const Node& a, const Node& b, const std::vector<std::uint32_t>& child_pool) {
  if (a.tag != b.tag) {
    return false;
  }
  if (a.tag == AtomTag::Num && !(a.num_value == b.num_value)) {
    return false;
  }
  if ((a.tag == AtomTag::Var || a.tag == AtomTag::Fun) &&
      a.symbol_value->id() != b.symbol_value->id()) {
    return false;
  }
  if (a.child_count != b.child_count) {
    return false;
  }
  for (std::uint32_t i = 0; i < a.child_count; ++i) {
    if (child_pool[a.child_start + i] != child_pool[b.child_start + i]) {
      return false;
    }
  }
  return true;
}

} // namespace

struct AtomStore::Impl {
  std::vector<Node> nodes;
  std::vector<std::uint32_t> child_pool;
  // Structural hash -> candidate node indices sharing that hash (hash-consing
  // table; collisions are resolved by the exact nodes_equal() check below).
  std::unordered_map<std::size_t, std::vector<std::uint32_t>> hash_buckets;

  std::uint32_t intern(Node candidate) {
    std::size_t h = compute_hash(candidate, child_pool);
    auto& bucket = hash_buckets[h];
    for (std::uint32_t existing : bucket) {
      if (nodes_equal(nodes[existing], candidate, child_pool)) {
        return existing;
      }
    }
    nodes.push_back(std::move(candidate));
    auto new_index = static_cast<std::uint32_t>(nodes.size() - 1);
    bucket.push_back(new_index);
    return new_index;
  }
};

AtomStore::AtomStore() : impl_(std::make_unique<Impl>()) {}
AtomStore::~AtomStore() = default;

AtomTag AtomView::tag() const {
  return store_->impl_->nodes[index_].tag;
}

const numerica_core::Rational& AtomView::as_num() const {
  const Node& node = store_->impl_->nodes[index_];
  if (node.tag != AtomTag::Num) {
    throw std::logic_error("AtomView::as_num() called on a non-Num atom");
  }
  return node.num_value;
}

symbol_table::Symbol AtomView::as_var() const {
  const Node& node = store_->impl_->nodes[index_];
  if (node.tag != AtomTag::Var) {
    throw std::logic_error("AtomView::as_var() called on a non-Var atom");
  }
  return *node.symbol_value;
}

symbol_table::Symbol AtomView::function_head() const {
  const Node& node = store_->impl_->nodes[index_];
  if (node.tag != AtomTag::Fun) {
    throw std::logic_error("AtomView::function_head() called on a non-Fun atom");
  }
  return *node.symbol_value;
}

std::size_t AtomView::child_count() const {
  return store_->impl_->nodes[index_].child_count;
}

AtomView AtomView::child(std::size_t index) const {
  const Node& node = store_->impl_->nodes[index_];
  if (index >= node.child_count) {
    throw std::out_of_range("AtomView::child() index out of range");
  }
  return AtomView(store_, store_->impl_->child_pool[node.child_start + index]);
}

std::strong_ordering AtomView::operator<=>(const AtomView& other) const {
  const Node& a = store_->impl_->nodes[index_];
  const Node& b = other.store_->impl_->nodes[other.index_];

  if (a.tag != b.tag) {
    return a.tag <=> b.tag;
  }

  switch (a.tag) {
    case AtomTag::Num:
      return a.num_value <=> b.num_value;
    case AtomTag::Var:
      return a.symbol_value->id() <=> b.symbol_value->id();
    case AtomTag::Fun: {
      auto head_cmp = a.symbol_value->id() <=> b.symbol_value->id();
      if (head_cmp != std::strong_ordering::equal) {
        return head_cmp;
      }
      break;
    }
    case AtomTag::Add:
    case AtomTag::Mul:
    case AtomTag::Pow:
      break;
  }

  const std::size_t n = std::min<std::size_t>(a.child_count, b.child_count);
  for (std::size_t i = 0; i < n; ++i) {
    AtomView child_a(store_, store_->impl_->child_pool[a.child_start + i]);
    AtomView child_b(other.store_, other.store_->impl_->child_pool[b.child_start + i]);
    auto cmp = child_a <=> child_b;
    if (cmp != std::strong_ordering::equal) {
      return cmp;
    }
  }
  return a.child_count <=> b.child_count;
}

bool AtomView::operator==(const AtomView& other) const {
  return (*this <=> other) == std::strong_ordering::equal;
}

std::size_t AtomView::structural_hash() const {
  return compute_hash(store_->impl_->nodes[index_], store_->impl_->child_pool);
}

bool AtomView::same_slot(const AtomView& other) const noexcept {
  return store_ == other.store_ && index_ == other.index_;
}

Atom AtomStore::num(numerica_core::Rational value) {
  Node node;
  node.tag = AtomTag::Num;
  node.num_value = std::move(value);
  return Atom(this, impl_->intern(std::move(node)));
}

Atom AtomStore::var(symbol_table::Symbol symbol) {
  Node node;
  node.tag = AtomTag::Var;
  node.symbol_value = symbol;
  return Atom(this, impl_->intern(std::move(node)));
}

// Flattens one level of nested `flatten_tag` nodes among `operands` (so
// Add(a, Add(b, c)) collects as [a, b, c], not [a, Add(b,c)]) and collects
// the resulting child indices, unsorted. A member function (not a free
// function) because it needs access to AtomView/Atom's private
// store_/index_ fields, which are only friended to AtomStore.
std::vector<std::uint32_t> AtomStore::flatten_operands(std::span<const Atom> operands,
                                                       AtomTag flatten_tag) {
  std::vector<std::uint32_t> collected;
  collected.reserve(operands.size());
  for (const Atom& operand : operands) {
    AtomView view = operand.view();
    const Node& node = impl_->nodes[view.index_];
    if (node.tag == flatten_tag && view.store_ == this) {
      for (std::uint32_t i = 0; i < node.child_count; ++i) {
        collected.push_back(impl_->child_pool[node.child_start + i]);
      }
    } else {
      collected.push_back(view.index_);
    }
  }
  return collected;
}

Atom AtomStore::build_variadic(AtomTag tag, std::span<const Atom> operands) {
  std::vector<std::uint32_t> children = flatten_operands(operands, tag);
  std::sort(children.begin(), children.end(), [this](std::uint32_t lhs, std::uint32_t rhs) {
    return (AtomView(this, lhs) <=> AtomView(this, rhs)) == std::strong_ordering::less;
  });

  Node node;
  node.tag = tag;
  node.child_start = static_cast<std::uint32_t>(impl_->child_pool.size());
  node.child_count = static_cast<std::uint32_t>(children.size());
  impl_->child_pool.insert(impl_->child_pool.end(), children.begin(), children.end());
  return Atom(this, impl_->intern(std::move(node)));
}

Atom AtomStore::add(std::span<const Atom> terms) {
  return build_variadic(AtomTag::Add, terms);
}

Atom AtomStore::mul(std::span<const Atom> factors) {
  return build_variadic(AtomTag::Mul, factors);
}

Atom AtomStore::pow(Atom base, Atom exponent) {
  Node node;
  node.tag = AtomTag::Pow;
  node.child_start = static_cast<std::uint32_t>(impl_->child_pool.size());
  node.child_count = 2;
  impl_->child_pool.push_back(base.view().index_);
  impl_->child_pool.push_back(exponent.view().index_);
  return Atom(this, impl_->intern(std::move(node)));
}

Atom AtomStore::fun(symbol_table::Symbol head, std::span<const Atom> args) {
  Node node;
  node.tag = AtomTag::Fun;
  node.symbol_value = head;
  node.child_start = static_cast<std::uint32_t>(impl_->child_pool.size());
  node.child_count = static_cast<std::uint32_t>(args.size());
  for (const Atom& arg : args) {
    impl_->child_pool.push_back(arg.view().index_);
  }
  return Atom(this, impl_->intern(std::move(node)));
}

std::size_t AtomStore::node_count() const noexcept {
  return impl_->nodes.size();
}

} // namespace atom_core
