#include <cstdint>
#include <optional>
#include <vector>

#include <algebra_core/rings.hh>
#include <numerica_core/big_int.hh>
#include <numerica_core/rational.hh>
#include <poly_core/monomial.hh>
#include <poly_core/multivariate_gcd.hh>
#include <poly_core/polynomial.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

/// @file
/// @brief Finite-field evaluation/interpolation roundtrip tests (M4-GATE
/// testing gap, filled 2026-08-04): reduce a MultivariatePolynomial<Rational>
/// mod several primes and reconstruct it, verifying the roundtrip across
/// each of poly_core::kMultivariateGcdPrimes plus a small custom prime, and
/// exercising the "modulus too small for degree" edge case directly at the
/// level it actually matters - Rational::reconstruct()'s bound, which
/// shrinks with the modulus (~sqrt(modulus/2)).
using algebra_core::RationalField;
using numerica_core::BigInt;
using numerica_core::Rational;
using poly_core::Exponent;
using poly_core::Monomial;
using poly_core::MultivariatePolynomial;

namespace {
constexpr const char* kNs = "poly_core_finite_field_roundtrip_tests";
using Poly = MultivariatePolynomial<RationalField>;
} // namespace

TEST_CASE("finite-field roundtrip: small integer coefficients across every kMultivariateGcdPrimes "
          "prime, plus a small custom prime",
          "[poly_core][finite_field_roundtrip]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x1");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y1");
  std::vector<symbol_table::Symbol> vars{x, y};

  Poly original =
      Poly::from_term(field, vars, Monomial(std::vector<Exponent>{2, 1}), Rational(7)) +
      Poly::from_term(field, vars, Monomial(std::vector<Exponent>{0, 3}), Rational(-11)) +
      Poly::from_term(field, vars, Monomial(std::vector<Exponent>{1, 0}), Rational(1, 3));

  std::vector<std::uint64_t> primes{
      poly_core::kMultivariateGcdPrimes[0], poly_core::kMultivariateGcdPrimes[1], 10007ULL};
  for (std::uint64_t prime : primes) {
    INFO("prime " << prime);
    algebra_core::FiniteFieldRing<std::uint64_t> field_ring(prime);
    auto reduced = poly_core::detail::reduce_polynomial_mod(original, field_ring, prime);
    std::optional<Poly> reconstructed =
        poly_core::detail::reconstruct_polynomial(reduced, field, prime);
    REQUIRE(reconstructed.has_value());
    REQUIRE(*reconstructed == original);
  }
}

TEST_CASE("finite-field roundtrip: modulus-too-small-for-degree - reconstruction fails cleanly "
          "rather than returning a wrong value",
          "[poly_core][finite_field_roundtrip]") {
  // Rational::reconstruct()'s default bound is floor(sqrt(modulus / 2)); a
  // coefficient larger than that bound cannot be recovered from a residue
  // mod a small prime, and must report failure (nullopt), never a silently
  // wrong rational.
  constexpr std::uint64_t kSmallPrime = 101; // bound ~= sqrt(50) ~= 7
  BigInt modulus(static_cast<std::int64_t>(kSmallPrime));

  Rational oversized_coefficient(1000); // far beyond the ~7 bound for this modulus
  BigInt residue = oversized_coefficient.numerator() % modulus;
  if (residue < BigInt(0)) {
    residue = residue + modulus;
  }
  std::optional<Rational> reconstructed = Rational::reconstruct(residue, modulus);
  REQUIRE_FALSE(reconstructed.has_value());
}

TEST_CASE("finite-field roundtrip: modulus-too-small-for-degree propagates through "
          "reconstruct_polynomial",
          "[poly_core][finite_field_roundtrip]") {
  RationalField field;
  algebra_core::FiniteFieldRing<std::uint64_t> field_ring(101);
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x2");

  Poly original = Poly::from_term(field, {x}, Monomial(std::vector<Exponent>{1}), Rational(1000));
  auto reduced = poly_core::detail::reduce_polynomial_mod(original, field_ring, 101);
  std::optional<Poly> reconstructed =
      poly_core::detail::reconstruct_polynomial(reduced, field, 101);
  REQUIRE_FALSE(reconstructed.has_value());
}

TEST_CASE("finite-field roundtrip: rational_multivariate_gcd stays correct with larger "
          "coefficients across its real (large) prime list",
          "[poly_core][finite_field_roundtrip]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x3");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y3");
  std::vector<symbol_table::Symbol> vars{x, y};

  Poly shared =
      Poly::from_term(field, vars, Monomial(std::vector<Exponent>{1, 0}), Rational(1000000)) +
      Poly::from_term(field, vars, Monomial(std::vector<Exponent>{0, 1}), Rational(1));
  Poly a =
      shared * (Poly::from_term(field, vars, Monomial(std::vector<Exponent>{1, 0}), Rational(1)) -
                Poly::constant(field, vars, Rational(1)));
  Poly b =
      shared * (Poly::from_term(field, vars, Monomial(std::vector<Exponent>{1, 0}), Rational(1)) +
                Poly::constant(field, vars, Rational(1)));

  Poly g = poly_core::rational_multivariate_gcd(field, a, b);
  auto [quotient_a, remainder_a] = poly_core::multivariate_div_rem(field, a, g);
  auto [quotient_b, remainder_b] = poly_core::multivariate_div_rem(field, b, g);
  REQUIRE(remainder_a.is_zero());
  REQUIRE(remainder_b.is_zero());
  REQUIRE(g.num_terms() == 2);
}
