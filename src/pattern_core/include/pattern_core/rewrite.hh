#pragma once

#include <cstddef>
#include <functional>

#include <atom_core/atom.hh>
#include <pattern_core/matcher.hh>

/// @file
/// @brief Rule-based rewriting (M5-T3): a `Rule` pairs a pattern (see
/// matcher.hh) with a replacement template and an optional guard
/// predicate; `replace_all()` applies it once, bottom-up, to every subterm
/// of a target atom; `replace_all_multiple()` repeats that until a fixed
/// point or a bounded iteration cap, so a rule set that never converges is
/// caught (`std::runtime_error`) rather than looping forever.
namespace pattern_core {

/// @brief A pattern-matching + replacement rule. `pattern` and
/// `replacement` may reference wildcards (see wildcard.hh); every wildcard
/// `replacement` uses must also appear (and get bound) in `pattern` -
/// checked by substitute() at rewrite time.
struct Rule {
  atom_core::AtomView pattern;
  atom_core::AtomView replacement;
  /// Optional guard: when set, a structural match is only accepted if this
  /// returns true for the resulting bindings (e.g. to restrict a rule to
  /// numeric wildcards). Called once per candidate match; a null
  /// `std::function` (the default) accepts every structural match.
  std::function<bool(const MatchBindings&)> guard;
};

/// @brief Rebuilds @p replacement in @p store with every wildcard
/// substituted per @p bindings: a `WildcardKind::Single` wildcard is
/// replaced by its bound atom; a sequence wildcard, appearing as a direct
/// child of `Add`/`Mul`/`Fun`, is spliced in place by its bound list.
/// Non-wildcard leaves are rebuilt into @p store unchanged.
/// @param store The store the result (and every rebuilt node) belongs to.
/// @param replacement The replacement template, possibly from a different
///        store than @p store.
/// @param bindings Bindings produced by match()ing some pattern.
/// @return The substituted replacement, owned by @p store.
/// @throws std::invalid_argument if @p replacement references a wildcard
///         absent from @p bindings, or uses a sequence wildcard outside a
///         direct `Add`/`Mul`/`Fun` child position.
[[nodiscard]] atom_core::Atom substitute(atom_core::AtomStore& store,
                                         atom_core::AtomView replacement,
                                         const MatchBindings& bindings);

/// @brief A single bottom-up pass over @p target: every child is
/// transformed first, then @p rule is tried against the (already
/// rebuilt) node; on a match that also passes @p rule's guard (if any),
/// the node is replaced by substitute()ing @p rule's replacement in
/// place - the freshly substituted replacement is *not* re-matched within
/// this same pass (that is replace_all_multiple()'s job). Every subterm of
/// @p target is visited exactly once.
/// @param store The store the result is built in. @p target must belong
///        to this same store (a precondition of `AtomStore::as_atom()`,
///        used internally); @p rule's pattern/replacement may belong to a
///        different store.
/// @param rule The rule to apply.
/// @param target The atom to rewrite; must belong to @p store.
/// @return The rewritten atom (structurally unchanged if @p rule never
///         matched anywhere).
[[nodiscard]] atom_core::Atom
replace_all(atom_core::AtomStore& store, const Rule& rule, atom_core::AtomView target);

/// @brief Repeatedly applies replace_all() until the result stops
/// changing (a fixed point) or @p max_iterations passes have run.
/// @param store The store @p target belongs to, and the result is built
///        in.
/// @param rule The rule to apply.
/// @param target The atom to rewrite.
/// @param max_iterations The iteration cap - a documented, tested
///        termination policy so a rule set that never converges (e.g. one
///        whose replacement keeps growing) is caught rather than hanging.
/// @return The converged, fully-rewritten atom.
/// @throws std::runtime_error if @p max_iterations passes complete without
///         reaching a fixed point.
[[nodiscard]] atom_core::Atom replace_all_multiple(atom_core::AtomStore& store,
                                                   const Rule& rule,
                                                   atom_core::AtomView target,
                                                   std::size_t max_iterations = 1000);

} // namespace pattern_core
