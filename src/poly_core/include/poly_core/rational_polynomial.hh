#pragma once

#include <utility>
#include <vector>

#include <algebra_core/rings.hh>
#include <poly_core/polynomial.hh>
#include <symbol_table/symbol.hh>

/// @file
/// @brief `RationalPolynomial` (M4-T4): a ratio of two
/// `MultivariatePolynomial<algebra_core::RationalField>`s, automatically
/// reduced to lowest terms via poly_core::rational_multivariate_gcd
/// (M4-T2) - reusing the project's own reconstruction-based GCD rather than
/// introducing a second GCD path. `RationalPolynomialField` wraps it as a
/// ring-struct satisfying algebra_core::Field (and, trivially,
/// EuclideanDomain - every field is one), per algebra_core's established
/// pattern (see algebra_core/rings.hh's `FiniteFieldRing<T>`).
namespace poly_core {

using RationalPoly = MultivariatePolynomial<algebra_core::RationalField>;

/// @brief A rational function `numerator() / denominator()` over ℚ, always
/// kept in lowest terms: `gcd(numerator(), denominator()) == 1` and
/// `denominator()` is monic (leading coefficient 1). Two `RationalPolynomial`s
/// are equal iff their canonical (reduced) forms are structurally equal -
/// meaningful because reduction is deterministic (poly_core's GCD pipeline
/// uses no randomness, unlike factorization.hh's Cantor-Zassenhaus step).
class RationalPolynomial {
public:
  /// @brief Builds and reduces @p numerator / @p denominator to lowest
  /// terms.
  /// @throws std::invalid_argument if @p numerator and @p denominator don't
  ///         share the same variable list.
  /// @throws std::domain_error if @p denominator is the zero polynomial.
  RationalPolynomial(const algebra_core::RationalField& field,
                     RationalPoly numerator,
                     RationalPoly denominator);

  /// @brief Builds `numerator / 1`.
  [[nodiscard]] static RationalPolynomial from_polynomial(const algebra_core::RationalField& field,
                                                          RationalPoly numerator);

  [[nodiscard]] const RationalPoly& numerator() const noexcept { return numerator_; }
  [[nodiscard]] const RationalPoly& denominator() const noexcept { return denominator_; }
  [[nodiscard]] const std::vector<symbol_table::Symbol>& variables() const noexcept {
    return numerator_.variables();
  }

  [[nodiscard]] bool is_zero() const noexcept { return numerator_.is_zero(); }

  [[nodiscard]] bool operator==(const RationalPolynomial& other) const;

  [[nodiscard]] RationalPolynomial operator+(const RationalPolynomial& other) const;
  [[nodiscard]] RationalPolynomial operator-(const RationalPolynomial& other) const;
  [[nodiscard]] RationalPolynomial operator*(const RationalPolynomial& other) const;

  /// @throws std::domain_error if @p other is zero.
  [[nodiscard]] RationalPolynomial operator/(const RationalPolynomial& other) const;

  [[nodiscard]] RationalPolynomial operator-() const;

private:
  const algebra_core::RationalField* field_;
  RationalPoly numerator_;
  RationalPoly denominator_;

  // Used internally where the mathematics guarantees the invariant already
  // holds (denominator == 1, or negating the numerator of an already-
  // reduced value) - skips a redundant GCD computation rather than being a
  // trust-me shortcut.
  struct AlreadyReduced {};
  RationalPolynomial(const algebra_core::RationalField& field,
                     RationalPoly numerator,
                     RationalPoly denominator,
                     AlreadyReduced) noexcept
    : field_(&field), numerator_(std::move(numerator)), denominator_(std::move(denominator)) {}
};

/// @brief Ring-struct wrapper making `RationalPolynomial` satisfy
/// `algebra_core::Field` (and, trivially, `EuclideanDomain`).
class RationalPolynomialField {
public:
  using Element = RationalPolynomial;

  RationalPolynomialField(const algebra_core::RationalField& field,
                          std::vector<symbol_table::Symbol> variables);

  [[nodiscard]] Element zero() const;
  [[nodiscard]] Element one() const;
  [[nodiscard]] Element add(const Element& a, const Element& b) const { return a + b; }
  [[nodiscard]] Element sub(const Element& a, const Element& b) const { return a - b; }
  [[nodiscard]] Element mul(const Element& a, const Element& b) const { return a * b; }
  [[nodiscard]] Element neg(const Element& a) const { return -a; }

  /// @throws std::domain_error if @p a is zero.
  [[nodiscard]] Element inv(const Element& a) const;

  /// @brief Every field is trivially a Euclidean domain: division is
  /// always exact, so the remainder is always zero - same rationale as
  /// `FiniteFieldRing<T>::div_rem`.
  /// @throws std::domain_error if @p b is zero.
  [[nodiscard]] std::pair<Element, Element> div_rem(const Element& a, const Element& b) const {
    return {mul(a, inv(b)), zero()};
  }

private:
  const algebra_core::RationalField* field_;
  std::vector<symbol_table::Symbol> variables_;
};

} // namespace poly_core
