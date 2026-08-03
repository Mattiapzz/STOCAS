#include <stdexcept>
#include <vector>

#include <algebra_core/rings.hh>
#include <numerica_core/rational.hh>
#include <poly_core/monomial.hh>
#include <poly_core/polynomial.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using algebra_core::RationalField;
using numerica_core::Rational;
using poly_core::Exponent;
using poly_core::Monomial;
using poly_core::MultivariatePolynomial;

namespace {
constexpr const char* kNs = "poly_core_polynomial_tests";

using Poly = MultivariatePolynomial<RationalField>;

Poly term(const RationalField& field,
          std::vector<symbol_table::Symbol> vars,
          std::vector<Exponent> exponents,
          long long coeff) {
  return Poly::from_term(field, std::move(vars), Monomial(std::move(exponents)), Rational(coeff));
}
} // namespace

TEST_CASE("MultivariatePolynomial: zero-coefficient term is dropped", "[poly_core][polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x");
  Poly p = Poly::from_term(field, {x}, Monomial(std::vector<Exponent>{1}), Rational(0));
  REQUIRE(p.is_zero());
  REQUIRE(p.num_terms() == 0);
}

TEST_CASE("MultivariatePolynomial: addition combines like monomials, cancels to zero",
          "[poly_core][polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x");
  Poly a = term(field, {x}, {2}, 3);  // 3x^2
  Poly b = term(field, {x}, {2}, -3); // -3x^2
  Poly sum = a + b;
  REQUIRE(sum.is_zero());

  Poly c = term(field, {x}, {2}, 5); // 5x^2
  Poly combined = a + c;             // 8x^2
  REQUIRE(combined.num_terms() == 1);
  REQUIRE(combined.leading_term().second == Rational(8));
}

TEST_CASE("MultivariatePolynomial: subtraction and unary negation", "[poly_core][polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x");
  Poly a = term(field, {x}, {1}, 5); // 5x
  Poly b = term(field, {x}, {1}, 2); // 2x
  Poly diff = a - b;                 // 3x
  REQUIRE(diff.num_terms() == 1);
  REQUIRE(diff.leading_term().second == Rational(3));

  Poly negated = -a;
  REQUIRE(negated.leading_term().second == Rational(-5));
}

TEST_CASE("MultivariatePolynomial: multiplication distributes and sorts by leading term",
          "[poly_core][polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x");
  // (x + 1) * (x - 1) = x^2 - 1
  Poly x_poly = term(field, {x}, {1}, 1);
  Poly one = Poly::constant(field, {x}, Rational(1));
  Poly x_plus_1 = x_poly + one;
  Poly x_minus_1 = x_poly - one;
  Poly product = x_plus_1 * x_minus_1;

  REQUIRE(product.num_terms() == 2);
  REQUIRE(product.leading_term().first.exponent(0) == 2);
  REQUIRE(product.leading_term().second == Rational(1));
  REQUIRE(product.terms()[1].first.exponent(0) == 0);
  REQUIRE(product.terms()[1].second == Rational(-1));
}

TEST_CASE("MultivariatePolynomial: multivariate multiplication", "[poly_core][polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "mvx");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "mvy");
  // (x + y) * (x + y) = x^2 + 2xy + y^2
  Poly x_poly = term(field, {x, y}, {1, 0}, 1);
  Poly y_poly = term(field, {x, y}, {0, 1}, 1);
  Poly sum = x_poly + y_poly;
  Poly squared = sum * sum;

  REQUIRE(squared.num_terms() == 3);
  REQUIRE(squared.leading_term().first == Monomial(std::vector<Exponent>{2, 0}));
  REQUIRE(squared.leading_term().second == Rational(1));
  REQUIRE(squared.terms()[1].first == Monomial(std::vector<Exponent>{1, 1}));
  REQUIRE(squared.terms()[1].second == Rational(2));
  REQUIRE(squared.terms()[2].first == Monomial(std::vector<Exponent>{0, 2}));
  REQUIRE(squared.terms()[2].second == Rational(1));
}

TEST_CASE("MultivariatePolynomial: mismatched variable lists throw", "[poly_core][polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "mmx");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "mmy");
  Poly a = term(field, {x}, {1}, 1);
  Poly b = term(field, {y}, {1}, 1);
  REQUIRE_THROWS_AS(a + b, std::invalid_argument);
  REQUIRE_THROWS_AS(a * b, std::invalid_argument);
}
