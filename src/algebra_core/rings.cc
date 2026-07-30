#include <algebra_core/rings.hh>

namespace algebra_core {

IntegerRing::Element IntegerRing::zero() const {
  return Element(0);
}
IntegerRing::Element IntegerRing::one() const {
  return Element(1);
}
IntegerRing::Element IntegerRing::add(const Element& a, const Element& b) const {
  return a + b;
}
IntegerRing::Element IntegerRing::sub(const Element& a, const Element& b) const {
  return a - b;
}
IntegerRing::Element IntegerRing::mul(const Element& a, const Element& b) const {
  return a * b;
}
IntegerRing::Element IntegerRing::neg(const Element& a) const {
  return -a;
}

std::pair<IntegerRing::Element, IntegerRing::Element> IntegerRing::div_rem(const Element& a,
                                                                           const Element& b) const {
  return {a / b, a % b};
}

RationalField::Element RationalField::zero() const {
  return Element(0);
}
RationalField::Element RationalField::one() const {
  return Element(1);
}
RationalField::Element RationalField::add(const Element& a, const Element& b) const {
  return a + b;
}
RationalField::Element RationalField::sub(const Element& a, const Element& b) const {
  return a - b;
}
RationalField::Element RationalField::mul(const Element& a, const Element& b) const {
  return a * b;
}
RationalField::Element RationalField::neg(const Element& a) const {
  return -a;
}
RationalField::Element RationalField::inv(const Element& a) const {
  return a.inv();
}

} // namespace algebra_core
