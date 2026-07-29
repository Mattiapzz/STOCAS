#include <hello_core/greeter.hh>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Greeter produces the expected greeting", "[hello_core][greeter]") {
  const hello_core::Greeter greeter("World");
  REQUIRE(greeter.greet() == "Hello, World!");
}
