#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include <atom_core/atom.hh>
#include <numerica_core/rational.hh>
#include <pattern_core/matcher.hh>
#include <pattern_core/parallel_rewrite.hh>
#include <pattern_core/rewrite.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

/// @file
/// @brief M5-T4 (parallel rewriting, M5's last subtask and TSan exit
/// criterion): exercises parallel_replace_all() with several worker
/// threads, asserting identical output to a single-threaded reference
/// replace_all() run per term. Runs as part of pattern_core_tests, which
/// the existing blocking `linux-tsan` CI job (ci.yml) already builds and
/// runs the full suite of under ThreadSanitizer on every PR - satisfying
/// the roadmap's "parallel rewrite test suite green under TSan on every
/// CI run, not just opt-in" exit criterion without any new CI wiring.
using atom_core::Atom;
using atom_core::AtomStore;
using atom_core::AtomView;
using numerica_core::Rational;
using pattern_core::parallel_replace_all;
using pattern_core::replace_all;
using pattern_core::Rule;
using symbol_table::get_symbol;

namespace {
symbol_table::Symbol test_symbol(const char* name) {
  return get_symbol("pattern_core_tests_parallel_rewrite", name);
}
} // namespace

TEST_CASE("parallel_replace_all: matches single-threaded replace_all on every term",
          "[pattern_core][parallel_rewrite]") {
  // Rule: x_^2 -> x_ * x_, same as test_rewrite.cc's square rule.
  AtomStore rule_store;
  symbol_table::Symbol wc = test_symbol("w_par_");
  Atom wildcard = rule_store.var(wc);
  Atom two = rule_store.num(Rational(2));
  Atom pattern = rule_store.pow(wildcard, two);
  std::array<Atom, 2> replacement_factors{wildcard, wildcard};
  Atom replacement = rule_store.mul(replacement_factors);
  Rule square_rule{pattern.view(), replacement.view(), nullptr};

  // Build a batch of independent terms, some matching the rule, some not,
  // deliberately more terms than worker threads so at least one thread
  // handles multiple terms.
  AtomStore input_store;
  std::vector<Atom> owned_terms;
  std::vector<AtomView> terms;
  for (int i = 0; i < 37; ++i) {
    symbol_table::Symbol xi =
        get_symbol("pattern_core_tests_parallel_rewrite", "x" + std::to_string(i));
    Atom vxi = input_store.var(xi);
    Atom exponent = input_store.num(Rational(i % 3 == 0 ? 3 : 2)); // mix matches/non-matches
    Atom term = input_store.pow(vxi, exponent);
    owned_terms.push_back(term);
    terms.push_back(term.view());
  }

  std::vector<pattern_core::ParallelRewriteResult> parallel_results =
      parallel_replace_all(square_rule, terms, /*num_threads=*/8);
  REQUIRE(parallel_results.size() == terms.size());

  for (std::size_t i = 0; i < terms.size(); ++i) {
    Atom sequential = replace_all(input_store, square_rule, terms[i]);
    REQUIRE(parallel_results[i].result->view() == sequential.view());
  }
}

TEST_CASE("parallel_replace_all: num_threads is clamped, empty input is handled",
          "[pattern_core][parallel_rewrite]") {
  AtomStore rule_store;
  symbol_table::Symbol wc = test_symbol("w_par_clamp_");
  Atom wildcard = rule_store.var(wc);
  Rule identity_rule{wildcard.view(), wildcard.view(), nullptr};

  std::vector<AtomView> empty_terms;
  std::vector<pattern_core::ParallelRewriteResult> empty_results =
      parallel_replace_all(identity_rule, empty_terms, 4);
  REQUIRE(empty_results.empty());

  AtomStore input_store;
  Atom single = input_store.num(Rational(42));
  std::array<AtomView, 1> one_term{single.view()};

  // num_threads = 0 must be clamped to at least 1, not crash/hang.
  std::vector<pattern_core::ParallelRewriteResult> zero_thread_results =
      parallel_replace_all(identity_rule, one_term, 0);
  REQUIRE(zero_thread_results.size() == 1);
  REQUIRE(zero_thread_results[0].result->view() == single.view());

  // num_threads > terms.size() must not spawn idle/unused threads that
  // touch nothing (just clamp down, no crash).
  std::vector<pattern_core::ParallelRewriteResult> many_thread_results =
      parallel_replace_all(identity_rule, one_term, 64);
  REQUIRE(many_thread_results.size() == 1);
  REQUIRE(many_thread_results[0].result->view() == single.view());
}
