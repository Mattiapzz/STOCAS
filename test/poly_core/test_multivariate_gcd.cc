#include <stdexcept>
#include <vector>

#include <algebra_core/rings.hh>
#include <numerica_core/rational.hh>
#include <poly_core/monomial.hh>
#include <poly_core/multivariate_gcd.hh>
#include <poly_core/polynomial.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using algebra_core::RationalField;
using numerica_core::Rational;
using poly_core::Exponent;
using poly_core::Monomial;
using poly_core::MultivariatePolynomial;
using poly_core::rational_multivariate_gcd;

namespace {
constexpr const char* kNs = "poly_core_mv_gcd_tests";

using Poly = MultivariatePolynomial<RationalField>;

Poly term(const RationalField& field,
          std::vector<symbol_table::Symbol> vars,
          std::vector<Exponent> exponents,
          long long coeff) {
  return Poly::from_term(field, std::move(vars), Monomial(std::move(exponents)), Rational(coeff));
}
} // namespace

TEST_CASE("rational_multivariate_gcd: single-variable falls back to univariate behavior",
          "[poly_core][mv_gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x1");
  // gcd(x^2 - 1, x^2 - 3x + 2) == x - 1
  Poly a = term(field, {x}, {2}, 1) + term(field, {x}, {0}, -1);
  Poly b = term(field, {x}, {2}, 1) + term(field, {x}, {1}, -3) + term(field, {x}, {0}, 2);
  Poly g = rational_multivariate_gcd(field, a, b);
  Poly expected = term(field, {x}, {1}, 1) + term(field, {x}, {0}, -1);
  REQUIRE(g == expected);
}

TEST_CASE("rational_multivariate_gcd: two variables, shared linear factor", "[poly_core][mv_gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "mx1");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "my1");
  // a = (x + y) * (x - y) = x^2 - y^2
  // b = (x + y) * (x + 2y) = x^2 + 3xy + 2y^2
  // gcd should be (x + y), up to normalization.
  Poly x_plus_y = term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 1}, 1);
  Poly x_minus_y = term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 1}, -1);
  Poly x_plus_2y = term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 1}, 2);
  Poly a = x_plus_y * x_minus_y;
  Poly b = x_plus_y * x_plus_2y;

  Poly g = rational_multivariate_gcd(field, a, b);

  // Verify by exact division rather than asserting a specific normalized
  // form, and confirm g is a nontrivial (non-constant) common factor.
  auto [qa, ra] = poly_core::multivariate_div_rem(field, a, g);
  auto [qb, rb] = poly_core::multivariate_div_rem(field, b, g);
  REQUIRE(ra.is_zero());
  REQUIRE(rb.is_zero());
  REQUIRE(g.num_terms() == 2);
}

TEST_CASE("rational_multivariate_gcd: coprime polynomials give a constant (unit) gcd",
          "[poly_core][mv_gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "mx2");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "my2");
  Poly a = term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 0}, 1); // x + 1
  Poly b = term(field, {x, y}, {0, 1}, 1) + term(field, {x, y}, {0, 0}, 1); // y + 1
  Poly g = rational_multivariate_gcd(field, a, b);
  REQUIRE(g.num_terms() == 1);
  REQUIRE(g.leading_term().first.total_degree() == 0);
  REQUIRE(g.leading_term().second == Rational(1));
}

TEST_CASE("rational_multivariate_gcd: gcd(0, b) is monic(b)", "[poly_core][mv_gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "mx3");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "my3");
  Poly zero(field, {x, y});
  Poly b = term(field, {x, y}, {1, 0}, 2) + term(field, {x, y}, {0, 1}, 4); // 2x + 4y
  Poly g = rational_multivariate_gcd(field, zero, b);
  Poly expected = term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 1}, 2); // x + 2y
  REQUIRE(g == expected);
}

TEST_CASE("rational_multivariate_gcd: gcd(0, 0) is zero", "[poly_core][mv_gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "mx4");
  Poly zero(field, {x});
  Poly g = rational_multivariate_gcd(field, zero, zero);
  REQUIRE(g.is_zero());
}

TEST_CASE("rational_multivariate_gcd: rational (non-integer) coefficients round-trip",
          "[poly_core][mv_gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "mx5");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "my5");
  // a = (x/2 + y/3) * (x - y), b = (x/2 + y/3) * (x + y)
  Poly factor =
      Poly::from_term(field, {x, y}, Monomial(std::vector<Exponent>{1, 0}), Rational(1, 2)) +
      Poly::from_term(field, {x, y}, Monomial(std::vector<Exponent>{0, 1}), Rational(1, 3));
  Poly x_minus_y = term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 1}, -1);
  Poly x_plus_y = term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 1}, 1);
  Poly a = factor * x_minus_y;
  Poly b = factor * x_plus_y;

  Poly g = rational_multivariate_gcd(field, a, b);
  auto [qa, ra] = poly_core::multivariate_div_rem(field, a, g);
  auto [qb, rb] = poly_core::multivariate_div_rem(field, b, g);
  REQUIRE(ra.is_zero());
  REQUIRE(rb.is_zero());
  REQUIRE(g.num_terms() == 2);
}

TEST_CASE("rational_multivariate_gcd: three variables", "[poly_core][mv_gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "mx6");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "my6");
  symbol_table::Symbol z = symbol_table::get_symbol(kNs, "mz6");
  // shared factor (x + y + z), a = shared*(x - z), b = shared*(y + 1)
  Poly shared = term(field, {x, y, z}, {1, 0, 0}, 1) + term(field, {x, y, z}, {0, 1, 0}, 1) +
                term(field, {x, y, z}, {0, 0, 1}, 1);
  Poly x_minus_z = term(field, {x, y, z}, {1, 0, 0}, 1) + term(field, {x, y, z}, {0, 0, 1}, -1);
  Poly y_plus_1 = term(field, {x, y, z}, {0, 1, 0}, 1) + term(field, {x, y, z}, {0, 0, 0}, 1);
  Poly a = shared * x_minus_z;
  Poly b = shared * y_plus_1;

  Poly g = rational_multivariate_gcd(field, a, b);
  auto [qa, ra] = poly_core::multivariate_div_rem(field, a, g);
  auto [qb, rb] = poly_core::multivariate_div_rem(field, b, g);
  REQUIRE(ra.is_zero());
  REQUIRE(rb.is_zero());
  REQUIRE(g.num_terms() == 3);
}

TEST_CASE("rational_multivariate_gcd: mismatched variable lists throw", "[poly_core][mv_gcd]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "mx7");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "my7");
  Poly a = term(field, {x}, {1}, 1);
  Poly b = term(field, {y}, {1}, 1);
  REQUIRE_THROWS_AS(rational_multivariate_gcd(field, a, b), std::invalid_argument);
}
