#include <cmath>

#include <numerica_core/error_propagating_float.hh>
#include <numerica_core/float_types.hh>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using numerica_core::ErrorPropagatingFloat;
using numerica_core::F64;
using numerica_core::Float;

TEST_CASE("ErrorPropagatingFloat propagates full precision through +, *, /",
          "[numerica_core][error_propagating_float]") {
  const ErrorPropagatingFloat<F64> a(F64(3.0), 53);
  const ErrorPropagatingFloat<F64> b(F64(4.0), 53);

  REQUIRE((a + b).precision_bits() == 53);
  REQUIRE((a * b).precision_bits() == 53);
  REQUIRE((a / b).precision_bits() == 53);
  REQUIRE((a + b).value().to_double() == Catch::Approx(7.0));
}

TEST_CASE("ErrorPropagatingFloat detects catastrophic cancellation in subtraction",
          "[numerica_core][error_propagating_float]") {
  // 1.0 and 1.0 + 2^-40 agree in their top ~40 bits; subtracting them
  // should report a large precision loss relative to a 53-bit start.
  const ErrorPropagatingFloat<F64> a(F64(1.0), 53);
  const ErrorPropagatingFloat<F64> b(F64(1.0 + std::ldexp(1.0, -40)), 53);

  const auto diff = b - a;
  REQUIRE(diff.precision_bits() < 20); // most of the 53 bits were cancelled
}

TEST_CASE("Regression: naive (e^x - 1)/x at x=1e-50 loses precision that the "
          "expm1 rewrite recovers",
          "[numerica_core][error_propagating_float]") {
  constexpr int kPrecisionBits = 256;
  const Float x(1e-50, kPrecisionBits);

  // Naive path: compute exp(x), then subtract 1 - this is where the
  // catastrophic cancellation described in the design discussion happens,
  // since exp(x) ~ 1 to within 2^-256 but the true (exp(x)-1) ~ x ~ 2^-166.
  const ErrorPropagatingFloat<Float> exp_x(Float::exp(x), kPrecisionBits);
  const ErrorPropagatingFloat<Float> one(Float(1.0, kPrecisionBits), kPrecisionBits);
  const auto naive_numerator = exp_x - one;
  const auto naive_result = naive_numerator / ErrorPropagatingFloat<Float>(x, kPrecisionBits);

  // Exact rewrite: expm1(x) computes exp(x)-1 directly via MPFR's
  // cancellation-free algorithm, so no precision is lost relative to the
  // (much smaller) result magnitude.
  const ErrorPropagatingFloat<Float> exact_numerator(Float::expm1(x), kPrecisionBits);
  const auto exact_result = exact_numerator / ErrorPropagatingFloat<Float>(x, kPrecisionBits);

  INFO("naive_numerator.precision_bits() = " << naive_numerator.precision_bits());
  INFO("exact_numerator.precision_bits() = " << exact_numerator.precision_bits());

  // The naive subtraction should report severely degraded precision -
  // losing roughly log2(1e50) ~ 166 of the 256 starting bits.
  REQUIRE(naive_numerator.precision_bits() < 100);
  // The expm1 rewrite retains full precision - no subtraction, no cancellation.
  REQUIRE(exact_numerator.precision_bits() == kPrecisionBits);

  // Both should still agree numerically (both approximate (e^x-1)/x -> 1
  // as x -> 0) - the degradation is about *trustworthiness*, not about
  // this particular value being visibly wrong yet.
  REQUIRE(naive_result.value().to_double() == Catch::Approx(1.0).epsilon(1e-6));
  REQUIRE(exact_result.value().to_double() == Catch::Approx(1.0).epsilon(1e-6));
}
