#pragma once

#include <stdexcept>
#include <utility>

#include <algebra_core/concepts.hh>
#include <poly_core/monomial.hh>
#include <poly_core/polynomial.hh>

/// @file
/// @brief General multivariate polynomial division by a single divisor
/// (M4-T2), reducing the dividend's leading term against the divisor's
/// leading term (w.r.t. `Monomial::operator<=>`'s lexicographic order,
/// the same order `MultivariatePolynomial<R>` already sorts terms by)
/// until nothing left in the dividend is divisible by the divisor's
/// leading monomial. This is the single-divisor case of Groebner-basis
/// division - no basis/S-polynomial machinery needed, just used here as an
/// exact "does g divide p" check (multivariate_gcd.hh's verification step).
namespace poly_core {

/// @brief Divides @p dividend by @p divisor: `dividend = quotient*divisor +
/// remainder`, where no term of `remainder` has a monomial divisible by
/// `divisor`'s leading monomial.
/// @throws std::invalid_argument if the two polynomials don't share the
///         same variable list.
/// @throws std::logic_error if @p divisor is zero.
template <typename R>
  requires algebra_core::Field<R>
[[nodiscard]] std::pair<MultivariatePolynomial<R>, MultivariatePolynomial<R>> multivariate_div_rem(
    const R& field, MultivariatePolynomial<R> dividend, const MultivariatePolynomial<R>& divisor) {
  if (dividend.variables().size() != divisor.variables().size()) {
    throw std::invalid_argument("poly_core::multivariate_div_rem: mismatched variable lists");
  }
  for (std::size_t i = 0; i < dividend.variables().size(); ++i) {
    if (!(dividend.variables()[i] == divisor.variables()[i])) {
      throw std::invalid_argument("poly_core::multivariate_div_rem: mismatched variable lists");
    }
  }
  if (divisor.is_zero()) {
    throw std::logic_error("poly_core::multivariate_div_rem: division by the zero polynomial");
  }

  std::vector<symbol_table::Symbol> variables = dividend.variables();
  MultivariatePolynomial<R> quotient(field, variables);
  MultivariatePolynomial<R> remainder(field, variables);
  MultivariatePolynomial<R> working = std::move(dividend);
  const Monomial& divisor_leading_monomial = divisor.leading_term().first;
  const typename R::Element& divisor_leading_coeff = divisor.leading_term().second;

  while (!working.is_zero()) {
    const Monomial& working_leading_monomial = working.leading_term().first;
    if (!divisor_leading_monomial.divides(working_leading_monomial)) {
      // The leading term can't be reduced further; it belongs to the
      // remainder. Peel it off and keep reducing the rest.
      remainder = remainder +
                  MultivariatePolynomial<R>::from_term(
                      field, variables, working_leading_monomial, working.leading_term().second);
      working =
          working - MultivariatePolynomial<R>::from_term(
                        field, variables, working_leading_monomial, working.leading_term().second);
      continue;
    }
    Monomial factor_monomial = working_leading_monomial / divisor_leading_monomial;
    typename R::Element factor_coeff =
        field.mul(working.leading_term().second, field.inv(divisor_leading_coeff));
    MultivariatePolynomial<R> factor_term =
        MultivariatePolynomial<R>::from_term(field, variables, factor_monomial, factor_coeff);
    quotient = quotient + factor_term;
    working = working - (factor_term * divisor);
  }
  return {quotient, remainder};
}

} // namespace poly_core
