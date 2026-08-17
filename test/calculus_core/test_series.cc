#include <array>
#include <cmath>
#include <stdexcept>

#include <atom_core/atom.hh>
#include <calculus_core/derivative.hh>
#include <calculus_core/series.hh>
#include <numerica_core/float_types.hh>
#include <numerica_core/rational.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using atom_core::Atom;
using atom_core::AtomStore;
using calculus_core::cos_symbol;
using calculus_core::exp_symbol;
using calculus_core::ln_symbol;
using calculus_core::Series;
using calculus_core::sin_symbol;
using calculus_core::taylor_series;
using numerica_core::F64;
using numerica_core::Rational;
using symbol_table::get_symbol;

namespace {
symbol_table::Symbol test_symbol(const char* name) {
  return get_symbol("calculus_core_tests", name);
}
} // namespace

TEST_CASE("taylor_series: exp(x) around 0 matches 1/k!", "[calculus_core][series]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_exp_series");
  Atom expr = store.fun(exp_symbol(), std::array<Atom, 1>{store.var(x)});

  const Series<F64, 5> series = taylor_series<F64, 5>(expr.view(), x, F64(0.0));
  double factorial = 1.0;
  for (std::size_t k = 0; k <= 5; ++k) {
    if (k > 0) {
      factorial *= static_cast<double>(k);
    }
    REQUIRE(series.coefficient(k).to_double() == Catch::Approx(1.0 / factorial));
  }
}

TEST_CASE("taylor_series: sin(x) around 0", "[calculus_core][series]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_sin_series");
  Atom expr = store.fun(sin_symbol(), std::array<Atom, 1>{store.var(x)});

  const Series<F64, 5> series = taylor_series<F64, 5>(expr.view(), x, F64(0.0));
  REQUIRE(series.coefficient(0).to_double() == Catch::Approx(0.0).margin(1e-12));
  REQUIRE(series.coefficient(1).to_double() == Catch::Approx(1.0));
  REQUIRE(series.coefficient(2).to_double() == Catch::Approx(0.0).margin(1e-12));
  REQUIRE(series.coefficient(3).to_double() == Catch::Approx(-1.0 / 6.0));
  REQUIRE(series.coefficient(4).to_double() == Catch::Approx(0.0).margin(1e-12));
  REQUIRE(series.coefficient(5).to_double() == Catch::Approx(1.0 / 120.0));
}

TEST_CASE("taylor_series: cos(x) around 0", "[calculus_core][series]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_cos_series");
  Atom expr = store.fun(cos_symbol(), std::array<Atom, 1>{store.var(x)});

  const Series<F64, 4> series = taylor_series<F64, 4>(expr.view(), x, F64(0.0));
  REQUIRE(series.coefficient(0).to_double() == Catch::Approx(1.0));
  REQUIRE(series.coefficient(1).to_double() == Catch::Approx(0.0).margin(1e-12));
  REQUIRE(series.coefficient(2).to_double() == Catch::Approx(-0.5));
  REQUIRE(series.coefficient(3).to_double() == Catch::Approx(0.0).margin(1e-12));
  REQUIRE(series.coefficient(4).to_double() == Catch::Approx(1.0 / 24.0));
}

TEST_CASE("taylor_series: ln(x) around 1 matches the alternating harmonic series",
          "[calculus_core][series]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_ln_series");
  Atom expr = store.fun(ln_symbol(), std::array<Atom, 1>{store.var(x)});

  // ln(x) = (x-1) - (x-1)^2/2 + (x-1)^3/3 - (x-1)^4/4 + ...
  const Series<F64, 4> series = taylor_series<F64, 4>(expr.view(), x, F64(1.0));
  REQUIRE(series.coefficient(0).to_double() == Catch::Approx(0.0).margin(1e-12));
  REQUIRE(series.coefficient(1).to_double() == Catch::Approx(1.0));
  REQUIRE(series.coefficient(2).to_double() == Catch::Approx(-0.5));
  REQUIRE(series.coefficient(3).to_double() == Catch::Approx(1.0 / 3.0));
  REQUIRE(series.coefficient(4).to_double() == Catch::Approx(-0.25));
}

