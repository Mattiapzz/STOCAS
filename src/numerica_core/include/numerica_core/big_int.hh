#pragma once

#include <compare>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>

/// @file
/// @brief Arbitrary-precision integer type with a small-int fast path.
namespace numerica_core {

/// @brief Arbitrary-precision signed integer.
///
/// Values that fit in an `int64_t` are stored inline with no heap
/// allocation or GMP involvement (the "small-int fast path"). Values
/// outside that range are promoted transparently to a GMP-backed `mpz_t`.
/// This promotion is an implementation detail invisible to callers.
class BigInt {
public:
  /// @brief Constructs a zero-valued BigInt.
  BigInt() noexcept;

  /// @brief Constructs from a native 64-bit integer (small-int fast path).
  BigInt(int64_t value) noexcept; // NOLINT(google-explicit-constructor)

  /// @brief Parses a base-10 string into a BigInt.
  /// @param decimal_string A string of the form `-?[0-9]+`.
  /// @throws std::invalid_argument if @p decimal_string is not a valid
  ///         base-10 integer literal.
  explicit BigInt(const std::string& decimal_string);

  BigInt(const BigInt& other);
  BigInt(BigInt&& other) noexcept;
  BigInt& operator=(const BigInt& other);
  BigInt& operator=(BigInt&& other) noexcept;
  ~BigInt();

  /// @return true if the value fits in the inline int64_t representation
  ///         (i.e. no GMP promotion has occurred).
  [[nodiscard]] bool is_small() const noexcept;

  /// @return The base-10 string representation of this value.
  [[nodiscard]] std::string to_string() const;

  [[nodiscard]] BigInt operator+(const BigInt& other) const;
  [[nodiscard]] BigInt operator-(const BigInt& other) const;
  [[nodiscard]] BigInt operator*(const BigInt& other) const;

  /// @brief Truncating (toward-zero) division.
  /// @throws std::domain_error if @p other is zero.
  [[nodiscard]] BigInt operator/(const BigInt& other) const;

  /// @brief Remainder of truncating division (same sign as the dividend).
  /// @throws std::domain_error if @p other is zero.
  [[nodiscard]] BigInt operator%(const BigInt& other) const;

  [[nodiscard]] BigInt operator-() const;

  [[nodiscard]] bool operator==(const BigInt& other) const;
  [[nodiscard]] std::strong_ordering operator<=>(const BigInt& other) const;

  /// @return The greatest common divisor of |a| and |b| (always non-negative).
  [[nodiscard]] static BigInt gcd(const BigInt& a, const BigInt& b);

  /// @brief Extended Euclidean algorithm.
  /// @return {g, x, y} such that a*x + b*y == g == gcd(a, b).
  [[nodiscard]] static std::tuple<BigInt, BigInt, BigInt> extended_gcd(const BigInt& a,
                                                                       const BigInt& b);

private:
  struct Impl;

  // Small-int fast path: no allocation, no GMP call, when the value fits.
  bool is_small_ = true;
  int64_t small_value_ = 0;

  // Populated only once promoted above the int64_t range.
  std::unique_ptr<Impl> impl_;

  void promote_to_big();
  [[nodiscard]] const Impl& big() const;
};

} // namespace numerica_core
