#include <cmath>

#include <numerica_core/dual.hh>
#include <numerica_core/float_types.hh>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using numerica_core::Dual;
using numerica_core::F64;

TEST_CASE("Dual derivative of a polynomial matches the analytic derivative",
          "[numerica_core][dual]") {
  // f(x) = x^2 + 3x, f'(x) = 2x + 3. At x = 2: f = 10, f' = 7.
  const Dual<F64> x = Dual<F64>::variable(F64(2.0));
  const Dual<F64> three = Dual<F64>::constant(F64(3.0));
  const Dual<F64> f = x * x + three * x;

  REQUIRE(f.value().to_double() == Catch::Approx(10.0));
  REQUIRE(f.derivative().to_double() == Catch::Approx(7.0));
}

TEST_CASE("Dual derivative of exp matches exp itself", "[numerica_core][dual]") {
  const Dual<F64> x = Dual<F64>::variable(F64(1.0));
  const Dual<F64> y = Dual<F64>::exp(x);

  REQUIRE(y.value().to_double() == Catch::Approx(std::exp(1.0)));
  REQUIRE(y.derivative().to_double() == Catch::Approx(std::exp(1.0)));
}

TEST_CASE("Dual quotient rule", "[numerica_core][dual]") {
  // f(x) = x / (x + 1), f'(x) = 1 / (x+1)^2. At x = 3: f = 0.75, f' = 0.0625.
  const Dual<F64> x = Dual<F64>::variable(F64(3.0));
  const Dual<F64> one = Dual<F64>::constant(F64(1.0));
  const Dual<F64> f = x / (x + one);

  REQUIRE(f.value().to_double() == Catch::Approx(0.75));
  REQUIRE(f.derivative().to_double() == Catch::Approx(0.0625));
}
