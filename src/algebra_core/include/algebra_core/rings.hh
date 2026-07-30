#pragma once

#include <utility>

#include <numerica_core/big_int.hh>
#include <numerica_core/finite_field.hh>
#include <numerica_core/rational.hh>

/// @file
/// @brief Ring-struct types wrapping numerica_core's exact number types,
/// satisfying algebra_core's concepts (concepts.hh).
namespace algebra_core {

/// @brief The ring of integers (numerica_core::BigInt), satisfying
/// EuclideanDomain via truncating division/remainder.
class IntegerRing {
public:
  using Element = numerica_core::BigInt;

  [[nodiscard]] Element zero() const;
  [[nodiscard]] Element one() const;
  [[nodiscard]] Element add(const Element& a, const Element& b) const;
  [[nodiscard]] Element sub(const Element& a, const Element& b) const;
  [[nodiscard]] Element mul(const Element& a, const Element& b) const;
  [[nodiscard]] Element neg(const Element& a) const;

  /// @brief Truncating division: `{a / b, a % b}`.
  /// @throws std::domain_error if @p b is zero.
  [[nodiscard]] std::pair<Element, Element> div_rem(const Element& a, const Element& b) const;
};

/// @brief The field of rationals (numerica_core::Rational).
class RationalField {
public:
  using Element = numerica_core::Rational;

  [[nodiscard]] Element zero() const;
  [[nodiscard]] Element one() const;
  [[nodiscard]] Element add(const Element& a, const Element& b) const;
  [[nodiscard]] Element sub(const Element& a, const Element& b) const;
  [[nodiscard]] Element mul(const Element& a, const Element& b) const;
  [[nodiscard]] Element neg(const Element& a) const;

  /// @brief Multiplicative inverse of @p a.
  /// @throws std::domain_error if @p a is zero.
  [[nodiscard]] Element inv(const Element& a) const;
};

/// @brief The finite field Z/pZ (numerica_core::FiniteField<T>), holding the
/// modulus once at the ring level rather than per-element.
///
/// Template (not declared/defined split across .hh/.cc) since it must be
/// instantiable for any FiniteFieldValueType T, matching the header-only
/// pattern already used by numerica_core::FiniteField itself.
template <numerica_core::FiniteFieldValueType T> class FiniteFieldRing {
public:
  using Element = numerica_core::FiniteField<T>;

  /// @param modulus The field modulus. Must be prime; not checked here (see
  ///        numerica_core::FiniteField's constructor note).
  explicit FiniteFieldRing(T modulus) : modulus_(modulus) {}

  [[nodiscard]] Element zero() const { return Element(modulus_, 0); }
  [[nodiscard]] Element one() const { return Element(modulus_, 1); }
  [[nodiscard]] Element add(const Element& a, const Element& b) const { return a + b; }
  [[nodiscard]] Element sub(const Element& a, const Element& b) const { return a - b; }
  [[nodiscard]] Element mul(const Element& a, const Element& b) const { return a * b; }
  [[nodiscard]] Element neg(const Element& a) const { return -a; }

  /// @throws std::domain_error if @p a is zero.
  [[nodiscard]] Element inv(const Element& a) const { return a.inv(); }

  /// @brief Every field is trivially a Euclidean domain: division is always
  /// exact, so the remainder is always zero. Lets FiniteFieldRing<T> satisfy
  /// EuclideanDomain too (needed by generic_algorithms.hh's gcd/extended_gcd,
  /// which reduce to a single step - "gcd" is any nonzero associate - for
  /// any field).
  /// @throws std::domain_error if @p b is zero.
  [[nodiscard]] std::pair<Element, Element> div_rem(const Element& a, const Element& b) const {
    return {mul(a, inv(b)), zero()};
  }

private:
  T modulus_;
};

} // namespace algebra_core
