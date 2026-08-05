#include <array>
#include <stdexcept>

#include <atom_core/atom.hh>
#include <numerica_core/rational.hh>
#include <pattern_core/matcher.hh>
#include <pattern_core/rewrite.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using atom_core::Atom;
using atom_core::AtomStore;
using numerica_core::Rational;
using pattern_core::match;
using pattern_core::MatchBindings;
using pattern_core::replace_all;
using pattern_core::replace_all_multiple;
using pattern_core::Rule;
using pattern_core::substitute;
using symbol_table::get_symbol;

namespace {
symbol_table::Symbol test_symbol(const char* name) {
  return get_symbol("pattern_core_tests_rewrite", name);
}
} // namespace

TEST_CASE("substitute: replaces a Single wildcard with its bound atom", "[pattern_core][rewrite]") {
  AtomStore store;
  symbol_table::Symbol wc = test_symbol("w_sub_");
  Atom wildcard = store.var(wc);
  Atom two = store.num(Rational(2));
  std::array<Atom, 2> replacement_factors{wildcard, two};
  Atom replacement = store.mul(replacement_factors);

  Atom five = store.num(Rational(5));
  auto bindings = match(wildcard.view(), five.view());
  REQUIRE(bindings.has_value());

  Atom result = substitute(store, replacement.view(), *bindings);
  std::array<Atom, 2> expected_factors{five, two};
  Atom expected = store.mul(expected_factors);
  REQUIRE(result.view() == expected.view());
}

TEST_CASE("substitute: splices a sequence wildcard into Add", "[pattern_core][rewrite]") {
  AtomStore store;
  symbol_table::Symbol a = test_symbol("a_sub_seq");
  symbol_table::Symbol seq = test_symbol("s_sub_seq__");

  Atom va = store.var(a);
  Atom seq_atom = store.var(seq);
  std::array<Atom, 2> pattern_terms{va, seq_atom};
  Atom pattern = store.add(pattern_terms);

  symbol_table::Symbol b = test_symbol("b_sub_seq");
  symbol_table::Symbol c = test_symbol("c_sub_seq");
  Atom vb = store.var(b);
  Atom vc = store.var(c);
  std::array<Atom, 3> target_terms{va, vb, vc};
  Atom target = store.add(target_terms);

  auto bindings = match(pattern.view(), target.view());
  REQUIRE(bindings.has_value());

  // Replacement re-uses the same sequence wildcard, wrapped with an extra
  // fixed term.
  symbol_table::Symbol d = test_symbol("d_sub_seq");
  Atom vd = store.var(d);
  std::array<Atom, 2> replacement_terms{vd, seq_atom};
  Atom replacement = store.add(replacement_terms);

  Atom result = substitute(store, replacement.view(), *bindings);
  std::array<Atom, 3> expected_terms{vd, vb, vc};
  Atom expected = store.add(expected_terms);
  REQUIRE(result.view() == expected.view());
}

TEST_CASE("substitute: throws for an unbound wildcard or a misused sequence wildcard",
          "[pattern_core][rewrite]") {
  AtomStore store;
  symbol_table::Symbol wc = test_symbol("w_sub_unbound_");
  Atom wildcard = store.var(wc);

  REQUIRE_THROWS_AS(substitute(store, wildcard.view(), MatchBindings{}), std::invalid_argument);

  symbol_table::Symbol seq = test_symbol("s_sub_misuse__");
  Atom seq_atom = store.var(seq);
  Atom two = store.num(Rational(2));
  Atom pow_replacement = store.pow(seq_atom, two);

  symbol_table::Symbol a = test_symbol("a_sub_misuse");
  Atom va = store.var(a);
  std::array<Atom, 2> pattern_terms{va, seq_atom};
  Atom pattern = store.add(pattern_terms);
  symbol_table::Symbol b = test_symbol("b_sub_misuse");
  Atom vb = store.var(b);
  std::array<Atom, 2> target_terms{va, vb};
  Atom target = store.add(target_terms);
  auto bindings = match(pattern.view(), target.view());
  REQUIRE(bindings.has_value());

  REQUIRE_THROWS_AS(substitute(store, pow_replacement.view(), *bindings), std::invalid_argument);
}

TEST_CASE("replace_all: rewrites every matching subterm bottom-up in one pass",
          "[pattern_core][rewrite]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_rewrite_pow");
  symbol_table::Symbol y = test_symbol("y_rewrite_pow");
  symbol_table::Symbol wc = test_symbol("w_rewrite_pow_");

  // Rule: x_^2 -> x_ * x_
  Atom wildcard = store.var(wc);
  Atom two = store.num(Rational(2));
  Atom pattern = store.pow(wildcard, two);
  std::array<Atom, 2> replacement_factors{wildcard, wildcard};
  Atom replacement = store.mul(replacement_factors);
  Rule square_rule{pattern.view(), replacement.view(), nullptr};

  Atom vx = store.var(x);
  Atom vy = store.var(y);
  Atom x_squared = store.pow(vx, two);
  Atom y_squared = store.pow(vy, two);
  std::array<Atom, 2> sum_terms{x_squared, y_squared};
  Atom target = store.add(sum_terms);

  Atom result = replace_all(store, square_rule, target.view());

  std::array<Atom, 2> x_times_x{vx, vx};
  std::array<Atom, 2> y_times_y{vy, vy};
  std::array<Atom, 2> expected_terms{store.mul(x_times_x), store.mul(y_times_y)};
  Atom expected = store.add(expected_terms);
  REQUIRE(result.view() == expected.view());

  // A non-matching subterm (exponent != 2) is left untouched.
  Atom three = store.num(Rational(3));
  Atom x_cubed = store.pow(vx, three);
  Atom unchanged = replace_all(store, square_rule, x_cubed.view());
  REQUIRE(unchanged.view() == x_cubed.view());
}

