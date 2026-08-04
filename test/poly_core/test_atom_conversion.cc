#include <stdexcept>
#include <vector>

#include <algebra_core/rings.hh>
#include <atom_core/atom.hh>
#include <atom_core/normalize.hh>
#include <atom_core/parser.hh>
#include <numerica_core/rational.hh>
#include <poly_core/atom_conversion.hh>
#include <poly_core/monomial.hh>
#include <poly_core/polynomial.hh>
#include <poly_core/rational_polynomial.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using algebra_core::RationalField;
using atom_core::Atom;
using atom_core::AtomStore;
using numerica_core::Rational;
using poly_core::atom_to_polynomial;
using poly_core::atom_to_rational_polynomial;
using poly_core::Exponent;
using poly_core::Monomial;
using poly_core::polynomial_to_atom;
using poly_core::rational_polynomial_to_atom;
using poly_core::RationalPoly;
using poly_core::RationalPolynomial;

namespace {
constexpr const char* kNs = "poly_core_atom_conversion_tests";

RationalPoly term(const RationalField& field,
                  std::vector<symbol_table::Symbol> vars,
                  std::vector<Exponent> exponents,
                  long long coeff) {
  return RationalPoly::from_term(
      field, std::move(vars), Monomial(std::move(exponents)), Rational(coeff));
}
} // namespace

TEST_CASE("atom_to_polynomial: simple sum", "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x1");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y1");
  Atom atom = atom_core::parse("x1 + y1", store, kNs);

  RationalPoly poly = atom_to_polynomial(atom.view(), field, {x, y});
  RationalPoly expected = term(field, {x, y}, {1, 0}, 1) + term(field, {x, y}, {0, 1}, 1);
  REQUIRE(poly == expected);
}

TEST_CASE("atom_to_polynomial: expansion via normalize matches manual expansion",
          "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x2");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y2");
  // (x+y)^2, not pre-expanded - atom_to_polynomial must expand Pow itself.
  Atom atom = atom_core::parse("(x2 + y2)^2", store, kNs);

  RationalPoly poly = atom_to_polynomial(atom.view(), field, {x, y});
  RationalPoly expected = term(field, {x, y}, {2, 0}, 1) + term(field, {x, y}, {1, 1}, 2) +
                          term(field, {x, y}, {0, 2}, 1);
  REQUIRE(poly == expected);
}

TEST_CASE("atom_to_polynomial: rejects function calls", "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x3");
  Atom atom = atom_core::parse("f(x3)", store, kNs);
  REQUIRE_THROWS_AS(atom_to_polynomial(atom.view(), field, {x}), std::invalid_argument);
}

TEST_CASE("atom_to_polynomial: rejects negative exponents", "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x4");
  Atom atom = atom_core::parse("1 / x4", store, kNs);
  REQUIRE_THROWS_AS(atom_to_polynomial(atom.view(), field, {x}), std::invalid_argument);
}

TEST_CASE("atom_to_polynomial: rejects unknown variables", "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x5");
  [[maybe_unused]] symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y5");
  Atom atom = atom_core::parse("x5 + y5", store, kNs);
  REQUIRE_THROWS_AS(atom_to_polynomial(atom.view(), field, {x}), std::invalid_argument);
}

TEST_CASE("atom_to_rational_polynomial: simple reciprocal", "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x6");
  Atom atom = atom_core::parse("1 / x6", store, kNs);

  RationalPolynomial rp = atom_to_rational_polynomial(atom.view(), field, {x});
  REQUIRE(rp.numerator() == RationalPoly::constant(field, {x}, Rational(1)));
  REQUIRE(rp.denominator() == term(field, {x}, {1}, 1));
}

TEST_CASE("atom_to_rational_polynomial: negative exponent nested inside Add",
          "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x7");
  // 1 + 1/x = (x + 1) / x
  Atom atom = atom_core::parse("1 + 1/x7", store, kNs);

  RationalPolynomial rp = atom_to_rational_polynomial(atom.view(), field, {x});
  RationalPolynomial expected(
      field, term(field, {x}, {1}, 1) + term(field, {x}, {0}, 1), term(field, {x}, {1}, 1));
  REQUIRE(rp == expected);
}

TEST_CASE("atom_to_rational_polynomial: rejects function calls", "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x8");
  Atom atom = atom_core::parse("1 / f(x8)", store, kNs);
  REQUIRE_THROWS_AS(atom_to_rational_polynomial(atom.view(), field, {x}), std::invalid_argument);
}

TEST_CASE("polynomial_to_atom: zero polynomial becomes Num(0)", "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x9");
  RationalPoly zero(field, {x});
  Atom atom = polynomial_to_atom(zero, store);
  REQUIRE(atom.view().tag() == atom_core::AtomTag::Num);
  REQUIRE(atom.view().as_num() == Rational(0));
}

TEST_CASE("polynomial_to_atom: round-trips through atom_to_polynomial",
          "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x10");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y10");
  Atom original = atom_core::parse("x10^2 + 2*x10*y10 + y10^2", store, kNs);
  RationalPoly poly = atom_to_polynomial(original.view(), field, {x, y});

  Atom rebuilt = polynomial_to_atom(poly, store);
  RationalPoly poly_again = atom_to_polynomial(rebuilt.view(), field, {x, y});
  REQUIRE(poly == poly_again);

  Atom normalized_original = atom_core::normalize(store, original.view());
  REQUIRE(rebuilt.view() == normalized_original.view());
}

TEST_CASE("rational_polynomial_to_atom: denominator 1 collapses to bare numerator atom",
          "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x11");
  RationalPolynomial rp = RationalPolynomial::from_polynomial(field, term(field, {x}, {1}, 1));
  Atom atom = rational_polynomial_to_atom(rp, store);
  REQUIRE(atom.view().tag() == atom_core::AtomTag::Var);
}

TEST_CASE("rational_polynomial_to_atom: nontrivial denominator round-trips",
          "[poly_core][atom_conversion]") {
  RationalField field;
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x12");
  Atom original = atom_core::parse("x12 / (x12 + 1)", store, kNs);
  RationalPolynomial rp = atom_to_rational_polynomial(original.view(), field, {x});

  Atom rebuilt = rational_polynomial_to_atom(rp, store);
  RationalPolynomial rp_again = atom_to_rational_polynomial(rebuilt.view(), field, {x});
  REQUIRE(rp == rp_again);
}
