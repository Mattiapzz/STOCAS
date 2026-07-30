#include <cstdint>
#include <stdexcept>

#include <numerica_core/finite_field.hh>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

using numerica_core::FiniteField;

TEMPLATE_TEST_CASE("FiniteField arithmetic matches brute-force mod arithmetic for a small prime",
                   "[numerica_core][finite_field]",
                   std::uint32_t,
                   std::uint64_t) {
  constexpr TestType kModulus = 13;

  for (TestType a = 0; a < kModulus; ++a) {
    for (TestType b = 0; b < kModulus; ++b) {
      const FiniteField<TestType> fa(kModulus, a);
      const FiniteField<TestType> fb(kModulus, b);

      CAPTURE(a, b);
      REQUIRE((fa + fb).value() == (a + b) % kModulus);
      REQUIRE((fa * fb).value() == (a * b) % kModulus);
      REQUIRE((fa - fb).value() == (a + kModulus - b) % kModulus);
    }
  }
}

TEMPLATE_TEST_CASE("FiniteField inverse satisfies a * a.inv() == 1",
                   "[numerica_core][finite_field]",
                   std::uint32_t,
                   std::uint64_t) {
  constexpr TestType kModulus = 101; // prime
  for (TestType v = 1; v < kModulus; ++v) {
    const FiniteField<TestType> a(kModulus, v);
    CAPTURE(v);
    REQUIRE((a * a.inv()).value() == 1);
  }
}

TEMPLATE_TEST_CASE("FiniteField zero has no inverse",
                   "[numerica_core][finite_field]",
                   std::uint32_t,
                   std::uint64_t) {
  const FiniteField<TestType> zero(TestType{101}, TestType{0});
  REQUIRE_THROWS_AS(zero.inv(), std::domain_error);
}

TEST_CASE("FiniteField<u64> is overflow-safe for a modulus near 2^64",
          "[numerica_core][finite_field]") {
  // Largest known prime below 2^64: 2^64 - 59.
  constexpr std::uint64_t kModulus = 18446744073709551557ULL;
  const FiniteField<std::uint64_t> a(kModulus, kModulus - 1);
  const FiniteField<std::uint64_t> b(kModulus, kModulus - 1);

  // (p-1)*(p-1) mod p == 1, since (p-1) == -1 (mod p).
  REQUIRE((a * b).value() == 1);

  const FiniteField<std::uint64_t> inverse = a.inv();
  REQUIRE((a * inverse).value() == 1);
}

TEST_CASE("FiniteField negation", "[numerica_core][finite_field]") {
  const FiniteField<std::uint32_t> a(7U, 3U);
  REQUIRE((-a).value() == 4U);
  const FiniteField<std::uint32_t> zero(7U, 0U);
  REQUIRE((-zero).value() == 0U);
}
