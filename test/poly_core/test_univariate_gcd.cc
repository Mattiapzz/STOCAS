#include <stdexcept>
#include <vector>

#include <algebra_core/rings.hh>
#include <numerica_core/rational.hh>
#include <poly_core/monomial.hh>
#include <poly_core/polynomial.hh>
#include <poly_core/univariate_gcd.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using algebra_core::RationalField;
using numerica_core::Rational;
using poly_core::Exponent;
using poly_core::Monomial;
using poly_core::MultivariatePolynomial;
using poly_core::univariate_div_rem;
using poly_core::univariate_gcd;

namespace {
constexpr const char* kNs = "poly_core_gcd_tests";

using Poly = MultivariatePolynomial<RationalField>;

Poly term(const RationalField& field, symbol_table::Symbol x, Exponent exponent, long long coeff) {
  return Poly::from_term(field, {x}, Monomial(std::vector<Exponent>{exponent}), Rational(coeff));
}

// Builds a polynomial in x from coefficients, highest degree first.
Poly from_coeffs(const RationalField& field,
                 symbol_table::Symbol x,
                 const std::vector<long long>& coeffs_high_to_low) {
  Poly result(field, {x});
  Exponent degree = static_cast<Exponent>(coeffs_high_to_low.size() - 1);
  for (long long coeff : coeffs_high_to_low) {
    if (coeff != 0) {
      result = result + term(field, x, degree, coeff);
    }
    --degree;
  }
  return result;
}
} // namespace

TEST_CASE("univariate_div_rem: exact division, zero remainder", "[poly_core][gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x1");
  // (x^2 - 1) / (x - 1) = x + 1 remainder 0
  Poly a = from_coeffs(field, x, {1, 0, -1});
  Poly b = from_coeffs(field, x, {1, -1});
  auto [quotient, remainder] = univariate_div_rem(field, a, b);
  REQUIRE(remainder.is_zero());
  REQUIRE(quotient == from_coeffs(field, x, {1, 1}));
}

TEST_CASE("univariate_div_rem: division with nonzero remainder", "[poly_core][gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x2");
  // x^2 + 1 divided by x - 1: quotient x + 1, remainder 2
  Poly a = from_coeffs(field, x, {1, 0, 1});
  Poly b = from_coeffs(field, x, {1, -1});
  auto [quotient, remainder] = univariate_div_rem(field, a, b);
  REQUIRE(quotient == from_coeffs(field, x, {1, 1}));
  REQUIRE(remainder.num_terms() == 1);
  REQUIRE(remainder.leading_term().second == Rational(2));
}

TEST_CASE("univariate_div_rem: division by zero throws", "[poly_core][gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x3");
  Poly a = from_coeffs(field, x, {1, 0});
  Poly zero(field, {x});
  REQUIRE_THROWS_AS(univariate_div_rem(field, a, zero), std::logic_error);
}

TEST_CASE("univariate_gcd: shared linear factor", "[poly_core][gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x4");
  // gcd(x^2 - 1, x^2 - 3x + 2) == x - 1 (both share the factor x-1:
  // x^2-1 = (x-1)(x+1), x^2-3x+2 = (x-1)(x-2))
  Poly a = from_coeffs(field, x, {1, 0, -1});
  Poly b = from_coeffs(field, x, {1, -3, 2});
  Poly gcd_result = univariate_gcd(field, a, b);
  REQUIRE(gcd_result == from_coeffs(field, x, {1, -1}));
}

TEST_CASE("univariate_gcd: coprime polynomials give a constant (unit) gcd", "[poly_core][gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x5");
  Poly a = from_coeffs(field, x, {1, -1}); // x - 1
  Poly b = from_coeffs(field, x, {1, 1});  // x + 1
  Poly gcd_result = univariate_gcd(field, a, b);
  REQUIRE(gcd_result.num_terms() == 1);
  REQUIRE(gcd_result.leading_term().first.total_degree() == 0);
  REQUIRE(gcd_result.leading_term().second == Rational(1)); // normalized monic
}

TEST_CASE("univariate_gcd: gcd(0, b) is monic(b)", "[poly_core][gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x6");
  Poly zero(field, {x});
  Poly b = from_coeffs(field, x, {2, 4}); // 2x + 4, monic form x + 2
  Poly gcd_result = univariate_gcd(field, zero, b);
  REQUIRE(gcd_result == from_coeffs(field, x, {1, 2}));
}

TEST_CASE("univariate_gcd: mismatched variable throws", "[poly_core][gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x7");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y7");
  Poly a = term(field, x, 1, 1);
  Poly b = term(field, y, 1, 1);
  REQUIRE_THROWS_AS(univariate_gcd(field, a, b), std::invalid_argument);
}

TEST_CASE("univariate_gcd: multivariate input throws", "[poly_core][gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x8");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y8");
  Poly a = Poly::from_term(field, {x, y}, Monomial(std::vector<Exponent>{1, 1}), Rational(1));
  Poly b = term(field, x, 1, 1);
  REQUIRE_THROWS_AS(univariate_gcd(field, a, b), std::invalid_argument);
}
