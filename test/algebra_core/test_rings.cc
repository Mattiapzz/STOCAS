#include <algebra_core/concepts.hh>
#include <algebra_core/rings.hh>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

using algebra_core::EuclideanDomain;
using algebra_core::Field;
using algebra_core::FiniteFieldRing;
using algebra_core::IntegerRing;
using algebra_core::RationalField;
using algebra_core::Ring;

static_assert(Ring<IntegerRing>);
static_assert(EuclideanDomain<IntegerRing>);
static_assert(!Field<IntegerRing>);

static_assert(Ring<RationalField>);
static_assert(Field<RationalField>);

static_assert(Ring<FiniteFieldRing<std::uint32_t>>);
static_assert(Field<FiniteFieldRing<std::uint32_t>>);
static_assert(Ring<FiniteFieldRing<std::uint64_t>>);
static_assert(Field<FiniteFieldRing<std::uint64_t>>);

TEST_CASE("IntegerRing wraps BigInt arithmetic", "[algebra_core][rings]") {
  const IntegerRing ring;
  REQUIRE(ring.add(ring.zero(), IntegerRing::Element(3)) == IntegerRing::Element(3));
  REQUIRE(ring.mul(IntegerRing::Element(4), IntegerRing::Element(5)) == IntegerRing::Element(20));
  REQUIRE(ring.sub(IntegerRing::Element(4), IntegerRing::Element(5)) == IntegerRing::Element(-1));
  REQUIRE(ring.neg(IntegerRing::Element(7)) == IntegerRing::Element(-7));

  const auto [quotient, remainder] = ring.div_rem(IntegerRing::Element(7), IntegerRing::Element(2));
  REQUIRE(quotient == IntegerRing::Element(3));
  REQUIRE(remainder == IntegerRing::Element(1));
}

TEST_CASE("RationalField wraps Rational arithmetic and inversion", "[algebra_core][rings]") {
  const RationalField field;
  const RationalField::Element half(numerica_core::BigInt(1), numerica_core::BigInt(2));
  REQUIRE(field.mul(half, RationalField::Element(2)) == field.one());
  REQUIRE(field.inv(half) == RationalField::Element(2));
  REQUIRE(field.add(field.zero(), half) == half);
}

TEMPLATE_TEST_CASE("FiniteFieldRing<T> hoists the modulus to the ring level",
                   "[algebra_core][rings]",
                   std::uint32_t,
                   std::uint64_t) {
  const FiniteFieldRing<TestType> field(TestType{101});
  const auto a = field.one();
  const auto b = field.add(a, a); // 2 (mod 101)
  REQUIRE(field.mul(b, field.inv(b)) == field.one());
  REQUIRE(field.zero().value() == 0);
}
