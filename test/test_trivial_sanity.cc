#include <catch2/catch_test_macros.hpp>

TEST_CASE("Catch2 and CTest wiring is functional", "[sanity]") {
  REQUIRE(true == true);
}
