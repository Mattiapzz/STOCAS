#include <stdexcept>
#include <utility>

#include <numerica_core/rational.hh>
#include <poly_core/multivariate_division.hh>
#include <poly_core/multivariate_gcd.hh>
#include <poly_core/rational_polynomial.hh>

namespace poly_core {

RationalPolynomial::RationalPolynomial(const algebra_core::RationalField& field,
                                       RationalPoly numerator,
                                       RationalPoly denominator)
  : field_(&field), numerator_(std::move(numerator)), denominator_(std::move(denominator)) {
  if (denominator_.is_zero()) {
    throw std::domain_error("RationalPolynomial: zero denominator");
  }

  RationalPoly gcd = rational_multivariate_gcd(field, numerator_, denominator_);
  numerator_ = multivariate_div_rem(field, numerator_, gcd).first;
  denominator_ = multivariate_div_rem(field, denominator_, gcd).first;

  numerica_core::Rational scale = denominator_.leading_term().second.inv();
  RationalPoly scale_poly = RationalPoly::constant(field, numerator_.variables(), scale);
  numerator_ = numerator_ * scale_poly;
  denominator_ = denominator_ * scale_poly;
}

RationalPolynomial RationalPolynomial::from_polynomial(const algebra_core::RationalField& field,
                                                       RationalPoly numerator) {
  RationalPoly denominator =
      RationalPoly::constant(field, numerator.variables(), numerica_core::Rational(1));
  return RationalPolynomial(field, std::move(numerator), std::move(denominator), AlreadyReduced{});
}

bool RationalPolynomial::operator==(const RationalPolynomial& other) const {
  return numerator_ == other.numerator_ && denominator_ == other.denominator_;
}

RationalPolynomial RationalPolynomial::operator+(const RationalPolynomial& other) const {
  RationalPoly num = numerator_ * other.denominator_ + other.numerator_ * denominator_;
  RationalPoly den = denominator_ * other.denominator_;
  return RationalPolynomial(*field_, std::move(num), std::move(den));
}

RationalPolynomial RationalPolynomial::operator-(const RationalPolynomial& other) const {
  RationalPoly num = numerator_ * other.denominator_ - other.numerator_ * denominator_;
  RationalPoly den = denominator_ * other.denominator_;
  return RationalPolynomial(*field_, std::move(num), std::move(den));
}

RationalPolynomial RationalPolynomial::operator*(const RationalPolynomial& other) const {
  RationalPoly num = numerator_ * other.numerator_;
  RationalPoly den = denominator_ * other.denominator_;
  return RationalPolynomial(*field_, std::move(num), std::move(den));
}

RationalPolynomial RationalPolynomial::operator/(const RationalPolynomial& other) const {
  RationalPoly num = numerator_ * other.denominator_;
  RationalPoly den = denominator_ * other.numerator_;
  return RationalPolynomial(*field_, std::move(num), std::move(den));
}

RationalPolynomial RationalPolynomial::operator-() const {
  RationalPoly negated_numerator = -numerator_;
  return RationalPolynomial(*field_, std::move(negated_numerator), denominator_, AlreadyReduced{});
}

RationalPolynomialField::RationalPolynomialField(const algebra_core::RationalField& field,
                                                 std::vector<symbol_table::Symbol> variables)
  : field_(&field), variables_(std::move(variables)) {}

RationalPolynomial RationalPolynomialField::zero() const {
  return RationalPolynomial::from_polynomial(*field_, RationalPoly(*field_, variables_));
}

RationalPolynomial RationalPolynomialField::one() const {
  return RationalPolynomial::from_polynomial(
      *field_, RationalPoly::constant(*field_, variables_, numerica_core::Rational(1)));
}

RationalPolynomial RationalPolynomialField::inv(const RationalPolynomial& a) const {
  if (a.is_zero()) {
    throw std::domain_error("RationalPolynomialField::inv: zero has no multiplicative inverse");
  }
  return RationalPolynomial(*field_, a.denominator(), a.numerator());
}

} // namespace poly_core
