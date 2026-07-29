#include <cstdlib>
#include <limits>
#include <stdexcept>

#include <gmp.h>
#include <numerica_core/big_int.hh>

namespace numerica_core {

struct BigInt::Impl {
  mpz_t value;

  Impl() { mpz_init(value); }
  explicit Impl(int64_t v) {
    mpz_init(value);
    if (v >= 0) {
      mpz_set_ui(value, static_cast<unsigned long>(v));
    } else {
      mpz_set_si(value, v);
    }
  }
  Impl(const Impl& other) {
    mpz_init(value);
    mpz_set(value, other.value);
  }
  ~Impl() { mpz_clear(value); }
};

namespace {

// Returns the value if it fits in int64_t, otherwise nullopt-like sentinel
// via the bool return.
bool fits_in_int64(const mpz_t value, int64_t* out) {
  if (mpz_fits_slong_p(value) == 0) {
    return false;
  }
  const long as_long = mpz_get_si(value);
  *out = static_cast<int64_t>(as_long);
  return true;
}

} // namespace

BigInt::BigInt() noexcept = default;

BigInt::BigInt(int64_t value) noexcept : small_value_(value) {}

BigInt::BigInt(const std::string& decimal_string) {
  impl_ = std::make_unique<Impl>();
  if (mpz_set_str(impl_->value, decimal_string.c_str(), 10) != 0) {
    throw std::invalid_argument("BigInt: invalid base-10 string '" + decimal_string + "'");
  }
  is_small_ = false;
  int64_t small = 0;
  if (fits_in_int64(impl_->value, &small)) {
    is_small_ = true;
    small_value_ = small;
    impl_.reset();
  }
}

BigInt::BigInt(const BigInt& other) : is_small_(other.is_small_), small_value_(other.small_value_) {
  if (!is_small_) {
    impl_ = std::make_unique<Impl>(*other.impl_);
  }
}

BigInt::BigInt(BigInt&& other) noexcept
  : is_small_(other.is_small_), small_value_(other.small_value_), impl_(std::move(other.impl_)) {
  other.is_small_ = true;
  other.small_value_ = 0;
}

BigInt& BigInt::operator=(const BigInt& other) {
  if (this != &other) {
    is_small_ = other.is_small_;
    small_value_ = other.small_value_;
    impl_ = is_small_ ? nullptr : std::make_unique<Impl>(*other.impl_);
  }
  return *this;
}

BigInt& BigInt::operator=(BigInt&& other) noexcept {
  if (this != &other) {
    is_small_ = other.is_small_;
    small_value_ = other.small_value_;
    impl_ = std::move(other.impl_);
    other.is_small_ = true;
    other.small_value_ = 0;
  }
  return *this;
}

BigInt::~BigInt() = default;

bool BigInt::is_small() const noexcept {
  return is_small_;
}

void BigInt::promote_to_big() {
  if (!is_small_) {
    return;
  }
  impl_ = std::make_unique<Impl>(small_value_);
  is_small_ = false;
}

const BigInt::Impl& BigInt::big() const {
  // Callers must have promoted (or constructed as big) before calling this.
  return *impl_;
}

std::string BigInt::to_string() const {
  if (is_small_) {
    return std::to_string(small_value_);
  }
  char* c_str = mpz_get_str(nullptr, 10, big().value);
  std::string result(c_str);
  // NOLINTNEXTLINE(cppcoreguidelines-no-malloc) - GMP's own allocator contract
  std::free(c_str);
  return result;
}

namespace {

// Detects int64_t add/sub/mul overflow without invoking UB, per operation.
bool add_overflows(int64_t a, int64_t b) {
  return (b > 0 && a > std::numeric_limits<int64_t>::max() - b) ||
         (b < 0 && a < std::numeric_limits<int64_t>::min() - b);
}

bool sub_overflows(int64_t a, int64_t b) {
  return (b < 0 && a > std::numeric_limits<int64_t>::max() + b) ||
         (b > 0 && a < std::numeric_limits<int64_t>::min() + b);
}

bool mul_overflows(int64_t a, int64_t b) {
  if (a == 0 || b == 0) {
    return false;
  }
  const int64_t result = a * b;
  return result / b != a;
}

} // namespace

BigInt BigInt::operator+(const BigInt& other) const {
  if (is_small_ && other.is_small_ && !add_overflows(small_value_, other.small_value_)) {
    return BigInt(small_value_ + other.small_value_);
  }
  BigInt lhs = *this;
  BigInt rhs = other;
  lhs.promote_to_big();
  rhs.promote_to_big();
  BigInt result;
  result.promote_to_big();
  mpz_add(result.impl_->value, lhs.impl_->value, rhs.impl_->value);
  int64_t small = 0;
  if (fits_in_int64(result.impl_->value, &small)) {
    result.is_small_ = true;
    result.small_value_ = small;
    result.impl_.reset();
  }
  return result;
}

BigInt BigInt::operator-(const BigInt& other) const {
  if (is_small_ && other.is_small_ && !sub_overflows(small_value_, other.small_value_)) {
    return BigInt(small_value_ - other.small_value_);
  }
  BigInt lhs = *this;
  BigInt rhs = other;
  lhs.promote_to_big();
  rhs.promote_to_big();
  BigInt result;
  result.promote_to_big();
  mpz_sub(result.impl_->value, lhs.impl_->value, rhs.impl_->value);
  int64_t small = 0;
  if (fits_in_int64(result.impl_->value, &small)) {
    result.is_small_ = true;
    result.small_value_ = small;
    result.impl_.reset();
  }
  return result;
}

BigInt BigInt::operator*(const BigInt& other) const {
  if (is_small_ && other.is_small_ && !mul_overflows(small_value_, other.small_value_)) {
    return BigInt(small_value_ * other.small_value_);
  }
  BigInt lhs = *this;
  BigInt rhs = other;
  lhs.promote_to_big();
  rhs.promote_to_big();
  BigInt result;
  result.promote_to_big();
  mpz_mul(result.impl_->value, lhs.impl_->value, rhs.impl_->value);
  int64_t small = 0;
  if (fits_in_int64(result.impl_->value, &small)) {
    result.is_small_ = true;
    result.small_value_ = small;
    result.impl_.reset();
  }
  return result;
}

BigInt BigInt::operator/(const BigInt& other) const {
  if (other == BigInt(0)) {
    throw std::domain_error("BigInt: division by zero");
  }
  if (is_small_ && other.is_small_ &&
      !(small_value_ == std::numeric_limits<int64_t>::min() && other.small_value_ == -1)) {
    return BigInt(small_value_ / other.small_value_);
  }
  BigInt lhs = *this;
  BigInt rhs = other;
  lhs.promote_to_big();
  rhs.promote_to_big();
  BigInt result;
  result.promote_to_big();
  mpz_tdiv_q(result.impl_->value, lhs.impl_->value, rhs.impl_->value);
  int64_t small = 0;
  if (fits_in_int64(result.impl_->value, &small)) {
    result.is_small_ = true;
    result.small_value_ = small;
    result.impl_.reset();
  }
  return result;
}

BigInt BigInt::operator%(const BigInt& other) const {
  if (other == BigInt(0)) {
    throw std::domain_error("BigInt: modulo by zero");
  }
  if (is_small_ && other.is_small_ &&
      !(small_value_ == std::numeric_limits<int64_t>::min() && other.small_value_ == -1)) {
    return BigInt(small_value_ % other.small_value_);
  }
  BigInt lhs = *this;
  BigInt rhs = other;
  lhs.promote_to_big();
  rhs.promote_to_big();
  BigInt result;
  result.promote_to_big();
  mpz_tdiv_r(result.impl_->value, lhs.impl_->value, rhs.impl_->value);
  int64_t small = 0;
  if (fits_in_int64(result.impl_->value, &small)) {
    result.is_small_ = true;
    result.small_value_ = small;
    result.impl_.reset();
  }
  return result;
}

BigInt BigInt::operator-() const {
  if (is_small_ && small_value_ != std::numeric_limits<int64_t>::min()) {
    return BigInt(-small_value_);
  }
  BigInt operand = *this;
  operand.promote_to_big();
  BigInt result;
  result.promote_to_big();
  mpz_neg(result.impl_->value, operand.impl_->value);
  int64_t small = 0;
  if (fits_in_int64(result.impl_->value, &small)) {
    result.is_small_ = true;
    result.small_value_ = small;
    result.impl_.reset();
  }
  return result;
}

bool BigInt::operator==(const BigInt& other) const {
  return (*this <=> other) == 0;
}

std::strong_ordering BigInt::operator<=>(const BigInt& other) const {
  if (is_small_ && other.is_small_) {
    return small_value_ <=> other.small_value_;
  }
  BigInt lhs = *this;
  BigInt rhs = other;
  lhs.promote_to_big();
  rhs.promote_to_big();
  const int cmp = mpz_cmp(lhs.impl_->value, rhs.impl_->value);
  if (cmp < 0) {
    return std::strong_ordering::less;
  }
  if (cmp > 0) {
    return std::strong_ordering::greater;
  }
  return std::strong_ordering::equal;
}

BigInt BigInt::gcd(const BigInt& a, const BigInt& b) {
  BigInt lhs = a;
  BigInt rhs = b;
  lhs.promote_to_big();
  rhs.promote_to_big();
  BigInt result;
  result.promote_to_big();
  mpz_gcd(result.impl_->value, lhs.impl_->value, rhs.impl_->value);
  int64_t small = 0;
  if (fits_in_int64(result.impl_->value, &small)) {
    result.is_small_ = true;
    result.small_value_ = small;
    result.impl_.reset();
  }
  return result;
}

std::tuple<BigInt, BigInt, BigInt> BigInt::extended_gcd(const BigInt& a, const BigInt& b) {
  BigInt lhs = a;
  BigInt rhs = b;
  lhs.promote_to_big();
  rhs.promote_to_big();
  BigInt g, x, y;
  g.promote_to_big();
  x.promote_to_big();
  y.promote_to_big();
  mpz_gcdext(g.impl_->value, x.impl_->value, y.impl_->value, lhs.impl_->value, rhs.impl_->value);

  auto shrink = [](BigInt& v) {
    int64_t small = 0;
    if (fits_in_int64(v.impl_->value, &small)) {
      v.is_small_ = true;
      v.small_value_ = small;
      v.impl_.reset();
    }
  };
  shrink(g);
  shrink(x);
  shrink(y);
  return {g, x, y};
}

} // namespace numerica_core
