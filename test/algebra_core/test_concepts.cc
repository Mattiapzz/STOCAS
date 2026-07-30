#include <algebra_core/concepts.hh>

#include <catch2/catch_test_macros.hpp>

using algebra_core::EuclideanDomain;
using algebra_core::Field;
using algebra_core::Ring;
using algebra_core::Semiring;

namespace {

// Minimal mock ring-struct types at each level of the hierarchy, used only
// to compile-check the concepts themselves (M2-T2 wraps the real
// numerica_core types as IntegerRing/RationalField/FiniteFieldRing<T>).

struct SemiringOnlyMock {
  using Element = int;
  [[nodiscard]] Element zero() const { return 0; }
  [[nodiscard]] Element one() const { return 1; }
  [[nodiscard]] Element add(Element a, Element b) const { return a + b; }
  [[nodiscard]] Element mul(Element a, Element b) const { return a * b; }
};

struct RingMock : SemiringOnlyMock {
  [[nodiscard]] Element sub(Element a, Element b) const { return a - b; }
  [[nodiscard]] Element neg(Element a) const { return -a; }
};

struct EuclideanDomainMock : RingMock {
  [[nodiscard]] std::pair<Element, Element> div_rem(Element a, Element b) const {
    return {a / b, a % b};
  }
};

struct FieldMock : RingMock {
  [[nodiscard]] Element inv(Element a) const { return 1 / a; }
};

struct NotEvenSemiringMock {
  using Element = int;
};

static_assert(Semiring<SemiringOnlyMock>);
static_assert(!Ring<SemiringOnlyMock>);

static_assert(Semiring<RingMock>);
static_assert(Ring<RingMock>);
static_assert(!EuclideanDomain<RingMock>);
static_assert(!Field<RingMock>);

static_assert(Ring<EuclideanDomainMock>);
static_assert(EuclideanDomain<EuclideanDomainMock>);
static_assert(!Field<EuclideanDomainMock>);

static_assert(Ring<FieldMock>);
static_assert(Field<FieldMock>);
static_assert(!EuclideanDomain<FieldMock>);

static_assert(!Semiring<NotEvenSemiringMock>);

} // namespace

TEST_CASE("Ring-hierarchy concepts accept satisfying mock types", "[algebra_core][concepts]") {
  const EuclideanDomainMock ring;
  REQUIRE(ring.add(2, 3) == 5);
  REQUIRE(ring.sub(5, 3) == 2);
  REQUIRE(ring.mul(4, 3) == 12);
  const auto [quotient, remainder] = ring.div_rem(7, 2);
  REQUIRE(quotient == 3);
  REQUIRE(remainder == 1);
}

TEST_CASE("Field mock supports inversion", "[algebra_core][concepts]") {
  const FieldMock field;
  REQUIRE(field.mul(field.one(), 5) == 5);
}
