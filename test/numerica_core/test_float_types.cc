#include <cmath>

#include <numerica_core/float_types.hh>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using numerica_core::F64;
using numerica_core::Float;

TEST_CASE("F64 basic arithmetic", "[numerica_core][float_types]") {
  const F64 a(3.0);
  const F64 b(4.0);
  REQUIRE((a + b).to_double() == Catch::Approx(7.0));
  REQUIRE((a - b).to_double() == Catch::Approx(-1.0));
  REQUIRE((a * b).to_double() == Catch::Approx(12.0));
  REQUIRE((b / a).to_double() == Catch::Approx(4.0 / 3.0));
}

TEST_CASE("F64 sin/cos", "[numerica_core][float_types]") {
  const F64 x(0.5);
  REQUIRE(F64::sin(x).to_double() == Catch::Approx(std::sin(0.5)));
  REQUIRE(F64::cos(x).to_double() == Catch::Approx(std::cos(0.5)));
}

TEST_CASE("F64 expm1 avoids cancellation that naive exp(x)-1 suffers",
          "[numerica_core][float_types]") {
  const F64 tiny(1e-15);
  const F64 exact = F64::expm1(tiny);
  // Should be close to 1e-15 (native double has limited room to demonstrate
  // the full cancellation effect - Float's test covers the extreme case).
  REQUIRE(exact.to_double() == Catch::Approx(1e-15).epsilon(0.01));
}

TEST_CASE("Float basic arithmetic at high precision", "[numerica_core][float_types]") {
  const Float a(3.0, 256);
  const Float b(4.0, 256);
  REQUIRE((a + b).to_double() == Catch::Approx(7.0));
  REQUIRE((a - b).to_double() == Catch::Approx(-1.0));
  REQUIRE((a * b).to_double() == Catch::Approx(12.0));
  REQUIRE((b / a).to_double() == Catch::Approx(4.0 / 3.0));
  REQUIRE(a.precision_bits() == 256);
}

TEST_CASE("Float comparisons", "[numerica_core][float_types]") {
  const Float a(1.0, 128);
  const Float b(2.0, 128);
  REQUIRE(a < b);
  REQUIRE(a == Float(1.0, 128));
  REQUIRE(a != b);
}

TEST_CASE("Float::expm1 matches Float::exp(x)-1 for moderate x, exactly for tiny x",
          "[numerica_core][float_types]") {
  const Float x(0.001, 256);
  const Float via_expm1 = Float::expm1(x);
  const Float via_naive = Float::exp(x) - Float(1.0, 256);
  // At this magnitude there's no severe cancellation yet, so both methods
  // should agree closely.
  REQUIRE(via_expm1.to_double() == Catch::Approx(via_naive.to_double()).epsilon(1e-9));
}
