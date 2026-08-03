#pragma once

#include <algorithm>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include <algebra_core/rings.hh>
#include <numerica_core/big_int.hh>
#include <numerica_core/finite_field.hh>
#include <poly_core/monomial.hh>
#include <poly_core/polynomial.hh>
#include <poly_core/univariate_gcd.hh>

/// @file
/// @brief Univariate polynomial factorization over a finite field (M4-T3's
/// first bullet): Yun's squarefree decomposition, distinct-degree
/// factorization, and Cantor-Zassenhaus equal-degree (randomized)
/// splitting, combined into a single factor_univariate_finite_field() entry
/// point returning monic irreducible factors with multiplicities.
///
/// Specialized to algebra_core::FiniteFieldRing<std::uint64_t> (not generic
/// over algebra_core::Field, unlike univariate_gcd.hh/multivariate_gcd.hh)
/// because distinct-degree factorization needs the field's prime modulus
/// explicitly - `x^(p^d) mod f`, computed via BigInt-exponent modular
/// exponentiation since `p^d` overflows uint64_t for realistic p, d - and
/// Cantor-Zassenhaus's splitting step assumes odd characteristic (checked
/// explicitly; p == 2 throws rather than silently misbehaving).
///
/// Deferred (documented here, same spirit as M4-T1/T2's deferrals): Hensel
/// lifting these finite-field factors back to ℤ/ℚ, and multivariate
/// factorization - both are later M4-T3 work per the roadmap.
namespace poly_core {

using FiniteFieldRing64 = algebra_core::FiniteFieldRing<std::uint64_t>;
using FiniteFieldElement64 = numerica_core::FiniteField<std::uint64_t>;
using FiniteFieldPoly = MultivariatePolynomial<FiniteFieldRing64>;

/// @brief One irreducible factor and its multiplicity in the original
/// polynomial.
struct FactorTerm {
  FiniteFieldPoly factor; // monic, irreducible
  unsigned multiplicity;
};

/// @brief A full factorization: `f == leading_coefficient *
/// product(term.factor ^ term.multiplicity for term in factors)`.
struct Factorization {
  FiniteFieldElement64 leading_coefficient;
  std::vector<FactorTerm> factors;
};

namespace detail {

[[nodiscard]] inline FiniteFieldPoly univariate_derivative(const FiniteFieldRing64& field,
                                                           const FiniteFieldPoly& f) {
  FiniteFieldPoly result(field, f.variables());
  for (const auto& term : f.terms()) {
    Exponent exponent = term.first.exponent(0);
    if (exponent == 0) {
      continue;
    }
    FiniteFieldElement64 scaled = field.zero();
    for (Exponent k = 0; k < exponent; ++k) {
      scaled = field.add(scaled, term.second);
    }
    if (scaled == field.zero()) {
      continue;
    }
    result =
        result + FiniteFieldPoly::from_term(
                     field, f.variables(), Monomial(std::vector<Exponent>{exponent - 1}), scaled);
  }
  return result;
}

[[nodiscard]] inline FiniteFieldPoly monic_normalize(const FiniteFieldRing64& field,
                                                     FiniteFieldPoly f) {
  if (f.is_zero()) {
    return f;
  }
  FiniteFieldElement64 inv_lead = field.inv(f.leading_term().second);
  FiniteFieldPoly scale = FiniteFieldPoly::constant(field, f.variables(), inv_lead);
  return f * scale;
}

[[nodiscard]] inline numerica_core::BigInt bigint_pow(numerica_core::BigInt base,
                                                      std::uint64_t exponent) {
  numerica_core::BigInt result(1);
  while (exponent > 0) {
    if (exponent & 1U) {
      result = result * base;
    }
    base = base * base;
    exponent >>= 1U;
  }
  return result;
}

// Computes base^exponent mod modulus for a BigInt exponent (needed since
// p^d overflows uint64_t for realistic p, d).
[[nodiscard]] inline FiniteFieldPoly poly_powmod(const FiniteFieldRing64& field,
                                                 FiniteFieldPoly base,
                                                 numerica_core::BigInt exponent,
                                                 const FiniteFieldPoly& modulus) {
  FiniteFieldPoly result = FiniteFieldPoly::constant(field, modulus.variables(), field.one());
  FiniteFieldPoly reduced_base = univariate_div_rem(field, std::move(base), modulus).second;
  const numerica_core::BigInt zero(0);
  const numerica_core::BigInt two(2);
  while (exponent > zero) {
    if (!((exponent % two) == zero)) {
      result = univariate_div_rem(field, result * reduced_base, modulus).second;
    }
    reduced_base = univariate_div_rem(field, reduced_base * reduced_base, modulus).second;
    exponent = exponent / two;
  }
  return result;
}

/// @brief Yun's squarefree decomposition: `f_monic == product(a_i^i)`, each
/// `a_i` squarefree and pairwise coprime (a product of the distinct
/// irreducible factors of @p f_monic with multiplicity exactly `i`).
/// Assumes no multiplicity is a multiple of the field's characteristic
/// (true in practice for any realistic test/caller-chosen prime).
[[nodiscard]] inline std::vector<std::pair<FiniteFieldPoly, unsigned>>
squarefree_decomposition(const FiniteFieldRing64& field, const FiniteFieldPoly& f_monic) {
  std::vector<std::pair<FiniteFieldPoly, unsigned>> result;
  FiniteFieldPoly f_prime = univariate_derivative(field, f_monic);
  FiniteFieldPoly a0 = univariate_gcd(field, f_monic, f_prime);
  FiniteFieldPoly b = univariate_div_rem(field, f_monic, a0).first;
  FiniteFieldPoly c = univariate_div_rem(field, f_prime, a0).first;
  FiniteFieldPoly d = c - univariate_derivative(field, b);

  unsigned i = 1;
  while (b.degree_in(0) > 0) {
    FiniteFieldPoly a_i = univariate_gcd(field, b, d);
    if (a_i.degree_in(0) > 0) {
      result.emplace_back(a_i, i);
    }
    b = univariate_div_rem(field, b, a_i).first;
    c = univariate_div_rem(field, d, a_i).first;
    d = c - univariate_derivative(field, b);
    ++i;
  }
  return result;
}

/// @brief Groups the irreducible factors of a monic squarefree @p f by
/// degree: `f == product(group for (degree, group) in result)`, `group`
/// being the product of all degree-`d` irreducible factors of @p f.
[[nodiscard]] inline std::vector<std::pair<unsigned, FiniteFieldPoly>>
distinct_degree_split(const FiniteFieldRing64& field, std::uint64_t prime, FiniteFieldPoly f) {
  std::vector<std::pair<unsigned, FiniteFieldPoly>> result;
  FiniteFieldPoly x_poly = FiniteFieldPoly::from_term(
      field, f.variables(), Monomial(std::vector<Exponent>{1}), field.one());
  numerica_core::BigInt prime_big(static_cast<std::int64_t>(prime));
  FiniteFieldPoly remaining = std::move(f);

  unsigned d = 1;
  while (remaining.degree_in(0) >= 2U * d) {
    numerica_core::BigInt exponent = bigint_pow(prime_big, d);
    FiniteFieldPoly frobenius = poly_powmod(field, x_poly, exponent, remaining);
    FiniteFieldPoly diff =
        frobenius - x_poly; // x_poly is already reduced (degree 1 < degree(remaining))
    FiniteFieldPoly g = monic_normalize(field, univariate_gcd(field, remaining, diff));
    if (g.degree_in(0) > 0) {
      result.emplace_back(d, g);
      remaining = univariate_div_rem(field, remaining, g).first;
    }
    ++d;
  }
  if (remaining.degree_in(0) > 0) {
    result.emplace_back(static_cast<unsigned>(remaining.degree_in(0)), remaining);
  }
  return result;
}

inline constexpr std::size_t kMaxSplitAttempts = 200;

/// @brief Cantor-Zassenhaus randomized splitting: splits @p f (a product of
/// degree-@p d irreducible factors) into its individual monic irreducible
/// factors.
/// @throws std::invalid_argument if @p prime == 2 (even characteristic
///         needs a different splitting step, not implemented here).
/// @throws std::runtime_error if no splitting polynomial is found within
///         kMaxSplitAttempts random tries for some subgroup (astronomically
///         unlikely for any of this file's fixed/test primes).
[[nodiscard]] inline std::vector<FiniteFieldPoly> equal_degree_split(const FiniteFieldRing64& field,
                                                                     std::uint64_t prime,
                                                                     FiniteFieldPoly f,
                                                                     unsigned d,
                                                                     std::mt19937_64& rng) {
  if (prime == 2) {
    throw std::invalid_argument("poly_core::equal_degree_split: requires an odd prime");
  }
  numerica_core::BigInt prime_big(static_cast<std::int64_t>(prime));
  numerica_core::BigInt exponent =
      (bigint_pow(prime_big, d) - numerica_core::BigInt(1)) / numerica_core::BigInt(2);
  std::uniform_int_distribution<std::uint64_t> dist(0, prime - 1);

  std::vector<FiniteFieldPoly> pending;
  pending.push_back(std::move(f));
  std::vector<FiniteFieldPoly> result;

  while (!pending.empty()) {
    FiniteFieldPoly poly = std::move(pending.back());
    pending.pop_back();
    auto degree = static_cast<unsigned>(poly.degree_in(0));
    if (degree == d) {
      result.push_back(std::move(poly));
      continue;
    }

    bool split = false;
    for (std::size_t attempt = 0; !split && attempt < kMaxSplitAttempts; ++attempt) {
      FiniteFieldPoly r(field, poly.variables());
      for (Exponent e = 0; e < degree; ++e) {
        std::uint64_t coeff_value = dist(rng);
        if (coeff_value == 0) {
          continue;
        }
        r = r + FiniteFieldPoly::from_term(field,
                                           poly.variables(),
                                           Monomial(std::vector<Exponent>{e}),
                                           FiniteFieldElement64(prime, coeff_value));
      }
      if (r.is_zero()) {
        continue;
      }

      FiniteFieldPoly g = monic_normalize(field, univariate_gcd(field, poly, r));
      auto g_degree = static_cast<unsigned>(g.degree_in(0));
      if (g_degree == 0) {
        FiniteFieldPoly power = poly_powmod(field, r, exponent, poly);
        FiniteFieldPoly power_minus_one =
            power - FiniteFieldPoly::constant(field, poly.variables(), field.one());
        g = monic_normalize(field, univariate_gcd(field, poly, power_minus_one));
        g_degree = static_cast<unsigned>(g.degree_in(0));
      }

      if (g_degree > 0 && g_degree < degree) {
        FiniteFieldPoly quotient = univariate_div_rem(field, poly, g).first;
        pending.push_back(std::move(g));
        pending.push_back(std::move(quotient));
        split = true;
      }
    }
    if (!split) {
      throw std::runtime_error(
          "poly_core::equal_degree_split: exhausted kMaxSplitAttempts without finding a splitting "
          "polynomial");
    }
  }
  return result;
}

[[nodiscard]] inline std::vector<FiniteFieldPoly> factor_squarefree(const FiniteFieldRing64& field,
                                                                    std::uint64_t prime,
                                                                    FiniteFieldPoly f,
                                                                    std::mt19937_64& rng) {
  std::vector<FiniteFieldPoly> result;
  for (auto& [degree, group] : distinct_degree_split(field, prime, std::move(f))) {
    for (auto& factor : equal_degree_split(field, prime, std::move(group), degree, rng)) {
      result.push_back(std::move(factor));
    }
  }
  return result;
}

} // namespace detail

/// @brief Factors @p f over the finite field Z/`prime`Z: `f ==
/// leading_coefficient * product(factor^multiplicity)`, each `factor`
/// monic and irreducible.
/// @param prime The field's modulus. Must be an odd prime (not checked for
///        primality - the caller's responsibility, same as
///        FiniteFieldRing's own constructor).
/// @param rng_seed Seeds the Cantor-Zassenhaus random search
///        (deterministic/reproducible for a given seed).
/// @throws std::invalid_argument if @p f is not univariate, is zero, or if
///         @p prime == 2.
[[nodiscard]] inline Factorization factor_univariate_finite_field(const FiniteFieldRing64& field,
                                                                  std::uint64_t prime,
                                                                  const FiniteFieldPoly& f,
                                                                  std::uint64_t rng_seed = 0) {
  if (f.variables().size() != 1) {
    throw std::invalid_argument(
        "poly_core::factor_univariate_finite_field: expected a univariate polynomial");
  }
  if (f.is_zero()) {
    throw std::invalid_argument(
        "poly_core::factor_univariate_finite_field: cannot factor the zero polynomial");
  }
  if (prime == 2) {
    throw std::invalid_argument("poly_core::factor_univariate_finite_field: requires an odd prime");
  }

  Factorization result{f.leading_term().second, {}};
  FiniteFieldPoly monic_f = detail::monic_normalize(field, f);
  std::mt19937_64 rng(rng_seed);

  for (auto& [group, multiplicity] : detail::squarefree_decomposition(field, monic_f)) {
    for (auto& factor : detail::factor_squarefree(field, prime, std::move(group), rng)) {
      result.factors.push_back({std::move(factor), multiplicity});
    }
  }
  return result;
}

} // namespace poly_core
