#include <pattern_core/wildcard.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using symbol_table::get_symbol;

namespace {
symbol_table::Symbol test_symbol(const char* name) {
  return get_symbol("pattern_core_tests", name);
}
} // namespace

TEST_CASE("is_wildcard: trailing underscore convention", "[pattern_core][wildcard]") {
  REQUIRE(pattern_core::is_wildcard(test_symbol("x_")));
  REQUIRE(pattern_core::is_wildcard(test_symbol("wildcard_name_")));
  REQUIRE_FALSE(pattern_core::is_wildcard(test_symbol("x")));
  REQUIRE_FALSE(pattern_core::is_wildcard(test_symbol("f")));
  // The underscore has to be trailing, not merely present.
  REQUIRE_FALSE(pattern_core::is_wildcard(test_symbol("under_score")));
}
