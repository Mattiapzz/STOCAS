#pragma once

#include <concepts>
#include <utility>

/// @file
/// @brief Forward-mode automatic differentiation number types.
namespace numerica_core {

/// @brief Constrains Dual<F>/HyperDual<F, Order> to scalar types providing
/// the arithmetic and transcendental operations forward-mode AD needs.
/// Both F64 and Float (numerica_core/float_types.hh) satisfy this.
template <typename F>
concept DualScalar = requires(const F& a, const F& b) {
  { a + b } -> std::same_as<F>;
  { a - b } -> std::same_as<F>;
  { a* b } -> std::same_as<F>;
  { a / b } -> std::same_as<F>;
  { F::exp(a) } -> std::same_as<F>;
  { F::log(a) } -> std::same_as<F>;
} && std::constructible_from<F, double>;

/// @brief A dual number a + b*epsilon (epsilon^2 == 0), carrying a value and
/// its first derivative through arithmetic via the chain rule.
template <DualScalar F> class Dual {
public:
  Dual(F value, F derivative) noexcept
    : value_(std::move(value)), derivative_(std::move(derivative)) {}

  /// @brief An independent variable: derivative 1 with respect to itself.
  [[nodiscard]] static Dual variable(F value) { return Dual(std::move(value), F(1.0)); }

  /// @brief A constant: derivative 0.
  [[nodiscard]] static Dual constant(F value) { return Dual(std::move(value), F(0.0)); }

  [[nodiscard]] const F& value() const noexcept { return value_; }
  [[nodiscard]] const F& derivative() const noexcept { return derivative_; }

  [[nodiscard]] Dual operator+(const Dual& other) const {
    return Dual(value_ + other.value_, derivative_ + other.derivative_);
  }

  [[nodiscard]] Dual operator-(const Dual& other) const {
    return Dual(value_ - other.value_, derivative_ - other.derivative_);
  }

  [[nodiscard]] Dual operator*(const Dual& other) const {
    return Dual(value_ * other.value_, derivative_ * other.value_ + value_ * other.derivative_);
  }

  [[nodiscard]] Dual operator/(const Dual& other) const {
    return Dual(value_ / other.value_,
                (derivative_ * other.value_ - value_ * other.derivative_) /
                    (other.value_ * other.value_));
  }

  [[nodiscard]] Dual operator-() const { return Dual(-value_, -derivative_); }

  [[nodiscard]] static Dual exp(const Dual& x) {
    const F exp_value = F::exp(x.value_);
    return Dual(exp_value, exp_value * x.derivative_);
  }

  [[nodiscard]] static Dual log(const Dual& x) {
    return Dual(F::log(x.value_), x.derivative_ / x.value_);
  }

private:
  F value_;
  F derivative_;
};

} // namespace numerica_core
