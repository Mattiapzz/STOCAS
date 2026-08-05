#pragma once

#include <map>
#include <optional>
#include <variant>
#include <vector>

#include <atom_core/atom.hh>
#include <symbol_table/symbol.hh>

/// @file
/// @brief Wildcard pattern matcher against `AtomView` (M5-T1, extended
/// M5-T2): matches a pattern atom (which may contain wildcards, see
/// wildcard.hh) against a target atom, returning a substitution map on
/// success.
///
/// Matching per `AtomTag`:
///   - `Num`/`Var` (non-wildcard) leaves: match iff equal.
///   - A `WildcardKind::Single` wildcard: matches any one target atom,
///     binding it (or, on repeat occurrence, requiring the same atom
///     again).
///   - `Pow`: base and exponent matched positionally (exponentiation isn't
///     commutative, and neither slot is a variadic argument list, so
///     sequence wildcards aren't meaningful here - see below).
///   - `Add`/`Mul`: always commutative. Children are matched as a multiset
///     via backtracking search (try each pattern child against each
///     not-yet-consumed target child), not positionally - order in the
///     canonicalized `AtomView` tree is irrelevant to matching, only to
///     construction. At most one direct child may be a sequence wildcard
///     (`WildcardKind::AtLeastOne`/`AnyCount`); it captures every target
///     child left unconsumed by the other pattern children, in whatever
///     order the backtracking search happens to leave them (order among a
///     sequence-wildcard capture is otherwise unconstrained, since the
///     enclosing context is commutative).
///   - `Fun`: head symbol must match. If the head was registered with
///     `symbol_table::SymbolAttribute::Symmetric`, arguments are matched
///     commutatively (same multiset search as Add/Mul, same at-most-one-
///     sequence-wildcard restriction). Otherwise arguments are matched
///     positionally, with at most one direct child allowed to be a
///     sequence wildcard - it captures the contiguous run of target
///     arguments between the (positionally matched) prefix and suffix.
///
/// A sequence wildcard appearing anywhere other than a direct child of
/// `Add`/`Mul`/`Fun` (e.g. as `Pow`'s base, or as the top-level pattern
/// passed to match()) is a pattern-authoring error and throws
/// `std::invalid_argument`, as does a pattern with more than one direct
/// sequence-wildcard child in a single `Add`/`Mul`/`Fun` argument list (an
/// ambiguous partitioning problem this matcher doesn't attempt to solve).
///
/// Not yet implemented (left for a later pattern_core task):
/// `SymbolAttribute::Antisymmetric`/`Linear` have no special matching
/// behavior yet (a `Fun` with either is matched positionally, same as an
/// unattributed one); conditional rules (guard predicates on matched
/// wildcards).
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

/// @brief What a wildcard is bound to: a `WildcardKind::Single` wildcard
/// binds one `AtomView`; a sequence wildcard binds the list of sibling
/// atoms it captured from an Add/Mul/Fun argument list (see the file
/// comment on capture order for the commutative Add/Mul/Symmetric-Fun
/// case).
using BoundValue = std::variant<atom_core::AtomView, std::vector<atom_core::AtomView>>;

/// @brief A wildcard-symbol -> matched-subterm(s) substitution map.
using MatchBindings = std::map<symbol_table::Symbol, BoundValue, SymbolIdLess>;

/// @brief Matches @p pattern against @p target, extending @p bindings. See
/// the file comment for the full per-`AtomTag` matching rules.
/// @param pattern The pattern to match, possibly containing wildcards.
/// @param target The concrete atom to match against.
/// @param bindings Bindings already established (e.g. by matching a
///        preceding sibling); matched wildcards are added on top of these.
///        Defaults to an empty map for a fresh top-level match.
/// @return The extended bindings on success, or `std::nullopt` if @p
///         pattern does not match @p target (given @p bindings).
/// @throws std::invalid_argument if @p pattern misuses a sequence wildcard
///         (see the file comment).
[[nodiscard]] std::optional<MatchBindings>
match(atom_core::AtomView pattern, atom_core::AtomView target, MatchBindings bindings = {});

} // namespace pattern_core
