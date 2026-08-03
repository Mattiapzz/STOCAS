#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include <algebra_core/concepts.hh>
#include <poly_core/monomial.hh>
#include <symbol_table/symbol.hh>

/// @file
/// @brief `MultivariatePolynomial<R>` (M4-T1): a sparse multivariate
/// polynomial generic over any `algebra_core::Ring`, following the same
/// "operations live on a ring-struct type" discipline as algebra_core
/// (see algebra_core/concepts.hh's coherence-risk note) - a polynomial
/// never assumes anything about its coefficient type beyond what the Ring
/// concept requires.
///
/// Header-only: every member is a function template (parameterized by the
/// ring type `R`), so there is no non-template translation unit to place
/// in a .cc - same pattern as algebra_core's generic_algorithms.hh.
namespace poly_core {

/// @brief A sparse multivariate polynomial over `R::Element`, with an
/// explicit, fixed variable list shared by every term's `Monomial`.
///
/// Terms are kept sorted in descending `Monomial` order (so `terms()[0]`,
/// when non-empty, is always the leading term) with no zero-coefficient or
/// duplicate-monomial entries - every public constructor/operator
/// maintains this invariant.
///
/// Lifetime: holds a non-owning pointer to the `R` ring instance used to
/// build it (rings in this codebase are lightweight, typically
/// function-local or static value types passed by `const&` - see
/// algebra_core/generic_algorithms.hh). The ring must outlive the
/// polynomial.
template <typename R>
  requires algebra_core::Ring<R>
class MultivariatePolynomial {
public:
  using Coeff = typename R::Element;
  using Term = std::pair<Monomial, Coeff>;

  /// @brief Builds the zero polynomial over @p variables.
  MultivariatePolynomial(const R& ring, std::vector<symbol_table::Symbol> variables)
    : ring_(&ring), variables_(std::move(variables)) {}

  /// @brief Builds a single-term polynomial `coefficient * monomial`.
  /// Omits the term entirely if @p coefficient is zero.
  /// @throws std::invalid_argument if @p monomial.num_vars() != variables.size().
  [[nodiscard]] static MultivariatePolynomial from_term(const R& ring,
                                                        std::vector<symbol_table::Symbol> variables,
                                                        Monomial monomial,
                                                        Coeff coefficient) {
    if (monomial.num_vars() != variables.size()) {
      throw std::invalid_argument(
          "MultivariatePolynomial::from_term: monomial/variables size mismatch");
    }
    MultivariatePolynomial result(ring, std::move(variables));
    if (!(coefficient == ring.zero())) {
      result.terms_.emplace_back(std::move(monomial), std::move(coefficient));
    }
    return result;
  }

  /// @brief Builds a constant polynomial (monomial 1, i.e. all exponents 0).
  [[nodiscard]] static MultivariatePolynomial
  constant(const R& ring, std::vector<symbol_table::Symbol> variables, Coeff value) {
    Monomial one(std::vector<Exponent>(variables.size(), 0));
    return from_term(ring, std::move(variables), std::move(one), std::move(value));
  }

  [[nodiscard]] const std::vector<symbol_table::Symbol>& variables() const noexcept {
    return variables_;
  }

  [[nodiscard]] const std::vector<Term>& terms() const noexcept { return terms_; }

  [[nodiscard]] std::size_t num_terms() const noexcept { return terms_.size(); }

  [[nodiscard]] bool is_zero() const noexcept { return terms_.empty(); }

  /// @return The leading (highest-order) term.
  /// @throws std::logic_error if is_zero().
  [[nodiscard]] const Term& leading_term() const {
    if (is_zero()) {
      throw std::logic_error("MultivariatePolynomial::leading_term: polynomial is zero");
    }
    return terms_.front();
  }

  [[nodiscard]] bool operator==(const MultivariatePolynomial& other) const {
    return variables_ == other.variables_ && terms_ == other.terms_;
  }

