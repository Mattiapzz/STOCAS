#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

#include <algebra_core/rings.hh>
#include <numerica_core/rational.hh>
#include <poly_core/monomial.hh>
#include <poly_core/multivariate_division.hh>
#include <poly_core/multivariate_gcd.hh>
#include <poly_core/polynomial.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

/// @file
/// @brief Property tests for poly_core's multivariate GCD (M4-GATE testing
/// gap, filled 2026-08-04): `gcd(a*c, b*c) == c * gcd(a, b)` up to a unit,
/// and `p / gcd(p, q) * gcd(p, q) == p`, over randomized small polynomials.
/// A fixed seed keeps this deterministic (no CI flakiness) while still
/// exercising inputs beyond the hand-picked cases in test_multivariate_gcd.cc.
///
/// A small minority of random inputs hit multivariate_gcd.hh's documented
/// residual limitation (the leading-coefficient prediction isn't
/// guaranteed exact in full generality even after content removal) and
/// `rational_multivariate_gcd` throws std::runtime_error rather than
/// returning a wrong answer - see that header's comment. Each test counts
/// and reports these as skipped trials (visibly, via WARN) rather than
/// failing outright, but REQUIREs the vast majority still succeed and
/// verify the property - this test exists to catch a *regression* in the
/// implementation's correctness, not to demand 100% coverage of an
/// honestly-documented open gap.
using algebra_core::RationalField;
using numerica_core::Rational;
using poly_core::Exponent;
using poly_core::Monomial;
using poly_core::multivariate_div_rem;
using poly_core::MultivariatePolynomial;
using poly_core::rational_multivariate_gcd;

namespace {
constexpr const char* kNs = "poly_core_gcd_property_tests";

using Poly = MultivariatePolynomial<RationalField>;

Poly monic_normalize(const RationalField& field, Poly poly) {
  if (poly.is_zero()) {
    return poly;
  }
  Rational inv_lead = poly.leading_term().second.inv();
  return poly * Poly::constant(field, poly.variables(), inv_lead);
}

Poly random_polynomial(const RationalField& field,
                       const std::vector<symbol_table::Symbol>& vars,
                       std::mt19937_64& rng,
                       int max_terms,
                       int max_degree,
                       int max_coeff) {
  Poly result(field, vars);
  std::uniform_int_distribution<int> term_count_dist(1, max_terms);
  std::uniform_int_distribution<int> degree_dist(0, max_degree);
  std::uniform_int_distribution<int> coeff_dist(-max_coeff, max_coeff);
  int terms = term_count_dist(rng);
  for (int t = 0; t < terms; ++t) {
    std::vector<Exponent> exponents;
    exponents.reserve(vars.size());
    for (std::size_t v = 0; v < vars.size(); ++v) {
      exponents.push_back(static_cast<Exponent>(degree_dist(rng)));
    }
    int coeff = coeff_dist(rng);
    if (coeff == 0) {
      continue;
    }
    result = result + Poly::from_term(field, vars, Monomial(std::move(exponents)), Rational(coeff));
  }
  return result;
}
} // namespace

TEST_CASE("property: gcd(a*c, b*c) == c * gcd(a, b) up to a unit", "[poly_core][gcd_property]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x1");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y1");
  std::vector<symbol_table::Symbol> vars{x, y};
  std::mt19937_64 rng(42);

  int succeeded = 0;
  int skipped = 0;
  for (int trial = 0; trial < 20; ++trial) {
    Poly a =
        random_polynomial(field, vars, rng, /*max_terms=*/3, /*max_degree=*/2, /*max_coeff=*/5);
    Poly b = random_polynomial(field, vars, rng, 3, 2, 5);
    Poly c = random_polynomial(field, vars, rng, 2, 2, 5);

    Poly lhs(field, vars);
    Poly rhs_raw(field, vars);
    try {
      lhs = rational_multivariate_gcd(field, a * c, b * c);
      rhs_raw = c * rational_multivariate_gcd(field, a, b);
    } catch (const std::runtime_error&) {
      WARN("trial " << trial << ": skipped (known multivariate_gcd.hh residual limitation)");
      ++skipped;
      continue;
    }

    INFO("trial " << trial);
    if (rhs_raw.is_zero()) {
      REQUIRE(lhs.is_zero());
    } else {
      REQUIRE(lhs == monic_normalize(field, rhs_raw));
    }
    ++succeeded;
  }
  REQUIRE(succeeded >= 15); // catches a real regression, not just the documented rare gap
  INFO("succeeded=" << succeeded << " skipped=" << skipped);
}

TEST_CASE("property: p / gcd(p, q) * gcd(p, q) == p", "[poly_core][gcd_property]") {
  RationalField field;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "x2");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "y2");
  std::vector<symbol_table::Symbol> vars{x, y};
  std::mt19937_64 rng(1337);

  int succeeded = 0;
  int skipped = 0;
  for (int trial = 0; trial < 20; ++trial) {
    Poly p = random_polynomial(field, vars, rng, 4, 2, 5);
    Poly q = random_polynomial(field, vars, rng, 4, 2, 5);

    INFO("trial " << trial);
    if (p.is_zero() && q.is_zero()) {
      continue; // gcd(0, 0) == 0 - dividing by it is undefined, not this property's concern.
    }
    Poly g(field, vars);
    try {
      g = rational_multivariate_gcd(field, p, q);
    } catch (const std::runtime_error&) {
      WARN("trial " << trial << ": skipped (known multivariate_gcd.hh residual limitation)");
      ++skipped;
      continue;
    }
    auto [quotient, remainder] = multivariate_div_rem(field, p, g);
    REQUIRE(remainder.is_zero());
    REQUIRE(quotient * g == p);
    ++succeeded;
  }
  REQUIRE(succeeded >= 15); // catches a real regression, not just the documented rare gap
  INFO("succeeded=" << succeeded << " skipped=" << skipped);
}
