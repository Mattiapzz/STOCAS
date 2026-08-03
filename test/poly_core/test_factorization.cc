#include <cstdint>
#include <stdexcept>
#include <vector>

#include <algebra_core/rings.hh>
#include <numerica_core/finite_field.hh>
#include <poly_core/factorization.hh>
#include <poly_core/monomial.hh>
#include <poly_core/polynomial.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using poly_core::factor_univariate_finite_field;
using poly_core::Factorization;
using poly_core::FactorTerm;
using poly_core::FiniteFieldElement64;
using poly_core::FiniteFieldPoly;
using poly_core::FiniteFieldRing64;
using poly_core::Monomial;

namespace {
constexpr const char* kNs = "poly_core_factorization_tests";

FiniteFieldPoly linear(const FiniteFieldRing64& field,
                       symbol_table::Symbol x,
                       std::uint64_t prime,
                       std::int64_t root) {
  // (x - root)
  std::int64_t normalized = root % static_cast<std::int64_t>(prime);
  if (normalized < 0) {
    normalized += static_cast<std::int64_t>(prime);
  }
  FiniteFieldElement64 neg_root =
      -FiniteFieldElement64(prime, static_cast<std::uint64_t>(normalized));
  return FiniteFieldPoly::from_term(
             field, {x}, Monomial(std::vector<poly_core::Exponent>{1}), field.one()) +
         FiniteFieldPoly::from_term(
             field, {x}, Monomial(std::vector<poly_core::Exponent>{0}), neg_root);
}

// Reconstructs leading_coefficient * product(factor^multiplicity) and
// compares to the original polynomial.
FiniteFieldPoly reconstruct(const FiniteFieldRing64& field,
                            const std::vector<symbol_table::Symbol>& vars,
                            const Factorization& factorization) {
  FiniteFieldPoly product =
      FiniteFieldPoly::constant(field, vars, factorization.leading_coefficient);
  for (const FactorTerm& term : factorization.factors) {
    for (unsigned i = 0; i < term.multiplicity; ++i) {
      product = product * term.factor;
    }
  }
  return product;
}
} // namespace

TEST_CASE("factor_univariate_finite_field: distinct linear roots", "[poly_core][factorization]") {
  constexpr std::uint64_t kPrime = 101;
  FiniteFieldRing64 field(kPrime);
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x1");

  FiniteFieldPoly a = linear(field, x, kPrime, 1);
  FiniteFieldPoly b = linear(field, x, kPrime, 2);
  FiniteFieldPoly c = linear(field, x, kPrime, 3);
  FiniteFieldPoly f = a * b * c;

  Factorization factorization = factor_univariate_finite_field(field, kPrime, f);
  REQUIRE(factorization.factors.size() == 3);
  for (const FactorTerm& term : factorization.factors) {
    REQUIRE(term.multiplicity == 1);
    REQUIRE(term.factor.degree_in(0) == 1);
  }
  REQUIRE(reconstruct(field, {x}, factorization) == f);
}

TEST_CASE("factor_univariate_finite_field: repeated root has correct multiplicity",
          "[poly_core][factorization]") {
  constexpr std::uint64_t kPrime = 101;
  FiniteFieldRing64 field(kPrime);
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x2");

  FiniteFieldPoly a = linear(field, x, kPrime, 1);
  FiniteFieldPoly b = linear(field, x, kPrime, 2);
  FiniteFieldPoly f = a * a * a * b; // (x-1)^3 * (x-2)

  Factorization factorization = factor_univariate_finite_field(field, kPrime, f);
  REQUIRE(factorization.factors.size() == 2);
  unsigned total_degree = 0;
  for (const FactorTerm& term : factorization.factors) {
    REQUIRE(term.factor.degree_in(0) == 1);
    total_degree += term.multiplicity;
    if (term.multiplicity == 3) {
      REQUIRE(term.factor == a);
    } else {
      REQUIRE(term.multiplicity == 1);
      REQUIRE(term.factor == b);
    }
  }
  REQUIRE(total_degree == 4);
  REQUIRE(reconstruct(field, {x}, factorization) == f);
}

