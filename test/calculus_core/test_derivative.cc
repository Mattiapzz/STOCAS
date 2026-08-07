#include <array>
#include <stdexcept>

#include <atom_core/atom.hh>
#include <atom_core/normalize.hh>
#include <calculus_core/derivative.hh>
#include <numerica_core/rational.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using atom_core::Atom;
using atom_core::AtomStore;
using calculus_core::cos_symbol;
using calculus_core::derivative;
using calculus_core::exp_symbol;
using calculus_core::ln_symbol;
using calculus_core::sin_symbol;
using numerica_core::Rational;
using symbol_table::get_symbol;

namespace {
symbol_table::Symbol test_symbol(const char* name) {
  return get_symbol("calculus_core_tests", name);
}

Atom normalized_derivative(AtomStore& store, atom_core::AtomView expr, symbol_table::Symbol var) {
  return atom_core::normalize(store, derivative(store, expr, var).view());
}
} // namespace

TEST_CASE("derivative: Num is always 0", "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_num");
  Atom result = derivative(store, store.num(Rational(7)).view(), x);
  REQUIRE(result == store.num(Rational(0)));
}

TEST_CASE("derivative: Var w.r.t. itself is 1, w.r.t. another is 0",
          "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_var");
  symbol_table::Symbol y = test_symbol("y_var");
  REQUIRE(derivative(store, store.var(x).view(), x) == store.num(Rational(1)));
  REQUIRE(derivative(store, store.var(x).view(), y) == store.num(Rational(0)));
}

TEST_CASE("derivative: Add is termwise", "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_add");
  // d/dx (x + x^2) = 1 + 2*x
  Atom expr =
      store.add(std::array<Atom, 2>{store.var(x), store.pow(store.var(x), store.num(Rational(2)))});
  Atom result = normalized_derivative(store, expr.view(), x);
  Atom expected = store.add(
      std::array<Atom, 2>{store.num(Rational(1)),
                          store.mul(std::array<Atom, 2>{store.num(Rational(2)), store.var(x)})});
  REQUIRE(result == atom_core::normalize(store, expected.view()));
}

TEST_CASE("derivative: Mul uses the product rule", "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_mul");
  symbol_table::Symbol y = test_symbol("y_mul");
  // d/dx (x*y) = y
  Atom expr = store.mul(std::array<Atom, 2>{store.var(x), store.var(y)});
  Atom result = normalized_derivative(store, expr.view(), x);
  REQUIRE(result == store.var(y));
}

TEST_CASE("derivative: Mul product rule with three factors", "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_mul3");
  // d/dx (x*x*x) = 3*x^2
  Atom expr = store.mul(std::array<Atom, 3>{store.var(x), store.var(x), store.var(x)});
  Atom result = normalized_derivative(store, expr.view(), x);
  Atom expected = store.mul(
      std::array<Atom, 2>{store.num(Rational(3)), store.pow(store.var(x), store.num(Rational(2)))});
  REQUIRE(result == atom_core::normalize(store, expected.view()));
}

TEST_CASE("derivative: Pow ordinary power rule (constant exponent)",
          "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_pow");
  // d/dx x^3 = 3*x^2
  Atom expr = store.pow(store.var(x), store.num(Rational(3)));
  Atom result = normalized_derivative(store, expr.view(), x);
  Atom expected = store.mul(
      std::array<Atom, 2>{store.num(Rational(3)), store.pow(store.var(x), store.num(Rational(2)))});
  REQUIRE(result == atom_core::normalize(store, expected.view()));
}

TEST_CASE("derivative: Pow with exponent 1 collapses to the base's derivative",
          "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_pow1");
  // d/dx x^1 = 1
  Atom expr = store.pow(store.var(x), store.num(Rational(1)));
  Atom result = normalized_derivative(store, expr.view(), x);
  REQUIRE(result == store.num(Rational(1)));
}

TEST_CASE("derivative: Pow with a constant base and variable exponent (exponential rule)",
          "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_exprule");
  // d/dx 2^x = 2^x * ln(2)
  Atom expr = store.pow(store.num(Rational(2)), store.var(x));
  Atom result = normalized_derivative(store, expr.view(), x);
  Atom expected = store.mul(
      std::array<Atom, 2>{store.pow(store.num(Rational(2)), store.var(x)),
                          store.fun(ln_symbol(), std::array<Atom, 1>{store.num(Rational(2))})});
  REQUIRE(result == atom_core::normalize(store, expected.view()));
}

TEST_CASE("derivative: Pow with both base and exponent depending on the variable",
          "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_general_pow");
  // d/dx x^x = x^x * (ln(x) + 1)
  Atom expr = store.pow(store.var(x), store.var(x));
  Atom result = normalized_derivative(store, expr.view(), x);
  Atom expected = store.mul(std::array<Atom, 2>{
      store.pow(store.var(x), store.var(x)),
      store.add(std::array<Atom, 2>{store.fun(ln_symbol(), std::array<Atom, 1>{store.var(x)}),
                                    store.num(Rational(1))})});
  REQUIRE(result == atom_core::normalize(store, expected.view()));
}

