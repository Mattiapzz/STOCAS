#include <stdexcept>

#include <numerica_core/big_int.hh>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

using numerica_core::BigInt;

TEST_CASE("BigInt small-int fast path", "[numerica_core][big_int]") {
  const BigInt a(3);
  const BigInt b(4);
  REQUIRE(a.is_small());
  REQUIRE(b.is_small());
  REQUIRE((a + b).to_string() == "7");
  REQUIRE((a - b).to_string() == "-1");
  REQUIRE((a * b).to_string() == "12");
  REQUIRE((b / a).to_string() == "1");
  REQUIRE((b % a).to_string() == "1");
  REQUIRE(a < b);
  REQUIRE(a != b);
}

TEST_CASE("BigInt promotes above int64_t range", "[numerica_core][big_int]") {
  const BigInt huge("100000000000000000000000000000"); // 10^29, far beyond int64
  REQUIRE_FALSE(huge.is_small());
  REQUIRE(huge.to_string() == "100000000000000000000000000000");

  const BigInt doubled = huge + huge;
  REQUIRE(doubled.to_string() == "200000000000000000000000000000");
}

TEST_CASE("BigInt promotes exactly at the int64_t multiply overflow boundary",
          "[numerica_core][big_int]") {
  const BigInt max_i64("9223372036854775807");
  const BigInt result = max_i64 * BigInt(2);
  REQUIRE_FALSE(result.is_small());
  REQUIRE(result.to_string() == "18446744073709551614");
}

TEST_CASE("BigInt division and modulo by zero throw", "[numerica_core][big_int]") {
  const BigInt a(10);
  const BigInt zero(0);
  REQUIRE_THROWS_AS(a / zero, std::domain_error);
  REQUIRE_THROWS_AS(a % zero, std::domain_error);
}

TEST_CASE("BigInt rejects invalid decimal strings", "[numerica_core][big_int]") {
  REQUIRE_THROWS_AS(BigInt("not-a-number"), std::invalid_argument);
}

TEST_CASE("BigInt gcd and extended_gcd", "[numerica_core][big_int]") {
  const BigInt a(48);
  const BigInt b(18);
  REQUIRE(BigInt::gcd(a, b).to_string() == "6");

  auto [g, x, y] = BigInt::extended_gcd(a, b);
  REQUIRE(g.to_string() == "6");
  REQUIRE((a * x + b * y) == g);
}

TEST_CASE("BigInt property: addition is commutative and associative", "[numerica_core][big_int]") {
  auto value = GENERATE(take(20, random(-1000000, 1000000)));
  auto other = GENERATE(take(5, random(-1000000, 1000000)));
  const BigInt a(static_cast<int64_t>(value));
  const BigInt b(static_cast<int64_t>(other));
  const BigInt c(7);

  REQUIRE(a + b == b + a);
  REQUIRE((a + b) + c == a + (b + c));
}

TEST_CASE("BigInt property: multiplication is commutative and associative",
          "[numerica_core][big_int]") {
  auto value = GENERATE(take(20, random(-1000, 1000)));
  auto other = GENERATE(take(5, random(-1000, 1000)));
  const BigInt a(static_cast<int64_t>(value));
  const BigInt b(static_cast<int64_t>(other));
  const BigInt c(3);

  REQUIRE(a * b == b * a);
  REQUIRE((a * b) * c == a * (b * c));
}

// Baseline perf tracking, not a CI gate: run explicitly via
// `numerica_core_tests "[!benchmark]"` (hidden tag excludes it from the
// default ctest run).
TEST_CASE("BigInt multiply/gcd benchmarks at several bit widths",
          "[numerica_core][big_int][!benchmark]") {
  const BigInt small_a(123456789);
  const BigInt small_b(987654321);
  const BigInt big_a("123456789012345678901234567890123456789012345678901234567890");
  const BigInt big_b("987654321098765432109876543210987654321098765432109876543210");

  BENCHMARK("multiply, ~30 bits") {
    return small_a * small_b;
  };
  BENCHMARK("multiply, ~200 bits") {
    return big_a * big_b;
  };
  BENCHMARK("gcd, ~30 bits") {
    return BigInt::gcd(small_a, small_b);
  };
  BENCHMARK("gcd, ~200 bits") {
    return BigInt::gcd(big_a, big_b);
  };
}
