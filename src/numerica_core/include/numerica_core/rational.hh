#pragma once

#include <compare>
#include <optional>
#include <string>

#include <numerica_core/big_int.hh>

/// @file
/// @brief Exact rational arithmetic and modular rational reconstruction.
namespace numerica_core {

/// @brief An exact fraction of two BigInt values, always kept in lowest
/// terms with a positive denominator.
class Rational {
public:
  /// @brief Constructs the zero rational (0/1).
  Rational() noexcept;

  /// @brief Constructs an integer rational (value/1).
  Rational(int64_t value) noexcept; // NOLINT(google-explicit-constructor)

  /// @brief Constructs an integer rational (value/1).
  explicit Rational(BigInt value);

  /// @brief Constructs numerator/denominator, auto-reducing to lowest terms
  /// and normalizing the sign so the denominator is always positive.
  /// @throws std::domain_error if @p denominator is zero.
  Rational(BigInt numerator, BigInt denominator);

  [[nodiscard]] const BigInt& numerator() const noexcept;
  [[nodiscard]] const BigInt& denominator() const noexcept;

  [[nodiscard]] Rational operator+(const Rational& other) const;
  [[nodiscard]] Rational operator-(const Rational& other) const;
  [[nodiscard]] Rational operator*(const Rational& other) const;

  /// @throws std::domain_error if @p other is zero.
  [[nodiscard]] Rational operator/(const Rational& other) const;

  [[nodiscard]] Rational operator-() const;

  /// @brief Multiplicative inverse.
  /// @throws std::domain_error if this value is zero.
  [[nodiscard]] Rational inv() const;

  [[nodiscard]] bool operator==(const Rational& other) const;
  [[nodiscard]] std::strong_ordering operator<=>(const Rational& other) const;

  [[nodiscard]] std::string to_string() const;

  /// @return `numerator() / denominator()` rounded to the nearest `double`
  ///         - see `BigInt::to_double()`'s precision caveat.
  [[nodiscard]] double to_double() const;

  /// @brief Rational reconstruction: recovers p/q from a residue r modulo m,
  /// via the extended Euclidean algorithm, given |p|, q <= bound.
  ///
  /// Used to lift results computed modulo a prime (e.g. polynomial GCD via
  /// finite-field evaluation) back to exact rationals over ℚ.
  ///
  /// @param residue The residue r, 0 <= r < modulus.
  /// @param modulus The modulus m > 0.
  /// @param bound The maximum absolute value permitted for numerator and
  ///        denominator. If not provided, defaults to floor(sqrt(m / 2)).
  /// @return The reconstructed Rational, or std::nullopt if no p/q with
  ///         |p|, q <= bound and p == q*residue (mod m) exists.
  [[nodiscard]] static std::optional<Rational> reconstruct(
      const BigInt& residue, const BigInt& modulus, std::optional<BigInt> bound = std::nullopt);

private:
  BigInt numerator_;
  BigInt denominator_{1};

  void reduce();
};

} // namespace numerica_core
