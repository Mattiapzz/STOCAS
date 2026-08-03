#include <stdexcept>
#include <vector>

#include <poly_core/monomial.hh>

#include <catch2/catch_test_macros.hpp>

using poly_core::Exponent;
using poly_core::Monomial;

TEST_CASE("Monomial: total_degree and is_one", "[poly_core][monomial]") {
  Monomial one(std::vector<Exponent>{0, 0});
  REQUIRE(one.is_one());
  REQUIRE(one.total_degree() == 0);

  Monomial m(std::vector<Exponent>{2, 3});
  REQUIRE_FALSE(m.is_one());
  REQUIRE(m.total_degree() == 5);
}

TEST_CASE("Monomial: equality and lexicographic order", "[poly_core][monomial]") {
  Monomial a(std::vector<Exponent>{2, 0});
  Monomial b(std::vector<Exponent>{1, 5});
  Monomial c(std::vector<Exponent>{2, 0});

  REQUIRE(a == c);
  REQUIRE(a > b); // first variable dominates lexicographic order
  REQUIRE(b < a);
}

TEST_CASE("Monomial: multiplication sums exponents", "[poly_core][monomial]") {
  Monomial a(std::vector<Exponent>{2, 1});
  Monomial b(std::vector<Exponent>{0, 3});
  Monomial product = a * b;
  REQUIRE(product.exponent(0) == 2);
  REQUIRE(product.exponent(1) == 4);
}

TEST_CASE("Monomial: mismatched num_vars throws", "[poly_core][monomial]") {
  Monomial a(std::vector<Exponent>{1});
  Monomial b(std::vector<Exponent>{1, 2});
  REQUIRE_THROWS_AS(a * b, std::invalid_argument);
  REQUIRE_THROWS_AS(a <=> b, std::invalid_argument);
}
