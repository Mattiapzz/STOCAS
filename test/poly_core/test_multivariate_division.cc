#include <stdexcept>
#include <vector>

#include <algebra_core/rings.hh>
#include <numerica_core/rational.hh>
#include <poly_core/monomial.hh>
#include <poly_core/multivariate_division.hh>
#include <poly_core/polynomial.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using algebra_core::RationalField;
using numerica_core::Rational;
using poly_core::Exponent;
using poly_core::Monomial;
using poly_core::multivariate_div_rem;
using poly_core::MultivariatePolynomial;

namespace {
constexpr const char* kNs = "poly_core_mv_division_tests";

using Poly = MultivariatePolynomial<RationalField>;

Poly term(const RationalField& field,
          std::vector<symbol_table::Symbol> vars,
          std::vector<Exponent> exponents,
          long long coeff) {
  return Poly::from_term(field, std::move(vars), Monomial(std::move(exponents)), Rational(coeff));
}
} // namespace

TEST_CASE("multivariate_div_rem: exact univariate division", "[poly_core][mv_division]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x1");
  // (x^2 - 1) = (x - 1)(x + 1), so dividing by (x-1) is exact.
  Poly a = term(field, {x}, {2}, 1) + term(field, {x}, {0}, -1);
  Poly b = term(field, {x}, {1}, 1) + term(field, {x}, {0}, -1);
  auto [quotient, remainder] = multivariate_div_rem(field, a, b);
  REQUIRE(remainder.is_zero());
  REQUIRE(quotient == term(field, {x}, {1}, 1) + term(field, {x}, {0}, 1));
}

TEST_CASE("multivariate_div_rem: multivariate exact division", "[poly_core][mv_division]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "mx1");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "my1");
  // (x^2 - y^2) / (x - y) = x + y
  Poly a = term(field, {x, y}, {2, 0}, 1) + term(field, {x, y}, {0, 2}, -1);
  Poly b = term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 1}, -1);
  auto [quotient, remainder] = multivariate_div_rem(field, a, b);
  REQUIRE(remainder.is_zero());
  REQUIRE(quotient == term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 1}, 1));
}

TEST_CASE("multivariate_div_rem: non-exact division produces a nonzero remainder",
          "[poly_core][mv_division]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x2");
  Poly a = term(field, {x}, {2}, 1) + term(field, {x}, {0}, 1);  // x^2 + 1
  Poly b = term(field, {x}, {1}, 1) + term(field, {x}, {0}, -1); // x - 1
  auto [quotient, remainder] = multivariate_div_rem(field, a, b);
  Poly reconstructed = quotient * b + remainder;
  REQUIRE(reconstructed == a);
  REQUIRE_FALSE(remainder.is_zero());
}

TEST_CASE("multivariate_div_rem: division by zero throws", "[poly_core][mv_division]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x3");
  Poly a = term(field, {x}, {1}, 1);
  Poly zero(field, {x});
  REQUIRE_THROWS_AS(multivariate_div_rem(field, a, zero), std::logic_error);
}

TEST_CASE("multivariate_div_rem: mismatched variable lists throw", "[poly_core][mv_division]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x4");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y4");
  Poly a = term(field, {x}, {1}, 1);
  Poly b = term(field, {y}, {1}, 1);
  REQUIRE_THROWS_AS(multivariate_div_rem(field, a, b), std::invalid_argument);
}

TEST_CASE("multivariate_div_rem: irreducible divisor leaves the dividend untouched",
          "[poly_core][mv_division]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "mx2");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "my2");
  Poly a = term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 1}, 1); // x + y
  Poly b = term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 0}, 1); // x + 1
  auto [quotient, remainder] = multivariate_div_rem(field, a, b);
  Poly reconstructed = quotient * b + remainder;
  REQUIRE(reconstructed == a);
}