  [[nodiscard]] MultivariatePolynomial operator+(const MultivariatePolynomial& other) const {
    require_same_variables(other);
    MultivariatePolynomial result(*ring_, variables_);
    merge_terms(other, [this](const Coeff& a, const Coeff& b) { return ring_->add(a, b); }, result);
    return result;
  }

  [[nodiscard]] MultivariatePolynomial operator-(const MultivariatePolynomial& other) const {
    require_same_variables(other);
    MultivariatePolynomial result(*ring_, variables_);
    merge_terms(other, [this](const Coeff& a, const Coeff& b) { return ring_->sub(a, b); }, result);
    return result;
  }

  [[nodiscard]] MultivariatePolynomial operator-() const {
    MultivariatePolynomial result(*ring_, variables_);
    result.terms_.reserve(terms_.size());
    for (const Term& term : terms_) {
      result.terms_.emplace_back(term.first, ring_->neg(term.second));
    }
    return result;
  }

  [[nodiscard]] MultivariatePolynomial operator*(const MultivariatePolynomial& other) const {
    require_same_variables(other);
    MultivariatePolynomial result(*ring_, variables_);
    if (is_zero() || other.is_zero()) {
      return result;
    }
    // Accumulate raw products, then combine like monomials in one sort pass
    // - simplest correct approach; not optimized for large sparse inputs.
    std::vector<Term> raw;
    raw.reserve(terms_.size() * other.terms_.size());
    for (const Term& a : terms_) {
      for (const Term& b : other.terms_) {
        raw.emplace_back(a.first * b.first, ring_->mul(a.second, b.second));
      }
    }
    std::sort(raw.begin(), raw.end(), [](const Term& lhs, const Term& rhs) {
      return lhs.first > rhs.first;
    });
    for (Term& term : raw) {
      if (!result.terms_.empty() && result.terms_.back().first == term.first) {
        result.terms_.back().second = ring_->add(result.terms_.back().second, term.second);
      } else {
        result.terms_.push_back(std::move(term));
      }
    }
    std::erase_if(result.terms_, [this](const Term& term) { return term.second == ring_->zero(); });
    return result;
  }

private:
  const R* ring_;
  std::vector<symbol_table::Symbol> variables_;
  std::vector<Term> terms_; // descending Monomial order, no zero/duplicate entries

  void require_same_variables(const MultivariatePolynomial& other) const {
    if (variables_.size() != other.variables_.size()) {
      throw std::invalid_argument("MultivariatePolynomial: mismatched variable lists");
    }
    for (std::size_t i = 0; i < variables_.size(); ++i) {
      if (!(variables_[i] == other.variables_[i])) {
        throw std::invalid_argument("MultivariatePolynomial: mismatched variable lists");
      }
    }
  }

  template <typename CombineOp>
  void merge_terms(const MultivariatePolynomial& other,
                   CombineOp combine,
                   MultivariatePolynomial& result) const {
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < terms_.size() && j < other.terms_.size()) {
      if (terms_[i].first == other.terms_[j].first) {
        Coeff combined = combine(terms_[i].second, other.terms_[j].second);
        if (!(combined == ring_->zero())) {
          result.terms_.emplace_back(terms_[i].first, std::move(combined));
        }
        ++i;
        ++j;
      } else if (terms_[i].first > other.terms_[j].first) {
        result.terms_.emplace_back(terms_[i]);
        ++i;
      } else {
        Coeff combined = combine(ring_->zero(), other.terms_[j].second);
        result.terms_.emplace_back(other.terms_[j].first, std::move(combined));
        ++j;
      }
    }
    for (; i < terms_.size(); ++i) {
      result.terms_.emplace_back(terms_[i]);
    }
    for (; j < other.terms_.size(); ++j) {
      Coeff combined = combine(ring_->zero(), other.terms_[j].second);
      result.terms_.emplace_back(other.terms_[j].first, std::move(combined));
    }
  }
};

} // namespace poly_core
