#include <random>
#include <stdexcept>
#include <vector>

#include <algebra_core/rings.hh>
#include <numerica_core/rational.hh>
#include <poly_core/monomial.hh>
#include <poly_core/multivariate_gcd.hh>
#include <poly_core/polynomial.hh>
#include <symbol_table/symbol.hh>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

/// @file
/// @brief Large sparse polynomial benchmarks (M4-GATE testing gap, filled
/// 2026-08-04): a recorded-baseline, non-blocking perf gate for
/// `MultivariatePolynomial<R>` multiplication and `rational_multivariate_gcd`
/// on polynomials with hundreds to thousands of terms - the regime
/// poly_core is meant for per the roadmap. Baseline tracking, not a CI
/// correctness gate - see the hidden `[!benchmark]` tag (same convention as
/// numerica_core's BigInt benchmarks, M1) and the `poly-core-benchmark
/// (non-blocking)` CI job that runs this explicitly and logs timings.
using algebra_core::RationalField;
using numerica_core::Rational;
using poly_core::Exponent;
using poly_core::Monomial;
using poly_core::MultivariatePolynomial;
using poly_core::rational_multivariate_gcd;

namespace {
constexpr const char* kNs = "poly_core_large_expression_benchmark";
using Poly = MultivariatePolynomial<RationalField>;

Poly random_dense_polynomial(const RationalField& field,
                             const std::vector<symbol_table::Symbol>& vars,
                             std::mt19937_64& rng,
                             int num_terms,
                             int max_degree,
                             int max_coeff) {
  Poly result(field, vars);
  std::uniform_int_distribution<int> degree_dist(0, max_degree);
  std::uniform_int_distribution<int> coeff_dist(1, max_coeff);
  for (int t = 0; t < num_terms; ++t) {
    std::vector<Exponent> exponents;
    exponents.reserve(vars.size());
    for (std::size_t v = 0; v < vars.size(); ++v) {
      exponents.push_back(static_cast<Exponent>(degree_dist(rng)));
    }
    result = result + Poly::from_term(
                          field, vars, Monomial(std::move(exponents)), Rational(coeff_dist(rng)));
  }
  return result;
}
} // namespace

TEST_CASE("large sparse polynomial multiplication and gcd benchmarks",
          "[poly_core][large_expression][!benchmark]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y");
  symbol_table::Symbol z = symbol_table::get_symbol(kNs, "z");
  std::vector<symbol_table::Symbol> vars{x, y, z};
  std::mt19937_64 rng(2026);

  Poly p = random_dense_polynomial(field, vars, rng, 60, 8, 20);
  Poly q = random_dense_polynomial(field, vars, rng, 60, 8, 20);
  INFO("p has " << p.num_terms() << " terms, q has " << q.num_terms() << " terms");

  BENCHMARK("multiply two ~60-term 3-variable polynomials") {
    return p * q;
  };

  // Search (once, outside the timed benchmark) for a seed whose gcd input
  // succeeds without hitting multivariate_gcd.hh's documented residual
  // limitation (see test_gcd_properties.cc) - the benchmark measures
  // steady-state performance, not that known, separately-tracked gap.
  Poly a(field, vars);
  Poly b(field, vars);
  bool found = false;
  for (int seed_attempt = 0; seed_attempt < 20 && !found; ++seed_attempt) {
    Poly shared = random_dense_polynomial(field, vars, rng, 20, 4, 10);
    Poly candidate_a = shared * random_dense_polynomial(field, vars, rng, 20, 4, 10);
    Poly candidate_b = shared * random_dense_polynomial(field, vars, rng, 20, 4, 10);
    try {
      [[maybe_unused]] Poly probe = rational_multivariate_gcd(field, candidate_a, candidate_b);
      a = candidate_a;
      b = candidate_b;
      found = true;
    } catch (const std::runtime_error&) {
      continue;
    }
  }
  REQUIRE(found);
  INFO("a has " << a.num_terms() << " terms, b has " << b.num_terms() << " terms");

  BENCHMARK("rational_multivariate_gcd on polynomials sharing a ~20-term common factor") {
    return rational_multivariate_gcd(field, a, b);
  };
}
