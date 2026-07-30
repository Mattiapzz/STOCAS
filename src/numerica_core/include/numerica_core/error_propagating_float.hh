#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>

/// @file
/// @brief A float wrapper that tracks precision loss (in valid bits) through
/// arithmetic, so catastrophic cancellation shows up as measurable data
/// instead of a silently wrong answer.
namespace numerica_core {

/// @brief Constrains ErrorPropagatingFloat<F> to the underlying float types
/// this project provides (F64, Float), both of which expose the same
/// operation set (+, -, *, /, unary -, comparisons, to_double()).
template <typename F>
concept FloatLike = requires(const F& a, const F& b) {
  { a + b } -> std::same_as<F>;
  { a - b } -> std::same_as<F>;
  { a* b } -> std::same_as<F>;
  { a / b } -> std::same_as<F>;
  { a.to_double() } -> std::same_as<double>;
};

/// @brief Wraps a float type F, tracking how many bits of precision remain
/// valid after a sequence of operations.
///
/// The model: every value starts with `total_precision_bits` valid bits.
/// Addition/multiplication/division conservatively propagate the minimum
/// precision of their operands. Subtraction is where precision is actually
/// lost: when two nearly-equal values are subtracted, the result's
/// magnitude shrinks relative to the operands' magnitude, and the bits
/// "used up" by that shrinkage (the classic catastrophic-cancellation
/// mechanism) are deducted from the remaining valid-bit count.
///
/// This mirrors the (e^x - 1)/x example from the design discussion: naively
/// computing exp(x) - 1 for tiny x cancels almost all of exp(x)'s valid
/// bits, which this type surfaces as a near-zero `precision_bits()` on the
/// subtraction's result - instead of a value that merely *looks* fine.
template <FloatLike F> class ErrorPropagatingFloat {
public:
  /// @brief Wraps @p value, treating it as exact to @p total_precision_bits.
  ErrorPropagatingFloat(F value, int total_precision_bits)
    : value_(std::move(value)), total_precision_bits_(total_precision_bits),
      precision_bits_(total_precision_bits) {}

  [[nodiscard]] const F& value() const noexcept { return value_; }

  /// @return The number of bits still considered numerically valid (can be
  ///         as low as 0 after severe cancellation).
  [[nodiscard]] int precision_bits() const noexcept { return precision_bits_; }

  [[nodiscard]] ErrorPropagatingFloat operator+(const ErrorPropagatingFloat& other) const {
    return ErrorPropagatingFloat(value_ + other.value_,
                                 total_precision_bits_,
                                 std::min(precision_bits_, other.precision_bits_));
  }

  [[nodiscard]] ErrorPropagatingFloat operator*(const ErrorPropagatingFloat& other) const {
    return ErrorPropagatingFloat(value_ * other.value_,
                                 total_precision_bits_,
                                 std::min(precision_bits_, other.precision_bits_));
  }

  [[nodiscard]] ErrorPropagatingFloat operator/(const ErrorPropagatingFloat& other) const {
    return ErrorPropagatingFloat(value_ / other.value_,
                                 total_precision_bits_,
                                 std::min(precision_bits_, other.precision_bits_));
  }

  /// @brief Subtraction, with cancellation-aware precision tracking: bits
  /// lost = log2(max(|a|, |b|) / |a - b|), clamped to
  /// [0, min(a.precision_bits, b.precision_bits)].
  [[nodiscard]] ErrorPropagatingFloat operator-(const ErrorPropagatingFloat& other) const {
    const F result_value = value_ - other.value_;
    const double result_magnitude = std::abs(result_value.to_double());
    const double operand_magnitude =
        std::max(std::abs(value_.to_double()), std::abs(other.value_.to_double()));

    int result_precision = std::min(precision_bits_, other.precision_bits_);
    if (operand_magnitude > 0.0) {
      const double bits_lost = result_magnitude > 0.0
                                   ? std::log2(operand_magnitude / result_magnitude)
                                   : static_cast<double>(result_precision);
      result_precision -= static_cast<int>(std::max(0.0, bits_lost));
    }
    result_precision = std::max(0, result_precision);

    return ErrorPropagatingFloat(result_value, total_precision_bits_, result_precision);
  }

private:
  F value_;
  int total_precision_bits_;
  int precision_bits_;

  ErrorPropagatingFloat(F value, int total_precision_bits, int precision_bits)
    : value_(std::move(value)), total_precision_bits_(total_precision_bits),
      precision_bits_(precision_bits) {}
};

} // namespace numerica_core
