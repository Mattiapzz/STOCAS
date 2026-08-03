#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <algebra_core/concepts.hh>
#include <algebra_core/rings.hh>
#include <numerica_core/finite_field.hh>
#include <numerica_core/rational.hh>
#include <poly_core/monomial.hh>
#include <poly_core/multivariate_division.hh>
#include <poly_core/polynomial.hh>
#include <poly_core/univariate_gcd.hh>

/// @file
/// @brief Multivariate polynomial GCD (M4-T2): dense evaluation/
/// interpolation over any `algebra_core::Field` (`finite_field_multivariate_gcd`),
/// plus a `RationalField`-specific entry point (`rational_multivariate_gcd`)
/// that reduces mod a large prime, computes the finite-field GCD, lifts
/// coefficients back to ℚ via `numerica_core::Rational::reconstruct`, and
/// verifies the result by exact multivariate division before returning it -
/// retrying with the next prime in `kMultivariateGcdPrimes` on any failure
/// (unlucky prime, failed reconstruction, or a failed verification).
///
/// Algorithm (classic dense interpolation, see e.g. Geddes/Czapor/Labahn
/// ch. 7): to compute gcd(a, b) over n variables, fix variable 0 as "main"
/// and eliminate the other n-1 variables one at a time - for each,
/// evaluate it at `degree_bound + 1` field points (the bound being
/// `min(deg_in(a, v), deg_in(b, v))`, a valid upper bound on the gcd's
/// degree in `v` throughout, since evaluating other variables never
/// increases it), recursing until every variable but the main one is a
/// concrete scalar, at which point a genuinely univariate `univariate_gcd`
/// (monic-normalized) is computed. Recursion unwinds via Lagrange
/// interpolation, rebuilding each eliminated variable from its sample
/// polynomials. Monic normalization at each univariate leaf (rather than
/// per-interpolation-level leading-coefficient tracking, the usual "hard
/// part" of production multivariate GCD algorithms) keeps every sample
/// polynomial's leading term consistent, at the cost of not being robust to
/// every pathological "unlucky evaluation point" case - which is exactly
/// why the top-level `rational_multivariate_gcd` verifies the final result
/// by exact division and retries on failure rather than trusting the
/// modular computation blindly.
///
/// Deferred: combining several primes via CRT for coefficients too large
/// for a single prime's reconstruction bound (`kMultivariateGcdPrimes` is
/// tried one prime at a time, not combined) - out of scope for M4-T2, noted
/// in the roadmap as a later enhancement, same spirit as M4-T1 deferring
/// subresultant PRS.
namespace poly_core {

namespace detail {

template <typename Field>
[[nodiscard]] typename Field::Element field_element_from_index(const Field& field,
                                                               std::size_t index) {
  typename Field::Element result = field.zero();
  const typename Field::Element one = field.one();
  for (std::size_t i = 0; i < index; ++i) {
    result = field.add(result, one);
  }
  return result;
}

template <typename Field>
[[nodiscard]] MultivariatePolynomial<Field> monic_normalize(const Field& field,
                                                            MultivariatePolynomial<Field> poly) {
  if (poly.is_zero()) {
    return poly;
  }
  typename Field::Element inv_lead = field.inv(poly.leading_term().second);
  MultivariatePolynomial<Field> scale =
      MultivariatePolynomial<Field>::constant(field, poly.variables(), inv_lead);
  return poly * scale;
}

// Lagrange-interpolates `variable_index` back into a set of sample
// polynomials (each already over the full `variables` list, per
// MultivariatePolynomial<R>::evaluate()'s "keep the variable list, freeze
// the exponent at 0" convention) taken at `points`.
template <typename Field>
[[nodiscard]] MultivariatePolynomial<Field>
lagrange_interpolate(const Field& field,
                     std::size_t variable_index,
                     const std::vector<symbol_table::Symbol>& variables,
                     const std::vector<typename Field::Element>& points,
                     const std::vector<MultivariatePolynomial<Field>>& samples) {
  MultivariatePolynomial<Field> result(field, variables);
  std::vector<Exponent> var_exponents(variables.size(), 0);
  var_exponents[variable_index] = 1;
  MultivariatePolynomial<Field> variable_poly = MultivariatePolynomial<Field>::from_term(
      field, variables, Monomial(var_exponents), field.one());

  for (std::size_t i = 0; i < points.size(); ++i) {
    MultivariatePolynomial<Field> basis =
        MultivariatePolynomial<Field>::constant(field, variables, field.one());
    typename Field::Element denominator = field.one();
    for (std::size_t j = 0; j < points.size(); ++j) {
      if (j == i) {
        continue;
      }
      MultivariatePolynomial<Field> linear_factor =
          variable_poly - MultivariatePolynomial<Field>::constant(field, variables, points[j]);
      basis = basis * linear_factor;
      denominator = field.mul(denominator, field.sub(points[i], points[j]));
    }
    MultivariatePolynomial<Field> scaled_basis =
        basis * MultivariatePolynomial<Field>::constant(field, variables, field.inv(denominator));
    result = result + (samples[i] * scaled_basis);
  }
  return result;
}

// Maximum number of evaluation-point-set retries gcd_eliminate() attempts
// per variable elimination before giving up (see the "unlucky evaluation
// point" note above lagrange_interpolate()).
inline constexpr std::size_t kMaxEliminationAttempts = 8;

// Recursively eliminates the variables in `other_var_indices[pos..]` (by
// evaluation + interpolation), reducing to a genuinely univariate GCD in
// `main_var_index` once none are left.
//
// Evaluation points aren't just "0, 1, 2, ...": a variable's evaluation
// point can be "unlucky" not only via the well-known leading-coefficient-
// vanishes-mod-p case, but structurally - e.g. evaluating y=0 collapses
// both `x+y` and `x+2y` to `x`, spuriously inflating the sampled gcd's
// degree, an artifact of the specific point rather than of the modulus.
// To catch this, each attempt samples one extra "check point" beyond what
// interpolation strictly needs: the interpolated candidate is evaluated
// back at the check point and compared against the check point's own
// (independently computed) sample. A mismatch means the point set was
// unlucky; retry with a shifted offset. `point_seed` lets nested
// eliminations (and repeated attempts) draw from disjoint point ranges
// rather than all starting from 0.
template <typename Field>
[[nodiscard]] MultivariatePolynomial<Field>
gcd_eliminate(const Field& field,
              const MultivariatePolynomial<Field>& a,
              const MultivariatePolynomial<Field>& b,
              std::size_t main_var_index,
              const std::vector<std::size_t>& other_var_indices,
              std::size_t pos,
              const std::vector<Exponent>& degree_bounds,
              std::size_t point_seed = 0) {
  if (pos == other_var_indices.size()) {
    MultivariatePolynomial<Field> gcd_univariate =
        univariate_gcd(field, a.to_univariate(main_var_index), b.to_univariate(main_var_index));
    return MultivariatePolynomial<Field>::embed_univariate(
        field, a.variables(), main_var_index, gcd_univariate);
  }

  const std::size_t var_index = other_var_indices[pos];
  const std::size_t num_points = static_cast<std::size_t>(degree_bounds[var_index]) + 1;

  for (std::size_t attempt = 0; attempt < kMaxEliminationAttempts; ++attempt) {
    const std::size_t offset = point_seed + attempt * (num_points + 1) + 1; // never include 0

    std::vector<typename Field::Element> points;
    std::vector<MultivariatePolynomial<Field>> samples;
    points.reserve(num_points + 1);
    samples.reserve(num_points + 1);
    for (std::size_t i = 0; i < num_points + 1; ++i) {
      typename Field::Element point = field_element_from_index(field, offset + i);
      MultivariatePolynomial<Field> a_sub = a.evaluate(var_index, point);
      MultivariatePolynomial<Field> b_sub = b.evaluate(var_index, point);
      points.push_back(point);
      samples.push_back(gcd_eliminate(
          field, a_sub, b_sub, main_var_index, other_var_indices, pos + 1, degree_bounds, offset));
    }

    std::vector<typename Field::Element> interp_points(points.begin(), points.begin() + num_points);
    std::vector<MultivariatePolynomial<Field>> interp_samples(samples.begin(),
                                                              samples.begin() + num_points);
    MultivariatePolynomial<Field> candidate =
        lagrange_interpolate(field, var_index, a.variables(), interp_points, interp_samples);

    if (candidate.evaluate(var_index, points[num_points]) == samples[num_points]) {
      return candidate;
    }
  }
  throw std::domain_error(
      "poly_core: unable to find a lucky evaluation point set for multivariate GCD interpolation");
}

} // namespace detail

/// @brief Multivariate GCD over any field, via dense evaluation/
/// interpolation, normalized to be monic (leading coefficient 1, w.r.t.
/// `Monomial`'s lexicographic order) - the canonical "gcd up to units"
/// representative.
/// @throws std::invalid_argument if @p a and @p b don't share the same
///         variable list.
template <typename Field>
  requires algebra_core::Field<Field>
[[nodiscard]] MultivariatePolynomial<Field>
finite_field_multivariate_gcd(const Field& field,
                              const MultivariatePolynomial<Field>& a,
                              const MultivariatePolynomial<Field>& b) {
  if (a.variables().size() != b.variables().size()) {
    throw std::invalid_argument(
        "poly_core::finite_field_multivariate_gcd: mismatched variable lists");
  }
  for (std::size_t i = 0; i < a.variables().size(); ++i) {
    if (!(a.variables()[i] == b.variables()[i])) {
      throw std::invalid_argument(
          "poly_core::finite_field_multivariate_gcd: mismatched variable lists");
    }
  }
  if (a.is_zero()) {
    return detail::monic_normalize(field, b);
  }
  if (b.is_zero()) {
    return detail::monic_normalize(field, a);
  }

  const std::size_t n = a.variables().size();
  std::vector<Exponent> degree_bounds(n);
  for (std::size_t i = 0; i < n; ++i) {
    degree_bounds[i] = std::min(a.degree_in(i), b.degree_in(i));
  }

  if (n == 1) {
    MultivariatePolynomial<Field> gcd_univariate =
        univariate_gcd(field, a.to_univariate(0), b.to_univariate(0));
    return MultivariatePolynomial<Field>::embed_univariate(field, a.variables(), 0, gcd_univariate);
  }

  std::vector<std::size_t> other_var_indices;
  other_var_indices.reserve(n - 1);
  for (std::size_t i = 1; i < n; ++i) {
    other_var_indices.push_back(i);
  }
  MultivariatePolynomial<Field> gcd_result =
      detail::gcd_eliminate(field, a, b, 0, other_var_indices, 0, degree_bounds);
  return detail::monic_normalize(field, gcd_result);
}

/// @brief A short, fixed list of known primes (Mersenne primes M31 and
/// M61), tried one at a time by rational_multivariate_gcd(). Combining
/// several primes via CRT for coefficients too large for a single prime's
/// reconstruction bound is deferred (see this file's header comment).
inline constexpr std::array<std::uint64_t, 2> kMultivariateGcdPrimes = {
    2147483647ULL,          // 2^31 - 1
    2305843009213693951ULL, // 2^61 - 1
};

namespace detail {

[[nodiscard]] inline numerica_core::BigInt canonical_residue(const numerica_core::BigInt& value,
                                                             const numerica_core::BigInt& modulus) {
  numerica_core::BigInt residue = value % modulus;
  if (residue < numerica_core::BigInt(0)) {
    residue = residue + modulus;
  }
  return residue;
}

// numerica_core::BigInt exposes no direct accessor into a native integer
// type (only to_string()/arithmetic); this is the only public-API way to
// extract a value already known to be small (< 2^63, guaranteed for every
// residue mod one of kMultivariateGcdPrimes).
[[nodiscard]] inline std::uint64_t bigint_to_u64(const numerica_core::BigInt& value) {
  return std::stoull(value.to_string());
}

/// @throws std::domain_error if @p value's denominator is `== 0 (mod prime)`
///         - an unlucky prime for this value, signaling the caller to retry.
[[nodiscard]] inline numerica_core::FiniteField<std::uint64_t>
reduce_rational_mod(const numerica_core::Rational& value, std::uint64_t prime) {
  numerica_core::BigInt modulus(static_cast<std::int64_t>(prime));
  numerica_core::BigInt numerator_residue = canonical_residue(value.numerator(), modulus);
  numerica_core::BigInt denominator_residue = canonical_residue(value.denominator(), modulus);
  std::uint64_t denominator_u64 = bigint_to_u64(denominator_residue);
  if (denominator_u64 == 0) {
    throw std::domain_error("poly_core: unlucky prime (a denominator vanishes mod p)");
  }
  numerica_core::FiniteField<std::uint64_t> numerator_ff(prime, bigint_to_u64(numerator_residue));
  numerica_core::FiniteField<std::uint64_t> denominator_ff(prime, denominator_u64);
  return numerator_ff * denominator_ff.inv();
}

[[nodiscard]] inline MultivariatePolynomial<algebra_core::FiniteFieldRing<std::uint64_t>>
reduce_polynomial_mod(const MultivariatePolynomial<algebra_core::RationalField>& poly,
                      const algebra_core::FiniteFieldRing<std::uint64_t>& field_ring,
                      std::uint64_t prime) {
  MultivariatePolynomial<algebra_core::FiniteFieldRing<std::uint64_t>> result(field_ring,
                                                                              poly.variables());
  for (const auto& term : poly.terms()) {
    numerica_core::FiniteField<std::uint64_t> reduced_coeff =
        reduce_rational_mod(term.second, prime);
    result =
        result + MultivariatePolynomial<algebra_core::FiniteFieldRing<std::uint64_t>>::from_term(
                     field_ring, poly.variables(), term.first, reduced_coeff);
  }
  return result;
}

/// @return std::nullopt if any coefficient fails to reconstruct within
///         Rational::reconstruct()'s default bound.
[[nodiscard]] inline std::optional<MultivariatePolynomial<algebra_core::RationalField>>
reconstruct_polynomial(
    const MultivariatePolynomial<algebra_core::FiniteFieldRing<std::uint64_t>>& poly,
    const algebra_core::RationalField& rationals,
    std::uint64_t prime) {
  numerica_core::BigInt modulus(static_cast<std::int64_t>(prime));
  MultivariatePolynomial<algebra_core::RationalField> result(rationals, poly.variables());
  for (const auto& term : poly.terms()) {
    numerica_core::BigInt residue(static_cast<std::int64_t>(term.second.value()));
    std::optional<numerica_core::Rational> reconstructed =
        numerica_core::Rational::reconstruct(residue, modulus);
    if (!reconstructed.has_value()) {
      return std::nullopt;
    }
    result = result + MultivariatePolynomial<algebra_core::RationalField>::from_term(
                          rationals, poly.variables(), term.first, *reconstructed);
  }
  return result;
}

} // namespace detail

/// @brief Multivariate GCD over ℚ: reduces @p a, @p b mod a large prime,
/// computes the finite-field GCD (finite_field_multivariate_gcd), lifts
/// each coefficient back to ℚ via rational reconstruction, and verifies the
/// result by exact multivariate division before returning it - retrying
/// with the next prime in kMultivariateGcdPrimes on any failure (unlucky
/// prime, failed reconstruction, or a failed division check).
/// @throws std::invalid_argument if @p a and @p b don't share the same
///         variable list.
/// @throws std::runtime_error if every prime in kMultivariateGcdPrimes
///         fails (extremely unlikely for the fixed, well-known primes
///         used here, absent pathological coefficients).
[[nodiscard]] inline MultivariatePolynomial<algebra_core::RationalField>
rational_multivariate_gcd(const algebra_core::RationalField& rationals,
                          const MultivariatePolynomial<algebra_core::RationalField>& a,
                          const MultivariatePolynomial<algebra_core::RationalField>& b) {
  if (a.variables().size() != b.variables().size()) {
    throw std::invalid_argument("poly_core::rational_multivariate_gcd: mismatched variable lists");
  }
  for (std::size_t i = 0; i < a.variables().size(); ++i) {
    if (!(a.variables()[i] == b.variables()[i])) {
      throw std::invalid_argument(
          "poly_core::rational_multivariate_gcd: mismatched variable lists");
    }
  }
  if (a.is_zero() && b.is_zero()) {
    return MultivariatePolynomial<algebra_core::RationalField>(rationals, a.variables());
  }

  for (std::uint64_t prime : kMultivariateGcdPrimes) {
    try {
      algebra_core::FiniteFieldRing<std::uint64_t> field_ring(prime);
      auto a_mod = detail::reduce_polynomial_mod(a, field_ring, prime);
      auto b_mod = detail::reduce_polynomial_mod(b, field_ring, prime);
      auto gcd_mod = finite_field_multivariate_gcd(field_ring, a_mod, b_mod);

      std::optional<MultivariatePolynomial<algebra_core::RationalField>> gcd_candidate =
          detail::reconstruct_polynomial(gcd_mod, rationals, prime);
      if (!gcd_candidate.has_value() || gcd_candidate->is_zero()) {
        continue;
      }

      auto [quotient_a, remainder_a] = multivariate_div_rem(rationals, a, *gcd_candidate);
      if (!remainder_a.is_zero()) {
        continue;
      }
      auto [quotient_b, remainder_b] = multivariate_div_rem(rationals, b, *gcd_candidate);
      if (!remainder_b.is_zero()) {
        continue;
      }
      return *gcd_candidate;
    } catch (const std::domain_error&) {
      continue; // Unlucky prime; try the next one.
    }
  }
  throw std::runtime_error(
      "poly_core::rational_multivariate_gcd: failed to compute a verified gcd (exhausted "
      "kMultivariateGcdPrimes)");
}

} // namespace poly_core