TEST_CASE("factor_univariate_finite_field: irreducible quadratic is not split",
          "[poly_core][factorization]") {
  // x^2 + 1 is irreducible over F_7 (7 == 3 mod 4, so -1 is not a QR mod 7).
  constexpr std::uint64_t kPrime = 7;
  FiniteFieldRing64 field(kPrime);
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x3");
  FiniteFieldPoly f = FiniteFieldPoly::from_term(
                          field, {x}, Monomial(std::vector<poly_core::Exponent>{2}), field.one()) +
                      FiniteFieldPoly::from_term(
                          field, {x}, Monomial(std::vector<poly_core::Exponent>{0}), field.one());

  Factorization factorization = factor_univariate_finite_field(field, kPrime, f);
  REQUIRE(factorization.factors.size() == 1);
  REQUIRE(factorization.factors[0].multiplicity == 1);
  REQUIRE(factorization.factors[0].factor.degree_in(0) == 2);
  REQUIRE(reconstruct(field, {x}, factorization) == f);
}

TEST_CASE("factor_univariate_finite_field: nontrivial leading coefficient is preserved",
          "[poly_core][factorization]") {
  constexpr std::uint64_t kPrime = 101;
  FiniteFieldRing64 field(kPrime);
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x4");
  FiniteFieldPoly a = linear(field, x, kPrime, 5);
  FiniteFieldPoly scale = FiniteFieldPoly::constant(field, {x}, FiniteFieldElement64(kPrime, 3));
  FiniteFieldPoly f = scale * a;

  Factorization factorization = factor_univariate_finite_field(field, kPrime, f);
  REQUIRE(factorization.leading_coefficient == FiniteFieldElement64(kPrime, 3));
  REQUIRE(factorization.factors.size() == 1);
  REQUIRE(reconstruct(field, {x}, factorization) == f);
}

TEST_CASE("factor_univariate_finite_field: rejects the zero polynomial",
          "[poly_core][factorization]") {
  constexpr std::uint64_t kPrime = 101;
  FiniteFieldRing64 field(kPrime);
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x5");
  FiniteFieldPoly zero(field, {x});
  REQUIRE_THROWS_AS(factor_univariate_finite_field(field, kPrime, zero), std::invalid_argument);
}

TEST_CASE("factor_univariate_finite_field: rejects multivariate input",
          "[poly_core][factorization]") {
  constexpr std::uint64_t kPrime = 101;
  FiniteFieldRing64 field(kPrime);
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x6");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y6");
  FiniteFieldPoly f = FiniteFieldPoly::from_term(
      field, {x, y}, Monomial(std::vector<poly_core::Exponent>{1, 1}), field.one());
  REQUIRE_THROWS_AS(factor_univariate_finite_field(field, kPrime, f), std::invalid_argument);
}

TEST_CASE("factor_univariate_finite_field: rejects prime == 2", "[poly_core][factorization]") {
  constexpr std::uint64_t kPrime = 2;
  FiniteFieldRing64 field(kPrime);
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x7");
  FiniteFieldPoly f = FiniteFieldPoly::from_term(
      field, {x}, Monomial(std::vector<poly_core::Exponent>{1}), field.one());
  REQUIRE_THROWS_AS(factor_univariate_finite_field(field, kPrime, f), std::invalid_argument);
}

TEST_CASE("factor_univariate_finite_field: five distinct roots", "[poly_core][factorization]") {
  constexpr std::uint64_t kPrime = 101;
  FiniteFieldRing64 field(kPrime);
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x8");

  FiniteFieldPoly f = FiniteFieldPoly::constant(field, {x}, field.one());
  for (std::int64_t root = 1; root <= 5; ++root) {
    f = f * linear(field, x, kPrime, root);
  }

  Factorization factorization = factor_univariate_finite_field(field, kPrime, f);
  REQUIRE(factorization.factors.size() == 5);
  for (const FactorTerm& term : factorization.factors) {
    REQUIRE(term.multiplicity == 1);
    REQUIRE(term.factor.degree_in(0) == 1);
  }
  REQUIRE(reconstruct(field, {x}, factorization) == f);
}
