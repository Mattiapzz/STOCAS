#include <cstdint>
#include <type_traits>
#include <vector>

#include <algebra_core/generic_algorithms.hh>
#include <algebra_core/rings.hh>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

using algebra_core::extended_gcd;
using algebra_core::FiniteFieldRing;
using algebra_core::gcd;
using algebra_core::IntegerRing;
using algebra_core::RationalField;
using algebra_core::row_reduce;

TEST_CASE("gcd matches the expected value for IntegerRing", "[algebra_core][generic_algorithms]") {
  const IntegerRing ring;
  REQUIRE(gcd(ring, IntegerRing::Element(48), IntegerRing::Element(18)) == IntegerRing::Element(6));
  REQUIRE(gcd(ring, IntegerRing::Element(0), IntegerRing::Element(5)) == IntegerRing::Element(5));
}

// The same test body run against IntegerRing (a genuine EuclideanDomain) and
// FiniteFieldRing<u32> (a field, i.e. a trivial EuclideanDomain where every
// division is exact) - this is the actual regression test for "genericity
// works", per the M2-T2 task. Checks the domain-agnostic property (the
// result divides both operands via an exact div_rem), not a specific
// canonical value, since "gcd up to units" differs between the two domains.
TEMPLATE_TEST_CASE("gcd's result divides both operands, genuinely generic over the ring type",
                   "[algebra_core][generic_algorithms]",
                   IntegerRing,
                   FiniteFieldRing<std::uint32_t>) {
  TestType ring = [] {
    if constexpr (std::is_same_v<TestType, IntegerRing>) {
      return IntegerRing();
    } else {
      return FiniteFieldRing<std::uint32_t>(101);
    }
  }();
  const typename TestType::Element a(ring.add(ring.add(ring.one(), ring.one()), ring.one())); // 3
  const typename TestType::Element b(
      ring.add(ring.add(ring.one(), ring.one()), ring.add(ring.one(), ring.one()))); // 4

  const typename TestType::Element result = gcd(ring, a, b);
  REQUIRE(ring.div_rem(a, result).second == ring.zero());
  REQUIRE(ring.div_rem(b, result).second == ring.zero());
}

TEST_CASE("extended_gcd satisfies the Bezout identity for IntegerRing",
          "[algebra_core][generic_algorithms]") {
  const IntegerRing ring;
  const auto result = extended_gcd(ring, IntegerRing::Element(35), IntegerRing::Element(15));
  REQUIRE(result.gcd == IntegerRing::Element(5));
  REQUIRE(ring.add(ring.mul(result.x, IntegerRing::Element(35)),
                   ring.mul(result.y, IntegerRing::Element(15))) == result.gcd);
}

TEST_CASE("row_reduce solves a 2x2 linear system over RationalField",
          "[algebra_core][generic_algorithms]") {
  const RationalField field;
  // x + y = 3
  // 2x - y = 0  =>  x = 1, y = 2
  std::vector<std::vector<RationalField::Element>> matrix = {
      {RationalField::Element(1), RationalField::Element(1), RationalField::Element(3)},
      {RationalField::Element(2), RationalField::Element(-1), RationalField::Element(0)}};

  row_reduce(field, matrix);

  REQUIRE(matrix[0][2] == RationalField::Element(1));
  REQUIRE(matrix[1][2] == RationalField::Element(2));
}
