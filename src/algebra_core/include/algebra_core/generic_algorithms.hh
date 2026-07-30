#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include <algebra_core/concepts.hh>

/// @file
/// @brief Generic algorithms written once against algebra_core's concepts -
/// gcd/extended_gcd (EuclideanDomain) and row reduction (Field). Header-only:
/// each is a function template constrained by a concept, so there is no
/// non-template translation unit to put in a .cc (same pattern as
/// numerica_core's finite_field.hh/dual.hh/hyper_dual.hh).
namespace algebra_core {

/// @brief Bezout coefficients and gcd: `x*a + y*b == gcd`.
template <typename R> struct ExtendedGcdResult {
  typename R::Element gcd;
  typename R::Element x;
  typename R::Element y;
};

/// @brief Euclidean-algorithm gcd, generic over any EuclideanDomain.
///
/// For a field (every EuclideanDomain division has zero remainder), this
/// returns a nonzero `b` unchanged when both operands are nonzero - the
/// correct "gcd up to units" result for a field, where every nonzero
/// element is an associate of every other. Callers that need a specific
/// canonical form (e.g. non-negative for IntegerRing) should normalize the
/// result themselves; this algorithm only guarantees the domain-agnostic
/// property that the result divides both @p a and @p b.
template <typename R>
  requires EuclideanDomain<R>
[[nodiscard]] typename R::Element gcd(const R& ring, typename R::Element a, typename R::Element b) {
  while (!(b == ring.zero())) {
    const typename R::Element remainder = ring.div_rem(a, b).second;
    a = b;
    b = remainder;
  }
  return a;
}

/// @brief Extended Euclidean algorithm, generic over any EuclideanDomain.
/// @return `{gcd, x, y}` such that `ring.add(ring.mul(x, a), ring.mul(y, b))
///         == gcd`.
template <typename R>
  requires EuclideanDomain<R>
[[nodiscard]] ExtendedGcdResult<R>
extended_gcd(const R& ring, typename R::Element a, typename R::Element b) {
  typename R::Element old_r = a;
  typename R::Element r = b;
  typename R::Element old_s = ring.one();
  typename R::Element s = ring.zero();
  typename R::Element old_t = ring.zero();
  typename R::Element t = ring.one();

  while (!(r == ring.zero())) {
    auto [quotient, remainder] = ring.div_rem(old_r, r);
    old_r = std::exchange(r, remainder);
    old_s = std::exchange(s, ring.sub(old_s, ring.mul(quotient, s)));
    old_t = std::exchange(t, ring.sub(old_t, ring.mul(quotient, t)));
  }
  return {old_r, old_s, old_t};
}

/// @brief In-place Gauss-Jordan row reduction to reduced row echelon form,
/// generic over any Field. Prerequisite primitive for tensor_core's
/// `Matrix<R>` (not yet implemented - this operates directly on a plain
/// row-major `std::vector<std::vector<Element>>` so the algorithm can be
/// written and tested now, ahead of the Matrix type).
template <typename R>
  requires Field<R>
void row_reduce(const R& field, std::vector<std::vector<typename R::Element>>& matrix) {
  const std::size_t rows = matrix.size();
  if (rows == 0) {
    return;
  }
  const std::size_t cols = matrix[0].size();

  std::size_t pivot_row = 0;
  for (std::size_t col = 0; col < cols && pivot_row < rows; ++col) {
    std::size_t selected = pivot_row;
    while (selected < rows && matrix[selected][col] == field.zero()) {
      ++selected;
    }
    if (selected == rows) {
      continue; // No pivot available in this column; move to the next.
    }
    std::swap(matrix[selected], matrix[pivot_row]);

    const typename R::Element inv_pivot = field.inv(matrix[pivot_row][col]);
    for (std::size_t c = 0; c < cols; ++c) {
      matrix[pivot_row][c] = field.mul(matrix[pivot_row][c], inv_pivot);
    }

    for (std::size_t r = 0; r < rows; ++r) {
      if (r == pivot_row || matrix[r][col] == field.zero()) {
        continue;
      }
      const typename R::Element factor = matrix[r][col];
      for (std::size_t c = 0; c < cols; ++c) {
        matrix[r][c] = field.sub(matrix[r][c], field.mul(factor, matrix[pivot_row][c]));
      }
    }
    ++pivot_row;
  }
}

} // namespace algebra_core
