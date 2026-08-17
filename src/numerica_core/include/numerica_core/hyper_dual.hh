#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>

#include <numerica_core/dual.hh>

/// @file
/// @brief Generic-order forward-mode automatic differentiation via
/// truncated Taylor series.
///
/// `+`/`-`/`*`/`/` and composition with `exp`/`log`/`sin`/`cos` are all
/// supported (the last three via the standard Taylor-coefficient
/// recurrences - see each method's comment). **Taylor only, not Laurent**:
/// every operation here assumes the result is regular (has no pole) at the
/// expansion point - `operator/` and `log()` throw `std::domain_error`
/// rather than produce a `Inf`/`NaN` coefficient when that assumption is
/// violated (a zero divisor constant term / a non-positive log argument,
/// respectively). Automatic pole-order detection (to support genuine
/// Laurent series, e.g. expanding `1/x` around `x=0`) is a documented,
/// deliberately out-of-scope gap for `calculus_core`'s M6-T2 series
/// expansion - callers hitting a pole get a clear, honest error instead of
/// silent garbage, mirroring `poly_core`'s bounded-search /
/// `pattern_core`'s attempt-budget precedent of reporting failure rather
/// than producing a wrong answer.
namespace numerica_core {

/// @brief A truncated Taylor series a_0 + a_1*(x-x0) + ... + a_N*(x-x0)^N,
/// generic over derivative order N. Coefficient a_k is the k-th Taylor
/// coefficient (i.e. f^(k)(x0) / k!, not the raw derivative).
///
/// This generalizes Dual<F> (which is the Order == 1 case) to arbitrary
/// derivative order, needed by calculus_core's series expansion (M6).
template <DualScalar F, std::size_t Order> class HyperDual {
public:
  explicit HyperDual(std::array<F, Order + 1> coefficients)
    : coefficients_(std::move(coefficients)) {}

  /// @brief An independent variable at @p value: coefficient 1 is 1, all
  /// higher-order coefficients are 0 (requires Order >= 1).
  [[nodiscard]] static HyperDual variable(F value) {
    static_assert(Order >= 1, "HyperDual::variable requires Order >= 1");
    std::array<F, Order + 1> coefficients{};
    coefficients[0] = std::move(value);
    coefficients[1] = F(1.0);
    for (std::size_t k = 2; k <= Order; ++k) {
      coefficients[k] = F(0.0);
    }
    return HyperDual(std::move(coefficients));
  }

  /// @brief A constant: coefficient 0 is @p value, all others 0.
  [[nodiscard]] static HyperDual constant(F value) {
    std::array<F, Order + 1> coefficients{};
    coefficients[0] = std::move(value);
    for (std::size_t k = 1; k <= Order; ++k) {
      coefficients[k] = F(0.0);
    }
    return HyperDual(std::move(coefficients));
  }

  [[nodiscard]] const F& coefficient(std::size_t k) const noexcept { return coefficients_[k]; }

  /// @return The k-th derivative f^(k)(x0), for 0 <= k <= Order.
  [[nodiscard]] F derivative(std::size_t k) const {
    F result = coefficients_[k];
    for (std::size_t i = 2; i <= k; ++i) {
      result = result * F(static_cast<double>(i));
    }
    return result;
  }

  [[nodiscard]] HyperDual operator+(const HyperDual& other) const {
    std::array<F, Order + 1> result{};
    for (std::size_t k = 0; k <= Order; ++k) {
      result[k] = coefficients_[k] + other.coefficients_[k];
    }
    return HyperDual(std::move(result));
  }

  [[nodiscard]] HyperDual operator-(const HyperDual& other) const {
    std::array<F, Order + 1> result{};
    for (std::size_t k = 0; k <= Order; ++k) {
      result[k] = coefficients_[k] - other.coefficients_[k];
    }
    return HyperDual(std::move(result));
  }

  /// @brief Cauchy product: (fg)_k = sum_{i=0}^{k} f_i * g_{k-i}.
  [[nodiscard]] HyperDual operator*(const HyperDual& other) const {
    std::array<F, Order + 1> result{};
    for (std::size_t k = 0; k <= Order; ++k) {
      F sum = coefficients_[0] * other.coefficients_[k];
      for (std::size_t i = 1; i <= k; ++i) {
        sum = sum + coefficients_[i] * other.coefficients_[k - i];
      }
      result[k] = sum;
    }
    return HyperDual(std::move(result));
  }

  /// @brief Division, solved from the Cauchy product identity `this ==
  /// (this/other) * other`: `q_0 = f_0/g_0`, then for `k >= 1`,
  /// `q_k = (f_k - sum_{i=0}^{k-1} q_i * g_{k-i}) / g_0`.
  /// @throws std::domain_error if `other`'s constant term is `0` - a
  ///         genuine pole at the expansion point, which this Taylor-only
  ///         type cannot represent (see hyper_dual.hh's file comment).
  [[nodiscard]] HyperDual operator/(const HyperDual& other) const {
    if (other.coefficients_[0] == F(0.0)) {
      throw std::domain_error(
          "HyperDual::operator/: divisor has a zero constant term (a pole at "
          "the expansion point) - only Taylor (non-singular) series are supported");
    }
    std::array<F, Order + 1> q{};
    q[0] = coefficients_[0] / other.coefficients_[0];
    for (std::size_t k = 1; k <= Order; ++k) {
      F sum = coefficients_[k];
      for (std::size_t i = 0; i < k; ++i) {
        sum = sum - q[i] * other.coefficients_[k - i];
      }
      q[k] = sum / other.coefficients_[0];
    }
    return HyperDual(std::move(q));
  }

  /// @brief Composition with exp: standard Taylor-coefficient recurrence
  /// c_0 = exp(b_0); c_k = (1/k) * sum_{i=1}^{k} i * b_i * c_{k-i}.
  [[nodiscard]] static HyperDual exp(const HyperDual& x) {
    std::array<F, Order + 1> c{};
    c[0] = F::exp(x.coefficients_[0]);
    for (std::size_t k = 1; k <= Order; ++k) {
      F sum(0.0);
      for (std::size_t i = 1; i <= k; ++i) {
        sum = sum + F(static_cast<double>(i)) * x.coefficients_[i] * c[k - i];
      }
      c[k] = sum / F(static_cast<double>(k));
    }
    return HyperDual(std::move(c));
  }

  /// @brief Composition with log: `exp`'s inverse recurrence, derived from
  /// `(log(g))' == g'/g`: `c_0 = log(b_0)`, then for `k >= 1`,
  /// `c_k = (b_k - (1/k) * sum_{i=1}^{k-1} i * c_i * b_{k-i}) / b_0`.
  /// @throws std::domain_error if `x`'s constant term is `<= 0` (real log
  ///         is undefined there).
  [[nodiscard]] static HyperDual log(const HyperDual& x) {
    if (!(x.coefficients_[0] > F(0.0))) {
      throw std::domain_error("HyperDual::log: constant term must be positive");
    }
    std::array<F, Order + 1> c{};
    c[0] = F::log(x.coefficients_[0]);
    for (std::size_t k = 1; k <= Order; ++k) {
      F sum(0.0);
      for (std::size_t i = 1; i < k; ++i) {
        sum = sum + F(static_cast<double>(i)) * c[i] * x.coefficients_[k - i];
      }
      c[k] = (x.coefficients_[k] - sum / F(static_cast<double>(k))) / x.coefficients_[0];
    }
    return HyperDual(std::move(c));
  }

  /// @brief Composition with sin, computed jointly with cos (they're
  /// mutually recursive: `sin' = cos`, `cos' = -sin`): `s_0 = sin(b_0)`,
  /// `c_0 = cos(b_0)`, then for `k >= 1`, `k*s_k = sum_{i=1}^{k} i*b_i*c_{k-i}`
  /// and `k*c_k = -sum_{i=1}^{k} i*b_i*s_{k-i}`.
  [[nodiscard]] static HyperDual sin(const HyperDual& x)
    requires TrigDualScalar<F>
  {
    return sin_cos(x).first;
  }

  /// @brief Composition with cos - see `sin()`'s comment for the joint
  /// recurrence.
  [[nodiscard]] static HyperDual cos(const HyperDual& x)
    requires TrigDualScalar<F>
  {
    return sin_cos(x).second;
  }

private:
  std::array<F, Order + 1> coefficients_;

  [[nodiscard]] static std::pair<HyperDual, HyperDual> sin_cos(const HyperDual& x)
    requires TrigDualScalar<F>
  {
    std::array<F, Order + 1> s{};
    std::array<F, Order + 1> c{};
    s[0] = F::sin(x.coefficients_[0]);
    c[0] = F::cos(x.coefficients_[0]);
    for (std::size_t k = 1; k <= Order; ++k) {
      F sin_sum(0.0);
      F cos_sum(0.0);
      for (std::size_t i = 1; i <= k; ++i) {
        const F term = F(static_cast<double>(i)) * x.coefficients_[i];
        sin_sum = sin_sum + term * c[k - i];
        cos_sum = cos_sum + term * s[k - i];
      }
      s[k] = sin_sum / F(static_cast<double>(k));
      c[k] = -(cos_sum / F(static_cast<double>(k)));
    }
    return {HyperDual(std::move(s)), HyperDual(std::move(c))};
  }
};

} // namespace numerica_core
