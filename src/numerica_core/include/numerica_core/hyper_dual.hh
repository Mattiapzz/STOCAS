#pragma once

#include <array>
#include <cstddef>

#include <numerica_core/dual.hh>

/// @file
/// @brief Generic-order forward-mode automatic differentiation via
/// truncated Taylor series.
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

private:
  std::array<F, Order + 1> coefficients_;
};

} // namespace numerica_core