TEST_CASE("replace_all: a guard predicate restricts which matches are accepted",
          "[pattern_core][rewrite]") {
  AtomStore store;
  symbol_table::Symbol f = test_symbol("f_guard");
  symbol_table::Symbol g = test_symbol("g_guard");
  symbol_table::Symbol wc = test_symbol("w_guard_");

  Atom wildcard = store.var(wc);
  std::array<Atom, 1> pattern_args{wildcard};
  Atom pattern = store.fun(f, pattern_args);
  std::array<Atom, 1> replacement_args{wildcard};
  Atom replacement = store.fun(g, replacement_args);

  Rule positive_only_rule{
      pattern.view(), replacement.view(), [wc](const MatchBindings& bindings) {
        atom_core::AtomView bound = std::get<atom_core::AtomView>(bindings.at(wc));
        return bound.tag() == atom_core::AtomTag::Num && bound.as_num() > Rational(0);
      }};

  Atom five = store.num(Rational(5));
  std::array<Atom, 1> positive_args{five};
  Atom f_positive = store.fun(f, positive_args);
  Atom result_positive = replace_all(store, positive_only_rule, f_positive.view());
  Atom expected_positive = store.fun(g, positive_args);
  REQUIRE(result_positive.view() == expected_positive.view());

  Atom minus_three = store.num(Rational(-3));
  std::array<Atom, 1> negative_args{minus_three};
  Atom f_negative = store.fun(f, negative_args);
  Atom result_negative = replace_all(store, positive_only_rule, f_negative.view());
  REQUIRE(result_negative.view() == f_negative.view());
}

TEST_CASE("replace_all_multiple: converges to a fixed point", "[pattern_core][rewrite]") {
  AtomStore store;
  symbol_table::Symbol f = test_symbol("f_multi");
  symbol_table::Symbol a = test_symbol("a_multi");
  symbol_table::Symbol wc = test_symbol("w_multi_");

  // Rule: f(f(x_)) -> f(x_)  (collapses doubled f).
  Atom wildcard = store.var(wc);
  std::array<Atom, 1> inner_pattern_args{wildcard};
  Atom inner_pattern = store.fun(f, inner_pattern_args);
  std::array<Atom, 1> outer_pattern_args{inner_pattern};
  Atom pattern = store.fun(f, outer_pattern_args);
  std::array<Atom, 1> replacement_args{wildcard};
  Atom replacement = store.fun(f, replacement_args);
  Rule collapse_rule{pattern.view(), replacement.view(), nullptr};

  Atom va = store.var(a);
  std::array<Atom, 1> level1_args{va};
  Atom level1 = store.fun(f, level1_args);
  std::array<Atom, 1> level2_args{level1};
  Atom level2 = store.fun(f, level2_args);
  std::array<Atom, 1> level3_args{level2};
  Atom level3 = store.fun(f, level3_args);
  std::array<Atom, 1> level4_args{level3};
  Atom level4 = store.fun(f, level4_args); // f(f(f(f(a))))

  Atom result = replace_all_multiple(store, collapse_rule, level4.view());
  REQUIRE(result.view() == level1.view()); // fully collapsed to f(a)
}

TEST_CASE("replace_all_multiple: a non-converging rule hits the iteration cap and throws",
          "[pattern_core][rewrite]") {
  AtomStore store;
  symbol_table::Symbol f = test_symbol("f_grow");
  symbol_table::Symbol g = test_symbol("g_grow");
  symbol_table::Symbol wc = test_symbol("w_grow_");

  // Rule: f(x_) -> f(g(x_))  - matches its own output forever, never
  // converges. The deliberately tiny max_iterations keeps this test fast.
  Atom wildcard = store.var(wc);
  std::array<Atom, 1> pattern_args{wildcard};
  Atom pattern = store.fun(f, pattern_args);
  std::array<Atom, 1> g_args{wildcard};
  Atom g_wildcard = store.fun(g, g_args);
  std::array<Atom, 1> replacement_args{g_wildcard};
  Atom replacement = store.fun(f, replacement_args);
  Rule growing_rule{pattern.view(), replacement.view(), nullptr};

  symbol_table::Symbol a = test_symbol("a_grow");
  Atom va = store.var(a);
  std::array<Atom, 1> start_args{va};
  Atom start = store.fun(f, start_args);

  REQUIRE_THROWS_AS(replace_all_multiple(store, growing_rule, start.view(), /*max_iterations=*/5),
                    std::runtime_error);
}
