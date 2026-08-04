#pragma once

#include <map>
#include <optional>

#include <atom_core/atom.hh>
#include <symbol_table/symbol.hh>

/// @file
/// @brief Wildcard pattern matcher against `AtomView` (M5-T1): matches a
/// pattern atom (which may contain wildcard variables, see wildcard.hh)
/// against a target atom, returning a substitution map on success.
///
/// Scope of this first cut: literal structural matching plus single
/// wildcards, positional (order-sensitive) matching for `Add`/`Mul`/`Fun`
/// children. Sequence wildcards (matching a variable number of `Add`/`Mul`
/// arguments) and commutativity-aware matching (trying argument
/// permutations for `Add`/`Mul` and for `SymbolAttribute::Symmetric`
/// functions) are explicitly deferred to a later pattern_core task - until
/// then, an `Add`/`Mul`/`Fun` pattern only matches a target whose children
/// appear in the exact same order, which is correct whenever both were
/// normalized by the same `atom_core::AtomStore` canonicalization but is
/// not full commutative pattern matching.
namespace pattern_core {

/// @brief Orders `symbol_table::Symbol`s by their table-internal id, so
/// they can be used as `std::map` keys (`Symbol` itself has no `operator<`
/// - equality/identity is all the symbol table contract guarantees).
struct SymbolIdLess {
  [[nodiscard]] bool operator()(const symbol_table::Symbol& lhs,
                                const symbol_table::Symbol& rhs) const noexcept {
    return lhs.id() < rhs.id();
  }
};

/// @brief A wildcard-symbol -> matched-subterm substitution map.
using MatchBindings = std::map<symbol_table::Symbol, atom_core::AtomView, SymbolIdLess>;

/// @brief Matches @p pattern against @p target, extending @p bindings.
///
/// A wildcard variable in @p pattern (see `is_wildcard()`) matches any
/// target subterm the first time it's encountered, binding it in the
/// returned map; every subsequent occurrence of the same wildcard within
/// this match must bind to a structurally equal (`AtomView::operator==`)
/// subterm, or the match fails. Non-wildcard `Num`/`Var` leaves must match
/// exactly; `Add`/`Mul`/`Pow`/`Fun` nodes must share the same tag (and, for
/// `Fun`, the same head symbol) and the same child count, with each child
/// matched positionally in order (see the file comment on the
/// commutativity/sequence-wildcard limitation this implies).
///
/// @param pattern The pattern to match, possibly containing wildcards.
/// @param target The concrete atom to match against.
/// @param bindings Bindings already established (e.g. by matching a
///        preceding sibling); matched wildcards are added on top of these.
///        Defaults to an empty map for a fresh top-level match.
/// @return The extended bindings on success, or `std::nullopt` if @p
///         pattern does not match @p target (given @p bindings).
[[nodiscard]] std::optional<MatchBindings>
match(atom_core::AtomView pattern, atom_core::AtomView target, MatchBindings bindings = {});

} // namespace pattern_core
