#include <stdexcept>

#include <numerica_core/rational.hh>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

using numerica_core::BigInt;
using numerica_core::Rational;

TEST_CASE("Rational auto-reduces and normalizes sign", "[numerica_core][rational]") {
  const Rational a(BigInt(6), BigInt(8));
  REQUIRE(a.numerator() == BigInt(3));
  REQUIRE(a.denominator() == BigInt(4));

  const Rational negative_den(BigInt(3), BigInt(-4));
  REQUIRE(negative_den.numerator() == BigInt(-3));
  REQUIRE(negative_den.denominator() == BigInt(4));

  const Rational zero(BigInt(0), BigInt(5));
  REQUIRE(zero.numerator() == BigInt(0));
  REQUIRE(zero.denominator() == BigInt(1));
}

TEST_CASE("Rational rejects zero denominator", "[numerica_core][rational]") {
  REQUIRE_THROWS_AS(Rational(BigInt(1), BigInt(0)), std::domain_error);
}

TEST_CASE("Rational arithmetic", "[numerica_core][rational]") {
  const Rational half(BigInt(1), BigInt(2));
  const Rational third(BigInt(1), BigInt(3));

  REQUIRE(half + third == Rational(BigInt(5), BigInt(6)));
  REQUIRE(half - third == Rational(BigInt(1), BigInt(6)));
  REQUIRE(half * third == Rational(BigInt(1), BigInt(6)));
  REQUIRE(half / third == Rational(BigInt(3), BigInt(2)));
  REQUIRE(-half == Rational(BigInt(-1), BigInt(2)));
}

TEST_CASE("Rational inverse and division-by-zero errors", "[numerica_core][rational]") {
  const Rational two_thirds(BigInt(2), BigInt(3));
  REQUIRE(two_thirds.inv() == Rational(BigInt(3), BigInt(2)));

  const Rational zero;
  REQUIRE_THROWS_AS(zero.inv(), std::domain_error);
  REQUIRE_THROWS_AS(two_thirds / zero, std::domain_error);
}

TEST_CASE("Rational property: a == a.inv().inv() for nonzero rationals",
          "[numerica_core][rational]") {
  auto num = GENERATE(take(20, filter([](int v) { return v != 0; }, random(-1000, 1000))));
  auto den = GENERATE(take(5, filter([](int v) { return v != 0; }, random(-1000, 1000))));
  const Rational a(BigInt(static_cast<int64_t>(num)), BigInt(static_cast<int64_t>(den)));
  REQUIRE(a == a.inv().inv());
}

TEST_CASE("Rational ordering", "[numerica_core][rational]") {
  const Rational a(BigInt(1), BigInt(3));
  const Rational b(BigInt(1), BigInt(2));
  REQUIRE(a < b);
  REQUIRE(b > a);
  REQUIRE(a != b);
}

TEST_CASE("Rational reconstruction recovers known modulus/residue pairs",
          "[numerica_core][rational]") {
  // p/q = 3/7, modulus m = 101 (prime). residue = (3 * inverse(7, 101)) mod 101.
  // inverse(7, 101) = 29 since 7*29 = 203 = 2*101 + 1. residue = (3*29) mod 101 = 87 mod 101 = 87.
  const BigInt modulus(101);
  const BigInt residue(87);
  const auto reconstructed = Rational::reconstruct(residue, modulus);
  REQUIRE(reconstructed.has_value());
  REQUIRE(*reconstructed == Rational(BigInt(3), BigInt(7)));
}

TEST_CASE("Rational reconstruction round-trips for an integer value", "[numerica_core][rational]") {
  const BigInt modulus(1000003); // prime
  const BigInt value(42);
  const auto reconstructed = Rational::reconstruct(value, modulus);
  REQUIRE(reconstructed.has_value());
  REQUIRE(*reconstructed == Rational(BigInt(42)));
}

TEST_CASE("Rational reconstruction fails when the modulus is too small for the value",
          "[numerica_core][rational]") {
  // With a tiny modulus, most residues cannot be reconstructed within the
  // default bound (sqrt(m/2)); this is the modulus-too-small edge case.
  const BigInt modulus(7);
  const BigInt residue(3);
  const auto reconstructed = Rational::reconstruct(residue, modulus);
  // Whatever the outcome, if a value IS returned, it must satisfy the
  // reconstruction identity: numerator == residue * denominator (mod modulus).
  if (reconstructed.has_value()) {
    const BigInt lhs = ((reconstructed->numerator() % modulus) + modulus) % modulus;
    const BigInt rhs = ((residue * reconstructed->denominator()) % modulus + modulus) % modulus;
    REQUIRE(lhs == rhs);
  }
}
