#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <vector>

/// @file
/// @brief `Monomial`: a fixed-length exponent vector, aligned positionally
/// to a `MultivariatePolynomial<R>`'s variable list (polynomial.hh).
namespace poly_core {

/// @brief A single variable's exponent within a monomial.
using Exponent = std::uint32_t;

/// @brief A monomial `x_0^e_0 * x_1^e_1 * ... * x_{n-1}^e_{n-1}`, stored as
/// a dense exponent vector. Carries no variable names itself - position `i`
/// always refers to the `i`-th variable of whichever
/// `MultivariatePolynomial<R>`'s variable list it is used with.
class Monomial {
public:
  Monomial() = default;

  /// @brief Constructs a monomial from an explicit exponent vector.
  /// @param exponents Exponent of each variable, in variable-list order.
  explicit Monomial(std::vector<Exponent> exponents) : exponents_(std::move(exponents)) {}

  /// @return The number of variables this monomial is defined over.
  [[nodiscard]] std::size_t num_vars() const noexcept { return exponents_.size(); }

  /// @throws std::out_of_range if @p index >= num_vars().
  [[nodiscard]] Exponent exponent(std::size_t index) const { return exponents_.at(index); }

  /// @return The sum of all exponents (total degree).
  [[nodiscard]] Exponent total_degree() const noexcept;

  /// @return true iff every exponent is zero.
  [[nodiscard]] bool is_one() const noexcept;

  [[nodiscard]] bool operator==(const Monomial& other) const = default;

  /// @brief Lexicographic order over exponent vectors (first variable is
  /// the most significant), the ordering `MultivariatePolynomial<R>` sorts
  /// its terms by (descending, so leading term is index 0).
  /// @throws std::invalid_argument if @p other has a different num_vars().
  [[nodiscard]] std::strong_ordering operator<=>(const Monomial& other) const;

  /// @brief Elementwise-sums exponents (monomial multiplication).
  /// @throws std::invalid_argument if @p other has a different num_vars().
  [[nodiscard]] Monomial operator*(const Monomial& other) const;

private:
  std::vector<Exponent> exponents_;
};

} // namespace poly_core
