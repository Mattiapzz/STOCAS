#include <algorithm>
#include <cmath>
#include <sstream>

#include <mpfr.h>
#include <numerica_core/float_types.hh>

namespace numerica_core {

// ---------------------------------------------------------------- F64 ----

std::string F64::to_string() const {
  std::ostringstream stream;
  stream.precision(17);
  stream << value_;
  return stream.str();
}

F64 F64::exp(const F64& x) {
  return F64(std::exp(x.value_));
}

F64 F64::expm1(const F64& x) {
  return F64(std::expm1(x.value_));
}

F64 F64::log(const F64& x) {
  return F64(std::log(x.value_));
}

F64 F64::sin(const F64& x) {
  return F64(std::sin(x.value_));
}

F64 F64::cos(const F64& x) {
  return F64(std::cos(x.value_));
}

// -------------------------------------------------------------- Float ----

struct Float::Impl {
  mpfr_t value;

  explicit Impl(unsigned precision_bits) { mpfr_init2(value, precision_bits); }
  Impl(const Impl& other) {
    mpfr_init2(value, mpfr_get_prec(other.value));
    mpfr_set(value, other.value, MPFR_RNDN);
  }
  ~Impl() { mpfr_clear(value); }
};

Float::Float(unsigned precision_bits) : impl_(std::make_unique<Impl>(precision_bits)) {
  mpfr_set_zero(impl_->value, 1);
}

Float::Float(double value, unsigned precision_bits)
  : impl_(std::make_unique<Impl>(precision_bits)) {
  mpfr_set_d(impl_->value, value, MPFR_RNDN);
}

Float::Float(const Float& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}

Float::Float(Float&& other) noexcept = default;

Float& Float::operator=(const Float& other) {
  if (this != &other) {
    impl_ = std::make_unique<Impl>(*other.impl_);
  }
  return *this;
}

Float& Float::operator=(Float&& other) noexcept = default;

Float::~Float() = default;

unsigned Float::precision_bits() const noexcept {
  return static_cast<unsigned>(mpfr_get_prec(impl_->value));
}

namespace {
unsigned max_precision(const Float& a, const Float& b) {
  return std::max(a.precision_bits(), b.precision_bits());
}
} // namespace

Float Float::operator+(const Float& other) const {
  Float result(max_precision(*this, other));
  mpfr_add(result.impl_->value, impl_->value, other.impl_->value, MPFR_RNDN);
  return result;
}

Float Float::operator-(const Float& other) const {
  Float result(max_precision(*this, other));
  mpfr_sub(result.impl_->value, impl_->value, other.impl_->value, MPFR_RNDN);
  return result;
}

Float Float::operator*(const Float& other) const {
  Float result(max_precision(*this, other));
  mpfr_mul(result.impl_->value, impl_->value, other.impl_->value, MPFR_RNDN);
  return result;
}

Float Float::operator/(const Float& other) const {
  Float result(max_precision(*this, other));
  mpfr_div(result.impl_->value, impl_->value, other.impl_->value, MPFR_RNDN);
  return result;
}

Float Float::operator-() const {
  Float result(precision_bits());
  mpfr_neg(result.impl_->value, impl_->value, MPFR_RNDN);
  return result;
}

bool Float::operator==(const Float& other) const {
  return mpfr_equal_p(impl_->value, other.impl_->value) != 0;
}

std::strong_ordering Float::operator<=>(const Float& other) const {
  const int cmp = mpfr_cmp(impl_->value, other.impl_->value);
  if (cmp < 0) {
    return std::strong_ordering::less;
  }
  if (cmp > 0) {
    return std::strong_ordering::greater;
  }
  return std::strong_ordering::equal;
}

double Float::to_double() const {
  return mpfr_get_d(impl_->value, MPFR_RNDN);
}

std::string Float::to_string(unsigned significant_digits) const {
  mpfr_exp_t exponent = 0;
  char* c_str = mpfr_get_str(nullptr, &exponent, 10, significant_digits, impl_->value, MPFR_RNDN);
  std::string digits(c_str);
  mpfr_free_str(c_str);

  bool negative = false;
  if (!digits.empty() && digits[0] == '-') {
    negative = true;
    digits.erase(0, 1);
  }
  if (digits.empty()) {
    digits = "0";
  }

  std::ostringstream stream;
  if (negative) {
    stream << '-';
  }
  stream << digits[0] << '.' << digits.substr(1) << 'e' << (exponent - 1);
  return stream.str();
}

Float Float::exp(const Float& x) {
  Float result(x.precision_bits());
  mpfr_exp(result.impl_->value, x.impl_->value, MPFR_RNDN);
  return result;
}

Float Float::expm1(const Float& x) {
  Float result(x.precision_bits());
  mpfr_expm1(result.impl_->value, x.impl_->value, MPFR_RNDN);
  return result;
}

Float Float::log(const Float& x) {
  Float result(x.precision_bits());
  mpfr_log(result.impl_->value, x.impl_->value, MPFR_RNDN);
  return result;
}

Float Float::sin(const Float& x) {
  Float result(x.precision_bits());
  mpfr_sin(result.impl_->value, x.impl_->value, MPFR_RNDN);
  return result;
}

Float Float::cos(const Float& x) {
  Float result(x.precision_bits());
  mpfr_cos(result.impl_->value, x.impl_->value, MPFR_RNDN);
  return result;
}

} // namespace numerica_core