TEST_CASE("derivative: Pow where neither base nor exponent depend on the variable is 0",
          "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_pow_const");
  Atom expr = store.pow(store.num(Rational(2)), store.num(Rational(5)));
  REQUIRE(derivative(store, expr.view(), x) == store.num(Rational(0)));
}

TEST_CASE("derivative: pre-registered sin/cos/exp/ln hooks", "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_builtin");
  Atom x_atom = store.var(x);

  // The hook's raw output is wrapped in an unsimplified `Mul(1, ...)`/
  // `Add(...)` shell (derivative()'s chain rule always multiplies by the
  // argument's own derivative, here `dx/dx = 1`) - normalize() strips
  // that down to compare against the bare expected expression.
  SECTION("sin(x)' = cos(x)") {
    Atom expr = store.fun(sin_symbol(), std::array<Atom, 1>{x_atom});
    Atom expected = store.fun(cos_symbol(), std::array<Atom, 1>{x_atom});
    REQUIRE(normalized_derivative(store, expr.view(), x) ==
            atom_core::normalize(store, expected.view()));
  }

  SECTION("cos(x)' = -sin(x)") {
    Atom expr = store.fun(cos_symbol(), std::array<Atom, 1>{x_atom});
    Atom expected = store.mul(std::array<Atom, 2>{
        store.num(Rational(-1)), store.fun(sin_symbol(), std::array<Atom, 1>{x_atom})});
    REQUIRE(normalized_derivative(store, expr.view(), x) ==
            atom_core::normalize(store, expected.view()));
  }

  SECTION("exp(x)' = exp(x)") {
    Atom expr = store.fun(exp_symbol(), std::array<Atom, 1>{x_atom});
    REQUIRE(normalized_derivative(store, expr.view(), x) ==
            atom_core::normalize(store, expr.view()));
  }

  SECTION("ln(x)' = x^-1") {
    Atom expr = store.fun(ln_symbol(), std::array<Atom, 1>{x_atom});
    Atom expected = store.pow(x_atom, store.num(Rational(-1)));
    REQUIRE(normalized_derivative(store, expr.view(), x) ==
            atom_core::normalize(store, expected.view()));
  }
}

TEST_CASE("derivative: chain rule composes a built-in hook with a non-trivial argument",
          "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_chain");
  // d/dx sin(x^2) = cos(x^2) * 2*x
  Atom inner = store.pow(store.var(x), store.num(Rational(2)));
  Atom expr = store.fun(sin_symbol(), std::array<Atom, 1>{inner});
  Atom result = normalized_derivative(store, expr.view(), x);
  Atom expected = store.mul(std::array<Atom, 3>{
      store.fun(cos_symbol(), std::array<Atom, 1>{inner}), store.num(Rational(2)), store.var(x)});
  REQUIRE(result == atom_core::normalize(store, expected.view()));
}

TEST_CASE("derivative: an unregistered function head throws", "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_unknown");
  symbol_table::Symbol mystery = test_symbol("mystery_fn");
  Atom expr = store.fun(mystery, std::array<Atom, 1>{store.var(x)});
  REQUIRE_THROWS_AS(derivative(store, expr.view(), x), std::invalid_argument);
}

TEST_CASE("derivative: a custom-registered function hook is differentiated through",
          "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_custom");
  symbol_table::Symbol square_plus_one = test_symbol("square_plus_one_fn");

  // Register d/du square_plus_one(u) = 2*u.
  calculus_core::register_derivative_hook(
      square_plus_one, [](AtomStore& hook_store, std::span<const atom_core::AtomView> args) {
        return std::vector<Atom>{hook_store.mul(
            std::array<Atom, 2>{hook_store.num(Rational(2)), hook_store.as_atom(args[0])})};
      });

  // d/dx square_plus_one(x^3) = 2*x^3 * 3*x^2 (chain rule)
  Atom inner = store.pow(store.var(x), store.num(Rational(3)));
  Atom expr = store.fun(square_plus_one, std::array<Atom, 1>{inner});
  Atom result = normalized_derivative(store, expr.view(), x);
  Atom expected = store.mul(std::array<Atom, 4>{store.num(Rational(2)),
                                                inner,
                                                store.num(Rational(3)),
                                                store.pow(store.var(x), store.num(Rational(2)))});
  REQUIRE(result == atom_core::normalize(store, expected.view()));
}

TEST_CASE("derivative: a hook returning the wrong number of partials throws",
          "[calculus_core][derivative]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_mismatch");
  symbol_table::Symbol bad_hook_fn = test_symbol("bad_hook_fn");

  calculus_core::register_derivative_hook(
      bad_hook_fn, [](AtomStore&, std::span<const atom_core::AtomView>) {
        return std::vector<Atom>{}; // wrong: this function takes one argument
      });

  Atom expr = store.fun(bad_hook_fn, std::array<Atom, 1>{store.var(x)});
  REQUIRE_THROWS_AS(derivative(store, expr.view(), x), std::logic_error);
}
