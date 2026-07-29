#pragma once

#include <concepts>
#include <cstdint>
#include <stdexcept>

/// @file
/// @brief Finite field (Z/pZ) arithmetic parametrized over the modulus type.
namespace numerica_core {

/// @brief Constrains FiniteField's modulus/value representation to the
/// native unsigned integer widths this type supports.
template <typename T>
concept FiniteFieldValueType = std::same_as<T, std::uint32_t> || std::same_as<T, std::uint64_t>;

namespace detail {

/// @brief 64-bit-modulus multiply-mod, using a 128-bit intermediate product.
/// Falls back to a portable schoolbook 64x64->128 split when the compiler
/// doesn't provide a native 128-bit integer (e.g. MSVC).
inline std::uint64_t mul_mod_u64(std::uint64_t a, std::uint64_t b, std::uint64_t modulus) {
#if defined(__SIZEOF_INT128__)
  const unsigned __int128 product = static_cast<unsigned __int128>(a) * b;
  return static_cast<std::uint64_t>(product % modulus);
#else
  // Portable fallback: Russian-peasant multiplication mod modulus, avoiding
  // any 128-bit intermediate at the cost of up to 64 iterations.
  std::uint64_t result = 0;
  a %= modulus;
  while (b > 0) {
    if (b & 1U) {
      result = (result + a) % modulus;
    }
    a = (a + a) % modulus;
    b >>= 1U;
  }
  return result;
#endif
}

} // namespace detail

/// @brief An element of the finite field Z/pZ for a prime modulus p, stored
/// as a canonical residue in [0, p).
///
/// The modulus is carried per-value (not hoisted into a separate ring type
/// yet - that split happens in algebra_core's FiniteFieldRing<T>, M2).
/// Multiply-mod uses hardware division directly for u32 (a 64-bit
/// intermediate divided by a 32-bit modulus is already a single hardware
/// instruction) and a 128-bit-intermediate reduction for u64.
template <FiniteFieldValueType T> class FiniteField {
public:
  using ValueType = T;

  /// @brief Constructs the residue of @p value modulo @p modulus.
  /// @param modulus The field modulus. Must be > 1; primality is the
  ///        caller's responsibility (not checked, to keep construction O(1)).
  FiniteField(T modulus, T value) noexcept : modulus_(modulus), value_(value % modulus) {}

  [[nodiscard]] T value() const noexcept { return value_; }
  [[nodiscard]] T modulus() const noexcept { return modulus_; }

  [[nodiscard]] FiniteField operator+(const FiniteField& other) const {
    T sum = value_ + other.value_;
    if (sum >= modulus_ || sum < value_) { // sum < value_ catches T overflow wraparound
      sum -= modulus_;
    }
    return FiniteField(modulus_, sum);
  }

  [[nodiscard]] FiniteField operator-(const FiniteField& other) const {
    T diff = value_ - other.value_;
    if (value_ < other.value_) {
      diff += modulus_;
    }
    return FiniteField(modulus_, diff);
  }

  [[nodiscard]] FiniteField operator*(const FiniteField& other) const {
    if constexpr (std::same_as<T, std::uint32_t>) {
      const std::uint64_t product = static_cast<std::uint64_t>(value_) * other.value_;
      return FiniteField(modulus_, static_cast<T>(product % modulus_));
    } else {
      return FiniteField(modulus_, detail::mul_mod_u64(value_, other.value_, modulus_));
    }
  }

  [[nodiscard]] FiniteField operator-() const {
    return FiniteField(modulus_, value_ == 0 ? 0 : modulus_ - value_);
  }

  /// @brief Multiplicative inverse via Fermat's little theorem
  /// (value^(modulus-2) mod modulus). Requires the modulus to be prime;
  /// avoids the signed-overflow trap an extended-Euclid implementation
  /// would hit for moduli near the top of the u64 range.
  /// @throws std::domain_error if this value is zero (never invertible).
  [[nodiscard]] FiniteField inv() const {
    if (value_ == 0) {
      throw std::domain_error("FiniteField: zero has no multiplicative inverse");
    }
    FiniteField base = *this;
    FiniteField result(modulus_, 1);
    T exponent = modulus_ - 2;
    while (exponent > 0) {
      if (exponent & 1U) {
        result = result * base;
      }
      base = base * base;
      exponent >>= 1U;
    }
    return result;
  }

  [[nodiscard]] bool operator==(const FiniteField& other) const {
    return modulus_ == other.modulus_ && value_ == other.value_;
  }

private:
  T modulus_;
  T value_;
};

} // namespace numerica_core
