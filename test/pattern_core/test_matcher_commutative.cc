#include <algorithm>
#include <array>
#include <variant>
#include <vector>

#include <atom_core/atom.hh>
#include <numerica_core/rational.hh>
#include <pattern_core/matcher.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

/// @file
/// @brief Tests for M5-T2's additions to pattern_core::match(): commutative
/// (multiset) matching for Add/Mul and Symmetric functions, and sequence
/// wildcards (`x__`/`x___`). test_matcher.cc covers the M5-T1 baseline
/// (literal/Single-wildcard/positional matching); this file only exercises
/// what changed.
using atom_core::Atom;
using atom_core::AtomStore;
using numerica_core::Rational;
using pattern_core::match;
using pattern_core::MatchBindings;
using symbol_table::get_symbol;
using symbol_table::SymbolAttribute;

namespace {
symbol_table::Symbol test_symbol(const char* name) {
  return get_symbol("pattern_core_tests_commutative", name);
}

symbol_table::Symbol symmetric_symbol(const char* name) {
  std::array<SymbolAttribute, 1> attrs{SymbolAttribute::Symmetric};
  return get_symbol("pattern_core_tests_commutative", name, attrs);
}

atom_core::AtomView bound_atom(const MatchBindings& bindings, symbol_table::Symbol wc) {
  return std::get<atom_core::AtomView>(bindings.at(wc));
}

const std::vector<atom_core::AtomView>& bound_sequence(const MatchBindings& bindings,
                                                       symbol_table::Symbol wc) {
  return std::get<std::vector<atom_core::AtomView>>(bindings.at(wc));
}

bool contains_view(const std::vector<atom_core::AtomView>& views, atom_core::AtomView target) {
  return std::find(views.begin(), views.end(), target) != views.end();
}
} // namespace

TEST_CASE("match: Add is fully commutative - order in the pattern doesn't matter",
          "[pattern_core][matcher][commutative]") {
  AtomStore store;
  symbol_table::Symbol a = test_symbol("a_comm");
  symbol_table::Symbol b = test_symbol("b_comm");
  symbol_table::Symbol wc = test_symbol("w_comm_");

  Atom va = store.var(a);
  Atom vb = store.var(b);
  Atom wildcard = store.var(wc);
  std::array<Atom, 2> pattern_terms{wildcard, va};
  Atom pattern = store.add(pattern_terms);

  // Target built from two Vars (both Var-tag, so canonical order depends
  // on symbol id, unlike the Num-sibling trick M5-T1's matcher relied on):
  // the old positional-only matcher could fail here depending on id order;
  // the new commutative matcher must not.
  std::array<Atom, 2> target_terms{va, vb};
  Atom target = store.add(target_terms);

  auto result = match(pattern.view(), target.view());
  REQUIRE(result.has_value());
  REQUIRE(bound_atom(*result, wc) == vb.view());
}

TEST_CASE("match: Mul multiset-matches two wildcards against two targets, either assignment "
          "is accepted",
          "[pattern_core][matcher][commutative]") {
  AtomStore store;
  symbol_table::Symbol wc1 = test_symbol("w1_mul_");
  symbol_table::Symbol wc2 = test_symbol("w2_mul_");
  Atom w1 = store.var(wc1);
  Atom w2 = store.var(wc2);
  std::array<Atom, 2> pattern_factors{w1, w2};
  Atom pattern = store.mul(pattern_factors);

  Atom five = store.num(Rational(5));
  Atom seven = store.num(Rational(7));
  std::array<Atom, 2> target_factors{five, seven};
  Atom target = store.mul(target_factors);

  auto result = match(pattern.view(), target.view());
  REQUIRE(result.has_value());
  // Whichever assignment the backtracking search found, both wildcards
  // must be bound to (distinct) members of {5, 7}.
  atom_core::AtomView bound1 = bound_atom(*result, wc1);
  atom_core::AtomView bound2 = bound_atom(*result, wc2);
  REQUIRE(bound1 != bound2);
  REQUIRE((bound1 == five.view() || bound1 == seven.view()));
  REQUIRE((bound2 == five.view() || bound2 == seven.view()));
}

TEST_CASE("match: sequence wildcard (x__) captures the remaining Add terms",
          "[pattern_core][matcher][commutative]") {
  AtomStore store;
  symbol_table::Symbol a = test_symbol("a_seq");
  symbol_table::Symbol seq = test_symbol("s_seq__");

  Atom va = store.var(a);
  Atom seq_atom = store.var(seq);
  std::array<Atom, 2> pattern_terms{va, seq_atom};
  Atom pattern = store.add(pattern_terms);

  symbol_table::Symbol b = test_symbol("b_seq");
  symbol_table::Symbol c = test_symbol("c_seq");
  Atom vb = store.var(b);
  Atom vc = store.var(c);
  std::array<Atom, 3> target_terms{va, vb, vc};
  Atom target = store.add(target_terms);

  auto result = match(pattern.view(), target.view());
  REQUIRE(result.has_value());
  const std::vector<atom_core::AtomView>& captured = bound_sequence(*result, seq);
  REQUIRE(captured.size() == 2);
  REQUIRE(contains_view(captured, vb.view()));
  REQUIRE(contains_view(captured, vc.view()));
}

