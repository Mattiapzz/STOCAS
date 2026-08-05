#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

#include <pattern_core/matcher.hh>
#include <pattern_core/wildcard.hh>

namespace pattern_core {

namespace {

using atom_core::AtomTag;
using atom_core::AtomView;
using symbol_table::Symbol;

bool atom_view_less(const AtomView& lhs, const AtomView& rhs) {
  return (lhs <=> rhs) == std::strong_ordering::less;
}

// Order-independent (multiset) equality, used to check a repeated sequence
// wildcard's new capture against its existing binding - the enclosing
// context (Add/Mul/Symmetric Fun) is commutative, so capture order alone
// must not make an otherwise-consistent repeat binding fail.
bool same_multiset(std::vector<AtomView> lhs, std::vector<AtomView> rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  std::sort(lhs.begin(), lhs.end(), atom_view_less);
  std::sort(rhs.begin(), rhs.end(), atom_view_less);
  return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

std::vector<AtomView> children_of(AtomView atom) {
  std::vector<AtomView> result;
  result.reserve(atom.child_count());
  for (std::size_t i = 0; i < atom.child_count(); ++i) {
    result.push_back(atom.child(i));
  }
  return result;
}

// A direct child that is itself a sequence wildcard (AtLeastOne/AnyCount).
// At most one is supported per Add/Mul/Fun argument list (see matcher.hh).
struct SequenceWildcard {
  Symbol symbol;
  WildcardKind kind;
};

// Splits pattern_children into (normals, at-most-one sequence wildcard).
// @throws std::invalid_argument if more than one direct child is a
//         sequence wildcard.
std::pair<std::vector<AtomView>, std::optional<SequenceWildcard>>
split_sequence_wildcard(const std::vector<AtomView>& pattern_children) {
  std::vector<AtomView> normals;
  std::optional<SequenceWildcard> sequence;
  for (const AtomView& child : pattern_children) {
    if (child.tag() == AtomTag::Var) {
      Symbol symbol = child.as_var();
      std::optional<WildcardKind> kind = wildcard_kind(symbol);
      if (kind.has_value() && *kind != WildcardKind::Single) {
        if (sequence.has_value()) {
          throw std::invalid_argument(
              "pattern_core::match: at most one sequence wildcard is supported per "
              "Add/Mul/Fun argument list");
        }
        sequence = SequenceWildcard{symbol, *kind};
        continue;
      }
    }
    normals.push_back(child);
  }
  return {std::move(normals), sequence};
}

std::optional<MatchBindings> match_impl(AtomView pattern, AtomView target, MatchBindings bindings);

// Binds (or checks consistency of) a sequence wildcard's capture.
std::optional<MatchBindings> bind_sequence(const SequenceWildcard& sequence,
                                           std::vector<AtomView> captured,
                                           MatchBindings bindings) {
  if (sequence.kind == WildcardKind::AtLeastOne && captured.empty()) {
    return std::nullopt;
  }
  auto existing = bindings.find(sequence.symbol);
  if (existing != bindings.end()) {
    const auto* existing_vector = std::get_if<std::vector<AtomView>>(&existing->second);
    if (existing_vector == nullptr || !same_multiset(*existing_vector, captured)) {
      return std::nullopt;
    }
    return bindings;
  }
  bindings.emplace(sequence.symbol, std::move(captured));
  return bindings;
}

// Backtracking multiset assignment: tries to match every element of
// `normals` against a distinct element of `pool` (order-independent).
// `used` is mutated in place to mark which pool indices were consumed by a
// successful assignment (left in its pre-call state, all false, on
// failure).
std::optional<MatchBindings> assign_normals(const std::vector<AtomView>& normals,
                                            std::size_t normal_index,
                                            const std::vector<AtomView>& pool,
                                            std::vector<bool>& used,
                                            MatchBindings bindings) {
  if (normal_index == normals.size()) {
    return bindings;
  }
  for (std::size_t i = 0; i < pool.size(); ++i) {
    if (used[i]) {
      continue;
    }
    std::optional<MatchBindings> attempt = match_impl(normals[normal_index], pool[i], bindings);
    if (!attempt.has_value()) {
      continue;
    }
    used[i] = true;
    std::optional<MatchBindings> result =
        assign_normals(normals, normal_index + 1, pool, used, std::move(*attempt));
    if (result.has_value()) {
      return result;
    }
    used[i] = false;
  }
  return std::nullopt;
}

// Commutative (multiset) matching of pattern_children against target's
// children, used for Add/Mul (always) and Symmetric Fun.
std::optional<MatchBindings> match_commutative(const std::vector<AtomView>& pattern_children,
                                               AtomView target,
                                               MatchBindings bindings) {
  auto [normals, sequence] = split_sequence_wildcard(pattern_children);
  std::vector<AtomView> target_children = children_of(target);
  if (!sequence.has_value() && normals.size() != target_children.size()) {
    return std::nullopt;
  }
  if (sequence.has_value() && normals.size() > target_children.size()) {
    return std::nullopt;
  }

  std::vector<bool> used(target_children.size(), false);
  std::optional<MatchBindings> assigned =
      assign_normals(normals, 0, target_children, used, std::move(bindings));
  if (!assigned.has_value()) {
    return std::nullopt;
  }

  if (!sequence.has_value()) {
    return assigned;
  }
  std::vector<AtomView> leftover;
  for (std::size_t i = 0; i < target_children.size(); ++i) {
    if (!used[i]) {
      leftover.push_back(target_children[i]);
    }
  }
  return bind_sequence(*sequence, std::move(leftover), std::move(*assigned));
}

// Positional matching of pattern_children against target's children, with
// at most one direct child allowed to be a sequence wildcard capturing the
// contiguous run between the (positionally matched) prefix and suffix.
// Used for non-Symmetric Fun.
std::optional<MatchBindings> match_positional_with_sequence(
    const std::vector<AtomView>& pattern_children, AtomView target, MatchBindings bindings) {
  std::optional<SequenceWildcard> sequence;
  std::size_t sequence_position = 0;
  for (std::size_t i = 0; i < pattern_children.size(); ++i) {
    const AtomView& child = pattern_children[i];
    if (child.tag() != AtomTag::Var) {
      continue;
    }
    std::optional<WildcardKind> kind = wildcard_kind(child.as_var());
    if (kind.has_value() && *kind != WildcardKind::Single) {
      if (sequence.has_value()) {
        throw std::invalid_argument(
            "pattern_core::match: at most one sequence wildcard is supported per "
            "Add/Mul/Fun argument list");
      }
      sequence = SequenceWildcard{child.as_var(), *kind};
      sequence_position = i;
    }
  }

  std::vector<AtomView> target_children = children_of(target);
  if (!sequence.has_value()) {
    if (pattern_children.size() != target_children.size()) {
      return std::nullopt;
    }
    for (std::size_t i = 0; i < pattern_children.size(); ++i) {
      std::optional<MatchBindings> next =
          match_impl(pattern_children[i], target_children[i], std::move(bindings));
      if (!next.has_value()) {
        return std::nullopt;
      }
      bindings = std::move(*next);
    }
    return bindings;
  }

  std::size_t prefix_count = sequence_position;
  std::size_t suffix_count = pattern_children.size() - sequence_position - 1;
  if (target_children.size() < prefix_count + suffix_count) {
    return std::nullopt;
  }
  std::size_t middle_count = target_children.size() - prefix_count - suffix_count;
  if (sequence->kind == WildcardKind::AtLeastOne && middle_count == 0) {
    return std::nullopt;
  }

  for (std::size_t i = 0; i < prefix_count; ++i) {
    std::optional<MatchBindings> next =
        match_impl(pattern_children[i], target_children[i], std::move(bindings));
    if (!next.has_value()) {
      return std::nullopt;
    }
    bindings = std::move(*next);
  }
  for (std::size_t i = 0; i < suffix_count; ++i) {
    std::optional<MatchBindings> next = match_impl(pattern_children[sequence_position + 1 + i],
                                                   target_children[prefix_count + middle_count + i],
                                                   std::move(bindings));
    if (!next.has_value()) {
      return std::nullopt;
    }
    bindings = std::move(*next);
  }
  std::vector<AtomView> captured(
      target_children.begin() + static_cast<std::ptrdiff_t>(prefix_count),
      target_children.begin() + static_cast<std::ptrdiff_t>(prefix_count + middle_count));
  return bind_sequence(*sequence, std::move(captured), std::move(bindings));
}

bool is_symmetric(Symbol head) {
  std::vector<symbol_table::SymbolAttribute> attributes = symbol_table::symbol_attributes(head);
  return std::find(attributes.begin(),
                   attributes.end(),
                   symbol_table::SymbolAttribute::Symmetric) != attributes.end();
}

std::optional<MatchBindings> match_impl(AtomView pattern, AtomView target, MatchBindings bindings) {
  if (pattern.tag() == AtomTag::Var) {
    Symbol symbol = pattern.as_var();
    std::optional<WildcardKind> kind = wildcard_kind(symbol);
    if (kind.has_value()) {
      if (*kind != WildcardKind::Single) {
        throw std::invalid_argument(
            "pattern_core::match: sequence wildcards are only valid as a direct child of "
            "Add/Mul/Fun");
      }
      auto existing = bindings.find(symbol);
      if (existing != bindings.end()) {
        const auto* existing_atom = std::get_if<AtomView>(&existing->second);
        if (existing_atom == nullptr || !(*existing_atom == target)) {
          return std::nullopt;
        }
        return bindings;
      }
      bindings.emplace(symbol, target);
      return bindings;
    }
  }

  if (pattern.tag() != target.tag()) {
    return std::nullopt;
  }

  switch (pattern.tag()) {
    case AtomTag::Num:
      if (!(pattern.as_num() == target.as_num())) {
        return std::nullopt;
      }
      return bindings;
    case AtomTag::Var:
      if (!(pattern.as_var() == target.as_var())) {
        return std::nullopt;
      }
      return bindings;
    case AtomTag::Pow: {
      if (pattern.child_count() != 2 || target.child_count() != 2) {
        return std::nullopt;
      }
      std::optional<MatchBindings> after_base =
          match_impl(pattern.child(0), target.child(0), std::move(bindings));
      if (!after_base.has_value()) {
        return std::nullopt;
      }
      return match_impl(pattern.child(1), target.child(1), std::move(*after_base));
    }
    case AtomTag::Add:
    case AtomTag::Mul:
      return match_commutative(children_of(pattern), target, std::move(bindings));
    case AtomTag::Fun:
      if (!(pattern.function_head() == target.function_head())) {
        return std::nullopt;
      }
      if (is_symmetric(pattern.function_head())) {
        return match_commutative(children_of(pattern), target, std::move(bindings));
      }
      return match_positional_with_sequence(children_of(pattern), target, std::move(bindings));
  }
  return std::nullopt; // unreachable
}

} // namespace

std::optional<MatchBindings>
match(atom_core::AtomView pattern, atom_core::AtomView target, MatchBindings bindings) {
  return match_impl(pattern, target, std::move(bindings));
}

} // namespace pattern_core
