#include <stdexcept>
#include <vector>

#include <algebra_core/concepts.hh>
#include <algebra_core/rings.hh>
#include <numerica_core/rational.hh>
#include <poly_core/monomial.hh>
#include <poly_core/polynomial.hh>
#include <poly_core/rational_polynomial.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using algebra_core::RationalField;
using numerica_core::Rational;
using poly_core::Exponent;
using poly_core::Monomial;
using poly_core::RationalPoly;
using poly_core::RationalPolynomial;
using poly_core::RationalPolynomialField;

static_assert(algebra_core::Field<RationalPolynomialField>);
static_assert(algebra_core::EuclideanDomain<RationalPolynomialField>);

namespace {
constexpr const char* kNs = "poly_core_rational_polynomial_tests";

RationalPoly term(const RationalField& field,
                  std::vector<symbol_table::Symbol> vars,
                  std::vector<Exponent> exponents,
                  long long coeff) {
  return RationalPoly::from_term(
      field, std::move(vars), Monomial(std::move(exponents)), Rational(coeff));
}
} // namespace

TEST_CASE("RationalPolynomial: constructing from unreduced input reduces to lowest terms",
          "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x1");
  // (x^2 - 1) / (x - 1) reduces to (x + 1) / 1
  RationalPoly num = term(field, {x}, {2}, 1) + term(field, {x}, {0}, -1);
  RationalPoly den = term(field, {x}, {1}, 1) + term(field, {x}, {0}, -1);
  RationalPolynomial r(field, num, den);

  RationalPoly expected_num = term(field, {x}, {1}, 1) + term(field, {x}, {0}, 1);
  RationalPoly expected_den = RationalPoly::constant(field, {x}, Rational(1));
  REQUIRE(r.numerator() == expected_num);
  REQUIRE(r.denominator() == expected_den);
}

TEST_CASE("RationalPolynomial: denominator is normalized to monic",
          "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x2");
  // 1 / (2x) should normalize to (1/2) / x
  RationalPoly num = RationalPoly::constant(field, {x}, Rational(1));
  RationalPoly den = term(field, {x}, {1}, 2);
  RationalPolynomial r(field, num, den);

  REQUIRE(r.denominator() == term(field, {x}, {1}, 1));
  REQUIRE(r.numerator() == RationalPoly::constant(field, {x}, Rational(1, 2)));
}

TEST_CASE("RationalPolynomial: zero denominator throws", "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x3");
  RationalPoly num = term(field, {x}, {1}, 1);
  RationalPoly zero(field, {x});
  REQUIRE_THROWS_AS(RationalPolynomial(field, num, zero), std::domain_error);
}

TEST_CASE("RationalPolynomial: from_polynomial has denominator 1",
          "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x4");
  RationalPoly num = term(field, {x}, {2}, 3) + term(field, {x}, {0}, 1);
  RationalPolynomial r = RationalPolynomial::from_polynomial(field, num);
  REQUIRE(r.numerator() == num);
  REQUIRE(r.denominator() == RationalPoly::constant(field, {x}, Rational(1)));
}

TEST_CASE("RationalPolynomial: addition of 1/x + 1/y", "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x5");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y5");
  RationalPolynomial one_over_x(
      field, RationalPoly::constant(field, {x, y}, Rational(1)), term(field, {x, y}, {1, 0}, 1));
  RationalPolynomial one_over_y(
      field, RationalPoly::constant(field, {x, y}, Rational(1)), term(field, {x, y}, {0, 1}, 1));
  RationalPolynomial sum = one_over_x + one_over_y;

  RationalPoly expected_num = term(field, {x, y}, {0, 1}, 1) + term(field, {x, y}, {1, 0}, 1);
  RationalPoly expected_den = term(field, {x, y}, {1, 1}, 1);
  RationalPolynomial expected(field, expected_num, expected_den);
  REQUIRE(sum == expected);
}

TEST_CASE("RationalPolynomial: multiplication and division are inverse operations",
          "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x6");
  RationalPolynomial a(
      field, term(field, {x}, {1}, 1) + term(field, {x}, {0}, 1), term(field, {x}, {1}, 1));
  RationalPolynomial b(field, term(field, {x}, {1}, 1), term(field, {x}, {0}, 3));

  RationalPolynomial product = a * b;
  RationalPolynomial back = product / b;
  REQUIRE(back == a);
}

TEST_CASE("RationalPolynomial: division by zero throws", "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x7");
  RationalPolynomial a = RationalPolynomial::from_polynomial(field, term(field, {x}, {1}, 1));
  RationalPolynomial zero = RationalPolynomial::from_polynomial(field, RationalPoly(field, {x}));
  REQUIRE_THROWS_AS(a / zero, std::domain_error);
}

TEST_CASE("RationalPolynomial: double negation is identity", "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x8");
  RationalPolynomial a(
      field, term(field, {x}, {1}, 1) + term(field, {x}, {0}, 1), term(field, {x}, {1}, 2));
  REQUIRE(-(-a) == a);
  REQUIRE_FALSE(-a == a);
}

TEST_CASE("RationalPolynomialField: zero/one identities", "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x9");
  RationalPolynomialField ring(field, {x});
  RationalPolynomial a(
      field, term(field, {x}, {1}, 1) + term(field, {x}, {0}, 5), term(field, {x}, {1}, 1));

  REQUIRE(ring.add(a, ring.zero()) == a);
  REQUIRE(ring.mul(a, ring.one()) == a);
  REQUIRE(ring.sub(a, a) == ring.zero());
}

TEST_CASE("RationalPolynomialField: inv(a) * a == one", "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x10");
  RationalPolynomialField ring(field, {x});
  RationalPolynomial a(field,
                       term(field, {x}, {1}, 1) + term(field, {x}, {0}, 5),
                       term(field, {x}, {1}, 1) + term(field, {x}, {0}, 1));

  RationalPolynomial inv_a = ring.inv(a);
  REQUIRE(ring.mul(inv_a, a) == ring.one());
}

TEST_CASE("RationalPolynomialField: inv(zero) throws", "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x11");
  RationalPolynomialField ring(field, {x});
  REQUIRE_THROWS_AS(ring.inv(ring.zero()), std::domain_error);
}

TEST_CASE("RationalPolynomialField: div_rem is exact with a zero remainder",
          "[poly_core][rational_polynomial]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x12");
  RationalPolynomialField ring(field, {x});
  RationalPolynomial a(field, term(field, {x}, {1}, 1), term(field, {x}, {0}, 1));
  RationalPolynomial b(field, term(field, {x}, {0}, 2), term(field, {x}, {0}, 1));

  auto [quotient, remainder] = ring.div_rem(a, b);
  REQUIRE(remainder == ring.zero());
  REQUIRE(ring.mul(quotient, b) == a);
}
