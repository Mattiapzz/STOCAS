#include <cmath>

#include <numerica_core/float_types.hh>
#include <numerica_core/hyper_dual.hh>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using numerica_core::F64;
using numerica_core::HyperDual;

TEST_CASE("HyperDual second derivative of a cubic matches the analytic derivative",
          "[numerica_core][hyper_dual]") {
  // f(x) = x^3. f = 8, f' = 3x^2 = 12, f'' = 6x = 12 at x = 2.
  const HyperDual<F64, 2> x = HyperDual<F64, 2>::variable(F64(2.0));
  const HyperDual<F64, 2> f = x * x * x;

  REQUIRE(f.derivative(0).to_double() == Catch::Approx(8.0));
  REQUIRE(f.derivative(1).to_double() == Catch::Approx(12.0));
  REQUIRE(f.derivative(2).to_double() == Catch::Approx(12.0));
}

TEST_CASE("HyperDual all derivatives of exp equal exp(x0)", "[numerica_core][hyper_dual]") {
  constexpr std::size_t kOrder = 4;
  const HyperDual<F64, kOrder> x = HyperDual<F64, kOrder>::variable(F64(1.5));
  const HyperDual<F64, kOrder> y = HyperDual<F64, kOrder>::exp(x);

  const double expected = std::exp(1.5);
  for (std::size_t k = 0; k <= kOrder; ++k) {
    REQUIRE(y.derivative(k).to_double() == Catch::Approx(expected));
  }
}

TEST_CASE("HyperDual addition and constants", "[numerica_core][hyper_dual]") {
  // f(x) = x^2 + 5. f' = 2x, f'' = 2.
  const HyperDual<F64, 2> x = HyperDual<F64, 2>::variable(F64(3.0));
  const HyperDual<F64, 2> five = HyperDual<F64, 2>::constant(F64(5.0));
  const HyperDual<F64, 2> f = x * x + five;

  REQUIRE(f.derivative(0).to_double() == Catch::Approx(14.0));
  REQUIRE(f.derivative(1).to_double() == Catch::Approx(6.0));
  REQUIRE(f.derivative(2).to_double() == Catch::Approx(2.0));
}