TEST_CASE("match: AtLeastOne (x__) sequence wildcard rejects an empty capture, AnyCount "
          "(x___) accepts it",
          "[pattern_core][matcher][commutative]") {
  AtomStore store;
  symbol_table::Symbol a = test_symbol("a_empty");
  symbol_table::Symbol b = test_symbol("b_empty");
  symbol_table::Symbol seq_at_least_one = test_symbol("s_empty__");
  symbol_table::Symbol seq_any_count = test_symbol("s_empty___");

  Atom va = store.var(a);
  Atom vb = store.var(b);
  // Target whose two terms are already fully consumed by the pattern's
  // non-sequence terms, forcing an empty leftover for the sequence
  // wildcard to capture.
  std::array<Atom, 2> target_terms{va, vb};
  Atom target = store.add(target_terms);

  Atom seq_at_least_one_atom = store.var(seq_at_least_one);
  std::array<Atom, 3> pattern_at_least_one_terms{va, vb, seq_at_least_one_atom};
  Atom pattern_at_least_one = store.add(pattern_at_least_one_terms);
  REQUIRE_FALSE(match(pattern_at_least_one.view(), target.view()).has_value());

  Atom seq_any_count_atom = store.var(seq_any_count);
  std::array<Atom, 3> pattern_any_count_terms{va, vb, seq_any_count_atom};
  Atom pattern_any_count = store.add(pattern_any_count_terms);
  auto result_any_count = match(pattern_any_count.view(), target.view());
  REQUIRE(result_any_count.has_value());
  REQUIRE(bound_sequence(*result_any_count, seq_any_count).empty());
}

TEST_CASE("match: Symmetric Fun matches arguments commutatively, non-Symmetric doesn't",
          "[pattern_core][matcher][commutative]") {
  AtomStore store;
  symbol_table::Symbol sym_f = symmetric_symbol("sym_f");
  symbol_table::Symbol plain_f = test_symbol("plain_f");
  symbol_table::Symbol wc = test_symbol("w_symfun_");
  symbol_table::Symbol y = test_symbol("y_symfun");

  Atom wildcard = store.var(wc);
  Atom vy = store.var(y);
  std::array<Atom, 2> pattern_args{wildcard, vy};

  Atom sym_pattern = store.fun(sym_f, pattern_args);
  Atom plain_pattern = store.fun(plain_f, pattern_args);

  Atom five = store.num(Rational(5));
  // Target has the arguments in the OPPOSITE order from the pattern.
  std::array<Atom, 2> swapped_args{vy, five};
  Atom sym_target = store.fun(sym_f, swapped_args);
  Atom plain_target = store.fun(plain_f, swapped_args);

  auto sym_result = match(sym_pattern.view(), sym_target.view());
  REQUIRE(sym_result.has_value());
  REQUIRE(bound_atom(*sym_result, wc) == five.view());

  REQUIRE_FALSE(match(plain_pattern.view(), plain_target.view()).has_value());
}

TEST_CASE("match: sequence wildcard in a non-Symmetric Fun captures a contiguous, "
          "order-preserved run",
          "[pattern_core][matcher][commutative]") {
  AtomStore store;
  symbol_table::Symbol f = test_symbol("f_seqfun");
  symbol_table::Symbol a = test_symbol("a_seqfun");
  symbol_table::Symbol b = test_symbol("b_seqfun");
  symbol_table::Symbol seq = test_symbol("s_seqfun__");

  Atom va = store.var(a);
  Atom vb = store.var(b);
  Atom seq_atom = store.var(seq);
  std::array<Atom, 3> pattern_args{va, seq_atom, vb};
  Atom pattern = store.fun(f, pattern_args);

  Atom one = store.num(Rational(1));
  Atom two = store.num(Rational(2));
  Atom three = store.num(Rational(3));
  std::array<Atom, 5> target_args{va, one, two, three, vb};
  Atom target = store.fun(f, target_args);

  auto result = match(pattern.view(), target.view());
  REQUIRE(result.has_value());
  const std::vector<atom_core::AtomView>& captured = bound_sequence(*result, seq);
  REQUIRE(captured.size() == 3);
  REQUIRE(captured[0] == one.view());
  REQUIRE(captured[1] == two.view());
  REQUIRE(captured[2] == three.view());
}

TEST_CASE("match: misuse of sequence wildcards throws std::invalid_argument",
          "[pattern_core][matcher][commutative]") {
  AtomStore store;
  symbol_table::Symbol seq1 = test_symbol("s1_misuse__");
  symbol_table::Symbol seq2 = test_symbol("s2_misuse__");
  symbol_table::Symbol a = test_symbol("a_misuse");

  Atom seq1_atom = store.var(seq1);
  Atom seq2_atom = store.var(seq2);
  Atom va = store.var(a);

  // Two sequence wildcards in the same Add.
  std::array<Atom, 2> two_sequences{seq1_atom, seq2_atom};
  Atom pattern_two_sequences = store.add(two_sequences);
  Atom target = store.add(std::array<Atom, 2>{va, va});
  REQUIRE_THROWS_AS(match(pattern_two_sequences.view(), target.view()), std::invalid_argument);

  // A sequence wildcard used bare (top-level, not a direct Add/Mul/Fun
  // child).
  REQUIRE_THROWS_AS(match(seq1_atom.view(), va.view()), std::invalid_argument);

  // A sequence wildcard used as Pow's base.
  Atom two = store.num(Rational(2));
  Atom pow_pattern = store.pow(seq1_atom, two);
  Atom pow_target = store.pow(va, two);
  REQUIRE_THROWS_AS(match(pow_pattern.view(), pow_target.view()), std::invalid_argument);
}
