#pragma once

#include <atom_core/atom.hh>

/// @file
/// @brief Basic normalization (M3-T4): flatten nested Add/Mul (already done
/// by AtomStore's builders), combine like terms, constant-fold sums and
/// products, canonical sign/ordering of terms (via AtomView's total order,
/// already used by AtomStore::add()/mul()).
namespace atom_core {

/// @brief Returns the normal form of @p atom, built in @p store.
///
/// Rules applied (bottom-up, children normalized first):
///  - `Add`: numeric terms are summed into a single constant; terms sharing
///    the same non-numeric factor are combined into one term with a summed
///    coefficient (`x + x` -> `2*x`).
///  - `Mul`: numeric factors are folded into a single constant; a zero
///    factor short-circuits the whole product to `0` (`0 * f(x)` -> `0`);
///    factors sharing the same base are combined into one `Pow` with a
///    summed exponent (`x * x` -> `x^2`).
///  - `Pow`: exponent `0` folds to `1` (`(x+1)^0` -> `1`); exponent `1`
///    folds to the base. General `Num^Num` constant-folding is out of
///    scope for M3-T4 (not required by its normalization table) and is
///    deferred to a later milestone.
///  - `Fun`: arguments are normalized; the call itself is not evaluated.
///
/// @param store The arena new (normalized) nodes are built in - must be the
///        same store @p atom came from.
/// @param atom The atom to normalize.
/// @return The normal-form atom.
[[nodiscard]] Atom normalize(AtomStore& store, AtomView atom);

} // namespace atom_core
