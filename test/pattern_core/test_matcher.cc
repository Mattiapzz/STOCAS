#include <array>

#include <atom_core/atom.hh>
#include <numerica_core/rational.hh>
#include <pattern_core/matcher.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using atom_core::Atom;
using atom_core::AtomStore;
using numerica_core::Rational;
using pattern_core::match;
using symbol_table::get_symbol;

namespace {
symbol_table::Symbol test_symbol(const char* name) {
  return get_symbol("pattern_core_tests", name);
}

atom_core::AtomView bound_atom(const pattern_core::MatchBindings& bindings,
                               symbol_table::Symbol wc) {
  return std::get<atom_core::AtomView>(bindings.at(wc));
}
} // namespace

TEST_CASE("match: literal Num/Var leaves", "[pattern_core][matcher]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_literal");
  symbol_table::Symbol y = test_symbol("y_literal");

  REQUIRE(match(store.num(Rational(3)).view(), store.num(Rational(3)).view()).has_value());
  REQUIRE_FALSE(match(store.num(Rational(3)).view(), store.num(Rational(4)).view()).has_value());
  REQUIRE(match(store.var(x).view(), store.var(x).view()).has_value());
  REQUIRE_FALSE(match(store.var(x).view(), store.var(y).view()).has_value());
  // Different tags never match.
  REQUIRE_FALSE(match(store.num(Rational(3)).view(), store.var(x).view()).has_value());
}

TEST_CASE("match: a bare wildcard matches anything and binds it", "[pattern_core][matcher]") {
  AtomStore store;
  symbol_table::Symbol wc = test_symbol("w_");
  symbol_table::Symbol y = test_symbol("y_bare");
  Atom pattern = store.var(wc);

  Atom target_num = store.num(Rational(42));
  auto result_num = match(pattern.view(), target_num.view());
  REQUIRE(result_num.has_value());
  REQUIRE(result_num->size() == 1);
  REQUIRE(bound_atom(*result_num, wc) == target_num.view());

  Atom target_var = store.var(y);
  auto result_var = match(pattern.view(), target_var.view());
  REQUIRE(result_var.has_value());
  REQUIRE(bound_atom(*result_var, wc) == target_var.view());
}

TEST_CASE("match: repeated wildcard requires a consistent binding", "[pattern_core][matcher]") {
  AtomStore store;
  symbol_table::Symbol f = test_symbol("f_repeat");
  symbol_table::Symbol wc = test_symbol("w_repeat_");

  Atom wildcard = store.var(wc);
  std::array<Atom, 2> pattern_args{wildcard, wildcard};
  Atom pattern = store.fun(f, pattern_args);

  Atom five = store.num(Rational(5));
  Atom six = store.num(Rational(6));

  std::array<Atom, 2> same_args{five, five};
  Atom target_same = store.fun(f, same_args);
  auto result_same = match(pattern.view(), target_same.view());
  REQUIRE(result_same.has_value());
  REQUIRE(bound_atom(*result_same, wc) == five.view());

  std::array<Atom, 2> different_args{five, six};
  Atom target_different = store.fun(f, different_args);
  REQUIRE_FALSE(match(pattern.view(), target_different.view()).has_value());
}

TEST_CASE("match: Fun requires the same head and argument count, args matched positionally",
          "[pattern_core][matcher]") {
  AtomStore store;
  symbol_table::Symbol f = test_symbol("f_head");
  symbol_table::Symbol g = test_symbol("g_head");
  symbol_table::Symbol wc = test_symbol("w_fun_");
  symbol_table::Symbol y = test_symbol("y_fun");

  Atom wildcard = store.var(wc);
  Atom vy = store.var(y);
  std::array<Atom, 2> pattern_args{wildcard, vy};
  Atom pattern = store.fun(f, pattern_args);

  Atom seven = store.num(Rational(7));
  std::array<Atom, 2> matching_args{seven, vy};
  Atom target_match = store.fun(f, matching_args);
  auto result = match(pattern.view(), target_match.view());
  REQUIRE(result.has_value());
  REQUIRE(bound_atom(*result, wc) == seven.view());

  // Different head.
  std::array<Atom, 2> other_head_args{seven, vy};
  Atom target_other_head = store.fun(g, other_head_args);
  REQUIRE_FALSE(match(pattern.view(), target_other_head.view()).has_value());

  // Second (non-wildcard) argument doesn't match.
  symbol_table::Symbol z = test_symbol("z_fun");
  Atom vz = store.var(z);
  std::array<Atom, 2> mismatched_args{seven, vz};
  Atom target_mismatched = store.fun(f, mismatched_args);
  REQUIRE_FALSE(match(pattern.view(), target_mismatched.view()).has_value());

  // Different argument count.
  std::array<Atom, 1> short_args{seven};
  Atom target_short = store.fun(f, short_args);
  REQUIRE_FALSE(match(pattern.view(), target_short.view()).has_value());
}

TEST_CASE("match: Pow preserves base/exponent order (non-commutative, no ambiguity)",
          "[pattern_core][matcher]") {
  AtomStore store;
  symbol_table::Symbol wc = test_symbol("w_pow_");
  Atom wildcard = store.var(wc);
  Atom two = store.num(Rational(2));
  Atom pattern = store.pow(wildcard, two);

  symbol_table::Symbol y = test_symbol("y_pow");
  Atom vy = store.var(y);
  Atom target = store.pow(vy, two);
  auto result = match(pattern.view(), target.view());
  REQUIRE(result.has_value());
  REQUIRE(bound_atom(*result, wc) == vy.view());

  // Exponent doesn't match.
  Atom three = store.num(Rational(3));
  Atom target_wrong_exponent = store.pow(vy, three);
  REQUIRE_FALSE(match(pattern.view(), target_wrong_exponent.view()).has_value());
}

TEST_CASE("match: Add is commutative - a wildcard binds regardless of canonical child order",
          "[pattern_core][matcher]") {
  AtomStore store;
  symbol_table::Symbol wc = test_symbol("w_add_");
  symbol_table::Symbol y = test_symbol("y_add");

  Atom wildcard = store.var(wc);
  Atom three = store.num(Rational(3));
  std::array<Atom, 2> pattern_terms{wildcard, three};
  Atom pattern = store.add(pattern_terms);

  Atom vy = store.var(y);
  std::array<Atom, 2> target_terms{vy, three};
  Atom target = store.add(target_terms);

  auto result = match(pattern.view(), target.view());
  REQUIRE(result.has_value());
  REQUIRE(bound_atom(*result, wc) == vy.view());
}

TEST_CASE("match: bindings accumulate across a threaded match, mismatches leave earlier "
          "bindings untouched by returning nullopt",
          "[pattern_core][matcher]") {
  AtomStore store;
  symbol_table::Symbol wc = test_symbol("w_thread_");
  symbol_table::Symbol y = test_symbol("y_thread");

  Atom wildcard = store.var(wc);
  Atom target_value = store.num(Rational(9));

  auto first = match(wildcard.view(), target_value.view());
  REQUIRE(first.has_value());

  // Re-matching the same wildcard against the same value, with the
  // established bindings threaded in, succeeds and keeps the binding.
  auto second = match(wildcard.view(), target_value.view(), *first);
  REQUIRE(second.has_value());
  REQUIRE(bound_atom(*second, wc) == target_value.view());

  // Re-matching against a different value, with those bindings threaded
  // in, fails - the wildcard is already bound.
  Atom vy = store.var(y);
  REQUIRE_FALSE(match(wildcard.view(), vy.view(), *first).has_value());
}
