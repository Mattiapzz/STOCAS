#include <algorithm>
#include <numeric>
#include <stdexcept>

#include <poly_core/monomial.hh>

namespace poly_core {

Exponent Monomial::total_degree() const noexcept {
  return std::accumulate(exponents_.begin(), exponents_.end(), Exponent{0});
}

bool Monomial::is_one() const noexcept {
  return std::all_of(exponents_.begin(), exponents_.end(), [](Exponent e) { return e == 0; });
}

std::strong_ordering Monomial::operator<=>(const Monomial& other) const {
  if (exponents_.size() != other.exponents_.size()) {
    throw std::invalid_argument("Monomial::operator<=>: mismatched num_vars()");
  }
  for (std::size_t i = 0; i < exponents_.size(); ++i) {
    if (auto cmp = exponents_[i] <=> other.exponents_[i]; cmp != std::strong_ordering::equal) {
      return cmp;
    }
  }
  return std::strong_ordering::equal;
}

Monomial Monomial::operator*(const Monomial& other) const {
  if (exponents_.size() != other.exponents_.size()) {
    throw std::invalid_argument("Monomial::operator*: mismatched num_vars()");
  }
  std::vector<Exponent> result(exponents_.size());
  for (std::size_t i = 0; i < exponents_.size(); ++i) {
    result[i] = exponents_[i] + other.exponents_[i];
  }
  return Monomial(std::move(result));
}

} // namespace poly_core
