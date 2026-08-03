#pragma once

#include <stdexcept>
#include <utility>

#include <algebra_core/concepts.hh>
#include <poly_core/monomial.hh>
#include <poly_core/polynomial.hh>

/// @file
/// @brief Univariate polynomial GCD (M4-T1), classic Euclidean algorithm on
/// `MultivariatePolynomial<R>` restricted to exactly one variable. Requires
/// `algebra_core::Field<R>` coefficients (the polynomial ring `R[x]` over a
/// field is itself a Euclidean domain; exact division needs `inv()` on
/// leading coefficients, which only a field guarantees). Subresultant PRS
/// (avoiding coefficient growth for exact-integer inputs) is deferred to a
/// later milestone per the roadmap.
namespace poly_core {

namespace detail {

template <typename R> void require_univariate(const MultivariatePolynomial<R>& poly) {
  if (poly.variables().size() != 1) {
    throw std::invalid_argument("poly_core: expected a univariate polynomial (exactly 1 variable)");
  }
}

} // namespace detail

/// @brief Polynomial long division: `a = quotient * b + remainder`, with
/// `remainder` either zero or of strictly lower degree than `b`.
/// @throws std::invalid_argument if either polynomial is not univariate, or
///         if the two polynomials don't share the same variable.
/// @throws std::logic_error if @p b is zero.
template <typename R>
  requires algebra_core::Field<R>
[[nodiscard]] std::pair<MultivariatePolynomial<R>, MultivariatePolynomial<R>> univariate_div_rem(
    const R& field, MultivariatePolynomial<R> a, const MultivariatePolynomial<R>& b) {
  detail::require_univariate(a);
  detail::require_univariate(b);
  if (!(a.variables()[0] == b.variables()[0])) {
    throw std::invalid_argument("poly_core::univariate_div_rem: mismatched variable");
  }
  if (b.is_zero()) {
    throw std::logic_error("poly_core::univariate_div_rem: division by the zero polynomial");
  }

  std::vector<symbol_table::Symbol> variables = a.variables();
  MultivariatePolynomial<R> quotient(field, variables);
  MultivariatePolynomial<R> remainder = std::move(a);
  const Exponent b_degree = b.leading_term().first.exponent(0);
  const auto& b_lead_coeff = b.leading_term().second;

  while (!remainder.is_zero() && remainder.leading_term().first.exponent(0) >= b_degree) {
    const Exponent factor_degree = remainder.leading_term().first.exponent(0) - b_degree;
    typename R::Element factor_coeff =
        field.mul(remainder.leading_term().second, field.inv(b_lead_coeff));
    Monomial factor_monomial(std::vector<Exponent>{factor_degree});
    MultivariatePolynomial<R> factor_term =
        MultivariatePolynomial<R>::from_term(field, variables, factor_monomial, factor_coeff);
    quotient = quotient + factor_term;
    remainder = remainder - (factor_term * b);
  }
  return {quotient, remainder};
}

/// @brief Euclidean-algorithm GCD of two univariate polynomials over a
/// field, normalized to be monic (leading coefficient 1) - the canonical
/// "gcd up to units" representative for a field's polynomial ring.
/// @return The monic GCD, or the zero polynomial if both @p a and @p b are
///         zero.
/// @throws std::invalid_argument if either polynomial is not univariate, or
///         if they don't share the same variable.
template <typename R>
  requires algebra_core::Field<R>
[[nodiscard]] MultivariatePolynomial<R>
univariate_gcd(const R& field, MultivariatePolynomial<R> a, MultivariatePolynomial<R> b) {
  detail::require_univariate(a);
  detail::require_univariate(b);
  if (!(a.variables()[0] == b.variables()[0])) {
    throw std::invalid_argument("poly_core::univariate_gcd: mismatched variable");
  }

  while (!b.is_zero()) {
    auto [unused_quotient, remainder] = univariate_div_rem(field, a, b);
    a = std::move(b);
    b = std::move(remainder);
  }
  if (a.is_zero()) {
    return a;
  }
  typename R::Element inv_lead = field.inv(a.leading_term().second);
  Monomial one(std::vector<Exponent>{0});
  MultivariatePolynomial<R> scale =
      MultivariatePolynomial<R>::from_term(field, a.variables(), one, inv_lead);
  return a * scale;
}

} // namespace poly_core
