#include <cstddef>
#include <utility>

#include <pattern_core/matcher.hh>
#include <pattern_core/wildcard.hh>

namespace pattern_core {

std::optional<MatchBindings>
match(atom_core::AtomView pattern, atom_core::AtomView target, MatchBindings bindings) {
  using atom_core::AtomTag;

  if (pattern.tag() == AtomTag::Var && is_wildcard(pattern.as_var())) {
    symbol_table::Symbol wildcard_symbol = pattern.as_var();
    auto existing = bindings.find(wildcard_symbol);
    if (existing != bindings.end()) {
      if (!(existing->second == target)) {
        return std::nullopt;
      }
      return bindings;
    }
    bindings.emplace(wildcard_symbol, target);
    return bindings;
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
    case AtomTag::Fun:
      if (!(pattern.function_head() == target.function_head())) {
        return std::nullopt;
      }
      break;
    case AtomTag::Add:
    case AtomTag::Mul:
    case AtomTag::Pow:
      break;
  }

  if (pattern.child_count() != target.child_count()) {
    return std::nullopt;
  }
  for (std::size_t i = 0; i < pattern.child_count(); ++i) {
    std::optional<MatchBindings> next =
        match(pattern.child(i), target.child(i), std::move(bindings));
    if (!next.has_value()) {
      return std::nullopt;
    }
    bindings = std::move(*next);
  }
  return bindings;
}

} // namespace pattern_core