TEST_CASE("taylor_series: a polynomial's series is exact (matches the hand-computed Taylor form)",
          "[calculus_core][series]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_poly_series");
  // f(x) = x^2 + 3*x + 5. f(2) = 15, f'(2) = 7, f''(2)/2! = 1.
  Atom expr = store.add(
      std::array<Atom, 3>{store.pow(store.var(x), store.num(Rational(2))),
                          store.mul(std::array<Atom, 2>{store.num(Rational(3)), store.var(x)}),
                          store.num(Rational(5))});

  const Series<F64, 2> series = taylor_series<F64, 2>(expr.view(), x, F64(2.0));
  REQUIRE(series.coefficient(0).to_double() == Catch::Approx(15.0));
  REQUIRE(series.coefficient(1).to_double() == Catch::Approx(7.0));
  REQUIRE(series.coefficient(2).to_double() == Catch::Approx(1.0));
}

TEST_CASE("taylor_series: negative integer Pow exponent (1/x around a nonzero point)",
          "[calculus_core][series]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_neg_pow_series");
  Atom expr = store.pow(store.var(x), store.num(Rational(-1)));

  const Series<F64, 2> series = taylor_series<F64, 2>(expr.view(), x, F64(2.0));
  REQUIRE(series.coefficient(0).to_double() == Catch::Approx(0.5));
  REQUIRE(series.coefficient(1).to_double() == Catch::Approx(-0.25));
  REQUIRE(series.coefficient(2).to_double() == Catch::Approx(0.125));
}

TEST_CASE("taylor_series: a genuine pole throws std::domain_error", "[calculus_core][series]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_pole_series");
  Atom expr = store.pow(store.var(x), store.num(Rational(-1)));
  REQUIRE_THROWS_AS((taylor_series<F64, 2>(expr.view(), x, F64(0.0))), std::domain_error);
}

TEST_CASE("taylor_series: composition through Pow's chain (sin(x^2) around 0)",
          "[calculus_core][series]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_composition_series");
  Atom inner = store.pow(store.var(x), store.num(Rational(2)));
  Atom expr = store.fun(sin_symbol(), std::array<Atom, 1>{inner});

  const Series<F64, 2> series = taylor_series<F64, 2>(expr.view(), x, F64(0.0));
  REQUIRE(series.coefficient(0).to_double() == Catch::Approx(0.0).margin(1e-12));
  REQUIRE(series.coefficient(1).to_double() == Catch::Approx(0.0).margin(1e-12));
  REQUIRE(series.coefficient(2).to_double() == Catch::Approx(1.0));
}

TEST_CASE("taylor_series: evaluate() cross-checks against exp(1) within the truncation error",
          "[calculus_core][series]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_evaluate_series");
  Atom expr = store.fun(exp_symbol(), std::array<Atom, 1>{store.var(x)});

  const Series<F64, 8> series = taylor_series<F64, 8>(expr.view(), x, F64(0.0));
  REQUIRE(series.evaluate(F64(1.0)).to_double() == Catch::Approx(std::exp(1.0)).epsilon(1e-4));
}

TEST_CASE("taylor_series: a free variable other than the expansion variable throws",
          "[calculus_core][series]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_free_var_series");
  symbol_table::Symbol y = test_symbol("y_free_var_series");
  Atom expr = store.add(std::array<Atom, 2>{store.var(x), store.var(y)});
  REQUIRE_THROWS_AS((taylor_series<F64, 2>(expr.view(), x, F64(0.0))), std::invalid_argument);
}

TEST_CASE("taylor_series: an unsupported function throws", "[calculus_core][series]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_unsupported_fn_series");
  symbol_table::Symbol mystery = test_symbol("mystery_series_fn");
  Atom expr = store.fun(mystery, std::array<Atom, 1>{store.var(x)});
  REQUIRE_THROWS_AS((taylor_series<F64, 2>(expr.view(), x, F64(0.0))), std::invalid_argument);
}

TEST_CASE("taylor_series: order() reflects the requested truncation order",
          "[calculus_core][series]") {
  REQUIRE(Series<F64, 3>::order() == 3);
}
