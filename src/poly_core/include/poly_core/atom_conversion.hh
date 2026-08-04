#pragma once

#include <vector>

#include <algebra_core/rings.hh>
#include <atom_core/atom.hh>
#include <poly_core/polynomial.hh>
#include <poly_core/rational_polynomial.hh>
#include <symbol_table/symbol.hh>

/// @file
/// @brief `Atom` <-> `MultivariatePolynomial`/`RationalPolynomial` conversion
/// (M4-T4's last bullet): the bridge between `atom_core`'s general
/// expression tree and `poly_core`'s polynomial types.
///
/// The `Atom -> polynomial` direction requires an explicit, ordered
/// variable list (same convention as `MultivariatePolynomial`/
/// `RationalPolynomial` themselves) and only accepts atoms built from
/// `Num`/`Var`/`Add`/`Mul`/`Pow` nodes - `Fun` (function call) nodes have no
/// polynomial representation and always throw. `atom_to_polynomial()`
/// additionally requires every `Pow` exponent to be a non-negative integer
/// (a bare `MultivariatePolynomial` can't represent division);
/// `atom_to_rational_polynomial()` also accepts negative integer exponents
/// (e.g. `1/x`, which `atom_core`'s parser desugars to `Pow(x, -1)`) by
/// taking a reciprocal.
///
/// The reverse direction never needs a variable list - both polynomial
/// types already carry their own - and produces atoms already in
/// `atom_core`'s own canonical shape (`AtomStore::mul()`/`add()` do the
/// same sorting/flattening `atom_core::normalize()` relies on), so the
/// result never needs a separate `normalize()` pass.
namespace poly_core {

/// @brief Converts @p atom into a `MultivariatePolynomial<RationalField>`
/// over the explicit variable list @p variables.
/// @throws std::invalid_argument if @p atom contains a `Fun` node, a
///         variable not present in @p variables, or a `Pow` whose exponent
///         is not a non-negative integer `Num` (use
///         atom_to_rational_polynomial() for negative integer exponents).
[[nodiscard]] MultivariatePolynomial<algebra_core::RationalField>
atom_to_polynomial(atom_core::AtomView atom,
                   const algebra_core::RationalField& field,
                   const std::vector<symbol_table::Symbol>& variables);

/// @brief Converts @p atom into a `RationalPolynomial`, like
/// atom_to_polynomial() but also accepting negative integer `Pow`
/// exponents (taking a reciprocal).
/// @throws std::invalid_argument if @p atom contains a `Fun` node, a
///         variable not present in @p variables, or a `Pow` whose exponent
///         is not an integer `Num`.
/// @throws std::domain_error if a negative exponent is applied to a base
///         that evaluates to zero.
[[nodiscard]] RationalPolynomial
atom_to_rational_polynomial(atom_core::AtomView atom,
                            const algebra_core::RationalField& field,
                            const std::vector<symbol_table::Symbol>& variables);

/// @brief Converts @p polynomial back into an `Atom`, built in @p store.
[[nodiscard]] atom_core::Atom
polynomial_to_atom(const MultivariatePolynomial<algebra_core::RationalField>& polynomial,
                   atom_core::AtomStore& store);

/// @brief Converts @p polynomial back into an `Atom`, built in @p store.
/// If the denominator is 1, returns the numerator's atom directly;
/// otherwise `Mul(numerator, Pow(denominator, -1))`, matching `atom_core`'s
/// own `/` desugaring convention (so the result round-trips through
/// `atom_core::print()`/`parse()` the same way hand-written division does).
[[nodiscard]] atom_core::Atom rational_polynomial_to_atom(const RationalPolynomial& polynomial,
                                                          atom_core::AtomStore& store);

} // namespace poly_core
