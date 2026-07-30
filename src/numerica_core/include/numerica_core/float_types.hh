#pragma once

#include <compare>
#include <memory>
#include <string>

/// @file
/// @brief Native double (F64) and arbitrary-precision (Float, MPFR-backed)
/// floating-point types behind a common set of operations.
namespace numerica_core {

/// @brief Native double-precision float, wrapped to present the same
/// operation set as Float so both can be used interchangeably by generic
/// code (e.g. ErrorPropagatingFloat<F>).
class F64 {
public:
  F64() noexcept : value_(0.0) {}
  F64(double value) noexcept : value_(value) {} // NOLINT(google-explicit-constructor)

  [[nodiscard]] double value() const noexcept { return value_; }

  [[nodiscard]] F64 operator+(const F64& other) const { return F64(value_ + other.value_); }
  [[nodiscard]] F64 operator-(const F64& other) const { return F64(value_ - other.value_); }
  [[nodiscard]] F64 operator*(const F64& other) const { return F64(value_ * other.value_); }
  [[nodiscard]] F64 operator/(const F64& other) const { return F64(value_ / other.value_); }
  [[nodiscard]] F64 operator-() const { return F64(-value_); }

  [[nodiscard]] bool operator==(const F64& other) const { return value_ == other.value_; }
  [[nodiscard]] auto operator<=>(const F64& other) const { return value_ <=> other.value_; }

  [[nodiscard]] double to_double() const { return value_; }
  [[nodiscard]] std::string to_string() const;

  [[nodiscard]] static F64 exp(const F64& x);
  /// @brief exp(x) - 1, computed without catastrophic cancellation for
  /// small |x| (delegates to std::expm1).
  [[nodiscard]] static F64 expm1(const F64& x);
  [[nodiscard]] static F64 log(const F64& x);

private:
  double value_;
};

/// @brief Arbitrary-precision floating point, backed by MPFR.
class Float {
public:
  static constexpr unsigned kDefaultPrecisionBits = 256;

  /// @brief Constructs a zero-valued Float at @p precision_bits.
  explicit Float(unsigned precision_bits = kDefaultPrecisionBits);

  /// @brief Constructs from a native double at @p precision_bits.
  Float(double value, unsigned precision_bits = kDefaultPrecisionBits);

  Float(const Float& other);
  Float(Float&& other) noexcept;
  Float& operator=(const Float& other);
  Float& operator=(Float&& other) noexcept;
  ~Float();

  [[nodiscard]] unsigned precision_bits() const noexcept;

  [[nodiscard]] Float operator+(const Float& other) const;
  [[nodiscard]] Float operator-(const Float& other) const;
  [[nodiscard]] Float operator*(const Float& other) const;
  [[nodiscard]] Float operator/(const Float& other) const;
  [[nodiscard]] Float operator-() const;

  [[nodiscard]] bool operator==(const Float& other) const;
  [[nodiscard]] std::strong_ordering operator<=>(const Float& other) const;

  [[nodiscard]] double to_double() const;
  /// @return A decimal string with at least @p significant_digits digits.
  [[nodiscard]] std::string to_string(unsigned significant_digits = 20) const;

  [[nodiscard]] static Float exp(const Float& x);
  /// @brief exp(x) - 1, computed via MPFR's mpfr_expm1 - accurate for small
  /// |x| where naive exp(x) - 1 suffers catastrophic cancellation.
  [[nodiscard]] static Float expm1(const Float& x);
  [[nodiscard]] static Float log(const Float& x);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace numerica_core
