#include <cmath>
#include <stdexcept>

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

TEST_CASE("HyperDual division: 1/x derivatives at x0=2", "[numerica_core][hyper_dual]") {
  // f(x) = 1/x. f = 0.5, f' = -1/x^2 = -0.25, f'' = 2/x^3 = 0.25.
  const HyperDual<F64, 2> x = HyperDual<F64, 2>::variable(F64(2.0));
  const HyperDual<F64, 2> one = HyperDual<F64, 2>::constant(F64(1.0));
  const HyperDual<F64, 2> f = one / x;

  REQUIRE(f.derivative(0).to_double() == Catch::Approx(0.5));
  REQUIRE(f.derivative(1).to_double() == Catch::Approx(-0.25));
  REQUIRE(f.derivative(2).to_double() == Catch::Approx(0.25));
}

TEST_CASE("HyperDual division by a zero-constant-term divisor throws",
          "[numerica_core][hyper_dual]") {
  const HyperDual<F64, 2> x = HyperDual<F64, 2>::variable(F64(0.0));
  const HyperDual<F64, 2> one = HyperDual<F64, 2>::constant(F64(1.0));
  REQUIRE_THROWS_AS(one / x, std::domain_error);
}

TEST_CASE("HyperDual log derivatives at x0=2", "[numerica_core][hyper_dual]") {
  // f(x) = log(x). f = ln(2), f' = 1/x = 0.5, f'' = -1/x^2 = -0.25.
  const HyperDual<F64, 2> x = HyperDual<F64, 2>::variable(F64(2.0));
  const HyperDual<F64, 2> f = HyperDual<F64, 2>::log(x);

  REQUIRE(f.derivative(0).to_double() == Catch::Approx(std::log(2.0)));
  REQUIRE(f.derivative(1).to_double() == Catch::Approx(0.5));
  REQUIRE(f.derivative(2).to_double() == Catch::Approx(-0.25));
}

TEST_CASE("HyperDual log of a non-positive constant term throws", "[numerica_core][hyper_dual]") {
  const HyperDual<F64, 2> x = HyperDual<F64, 2>::variable(F64(-1.0));
  REQUIRE_THROWS_AS((HyperDual<F64, 2>::log(x)), std::domain_error);
}

TEST_CASE("HyperDual sin/cos derivatives at x0=0", "[numerica_core][hyper_dual]") {
  constexpr std::size_t kOrder = 3;
  const HyperDual<F64, kOrder> x = HyperDual<F64, kOrder>::variable(F64(0.0));
  const HyperDual<F64, kOrder> s = HyperDual<F64, kOrder>::sin(x);
  const HyperDual<F64, kOrder> c = HyperDual<F64, kOrder>::cos(x);

  // sin: 0, 1, 0, -1. cos: 1, 0, -1, 0.
  REQUIRE(s.derivative(0).to_double() == Catch::Approx(0.0).margin(1e-12));
  REQUIRE(s.derivative(1).to_double() == Catch::Approx(1.0));
  REQUIRE(s.derivative(2).to_double() == Catch::Approx(0.0).margin(1e-12));
  REQUIRE(s.derivative(3).to_double() == Catch::Approx(-1.0));

  REQUIRE(c.derivative(0).to_double() == Catch::Approx(1.0));
  REQUIRE(c.derivative(1).to_double() == Catch::Approx(0.0).margin(1e-12));
  REQUIRE(c.derivative(2).to_double() == Catch::Approx(-1.0));
  REQUIRE(c.derivative(3).to_double() == Catch::Approx(0.0).margin(1e-12));
}

TEST_CASE("HyperDual sin composed with a non-trivial argument", "[numerica_core][hyper_dual]") {
  // f(x) = sin(x^2), around x0=1: f = sin(1), f' = 2x*cos(x^2) = 2*cos(1).
  constexpr std::size_t kOrder = 1;
  const HyperDual<F64, kOrder> x = HyperDual<F64, kOrder>::variable(F64(1.0));
  const HyperDual<F64, kOrder> f = HyperDual<F64, kOrder>::sin(x * x);

  REQUIRE(f.derivative(0).to_double() == Catch::Approx(std::sin(1.0)));
  REQUIRE(f.derivative(1).to_double() == Catch::Approx(2.0 * std::cos(1.0)));
}
