#include <stdexcept>

#include <numerica_core/rational.hh>

namespace numerica_core {

namespace {

BigInt abs_value(const BigInt& value) {
  return value < BigInt(0) ? -value : value;
}

// Integer square root via Newton's method, using only BigInt's public
// arithmetic (no GMP access needed here - BigInt already hides that).
BigInt isqrt(const BigInt& value) {
  if (value < BigInt(0)) {
    throw std::domain_error("isqrt: negative argument");
  }
  if (value == BigInt(0)) {
    return BigInt(0);
  }
  BigInt x = value;
  BigInt y = (x + BigInt(1)) / BigInt(2);
  while (y < x) {
    x = y;
    y = (x + value / x) / BigInt(2);
  }
  return x;
}

} // namespace

Rational::Rational() noexcept : numerator_(0), denominator_(1) {}

Rational::Rational(int64_t value) noexcept : numerator_(value), denominator_(1) {}

Rational::Rational(BigInt value) : numerator_(std::move(value)), denominator_(1) {}

Rational::Rational(BigInt numerator, BigInt denominator)
  : numerator_(std::move(numerator)), denominator_(std::move(denominator)) {
  if (denominator_ == BigInt(0)) {
    throw std::domain_error("Rational: zero denominator");
  }
  reduce();
}

void Rational::reduce() {
  if (denominator_ < BigInt(0)) {
    numerator_ = -numerator_;
    denominator_ = -denominator_;
  }
  if (numerator_ == BigInt(0)) {
    denominator_ = BigInt(1);
    return;
  }
  const BigInt g = BigInt::gcd(abs_value(numerator_), denominator_);
  if (!(g == BigInt(1))) {
    numerator_ = numerator_ / g;
    denominator_ = denominator_ / g;
  }
}

const BigInt& Rational::numerator() const noexcept {
  return numerator_;
}

const BigInt& Rational::denominator() const noexcept {
  return denominator_;
}

Rational Rational::operator+(const Rational& other) const {
  return Rational(numerator_ * other.denominator_ + other.numerator_ * denominator_,
                  denominator_ * other.denominator_);
}

Rational Rational::operator-(const Rational& other) const {
  return Rational(numerator_ * other.denominator_ - other.numerator_ * denominator_,
                  denominator_ * other.denominator_);
}

Rational Rational::operator*(const Rational& other) const {
  return Rational(numerator_ * other.numerator_, denominator_ * other.denominator_);
}

Rational Rational::operator/(const Rational& other) const {
  if (other.numerator_ == BigInt(0)) {
    throw std::domain_error("Rational: division by zero");
  }
  return Rational(numerator_ * other.denominator_, denominator_ * other.numerator_);
}

Rational Rational::operator-() const {
  return Rational(-numerator_, denominator_);
}

Rational Rational::inv() const {
  if (numerator_ == BigInt(0)) {
    throw std::domain_error("Rational: inverse of zero");
  }
  return Rational(denominator_, numerator_);
}

bool Rational::operator==(const Rational& other) const {
  return numerator_ == other.numerator_ && denominator_ == other.denominator_;
}

std::strong_ordering Rational::operator<=>(const Rational& other) const {
  return (numerator_ * other.denominator_) <=> (other.numerator_ * denominator_);
}

double Rational::to_double() const {
  return numerator_.to_double() / denominator_.to_double();
}

std::string Rational::to_string() const {
  if (denominator_ == BigInt(1)) {
    return numerator_.to_string();
  }
  return numerator_.to_string() + "/" + denominator_.to_string();
}

std::optional<Rational>
Rational::reconstruct(const BigInt& residue, const BigInt& modulus, std::optional<BigInt> bound) {
  if (!(modulus > BigInt(0))) {
    throw std::domain_error("Rational::reconstruct: modulus must be positive");
  }
  const BigInt actual_bound = bound.has_value() ? *bound : isqrt(modulus / BigInt(2));

  // Extended-Euclid-style continued fraction expansion of modulus/residue,
  // stopping as soon as the remainder drops at or below actual_bound - the
  // standard rational reconstruction algorithm.
  BigInt r_prev = modulus;
  BigInt r_curr = ((residue % modulus) + modulus) % modulus;
  BigInt t_prev(0);
  BigInt t_curr(1);

  while (r_curr > actual_bound) {
    if (r_curr == BigInt(0)) {
      return std::nullopt;
    }
    const BigInt quotient = r_prev / r_curr;
    const BigInt r_next = r_prev - quotient * r_curr;
    const BigInt t_next = t_prev - quotient * t_curr;
    r_prev = r_curr;
    r_curr = r_next;
    t_prev = t_curr;
    t_curr = t_next;
  }

  const BigInt& denominator = t_curr;
  const BigInt abs_denominator = abs_value(denominator);
  if (abs_denominator == BigInt(0) || abs_denominator > actual_bound) {
    return std::nullopt;
  }
  if (BigInt::gcd(abs_value(r_curr), abs_denominator) != BigInt(1)) {
    return std::nullopt;
  }

  return Rational(r_curr, denominator);
}

} // namespace numerica_core
