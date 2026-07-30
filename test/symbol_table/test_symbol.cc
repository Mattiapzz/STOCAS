#include <array>
#include <stdexcept>

#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using symbol_table::get_symbol;
using symbol_table::Symbol;
using symbol_table::symbol_name;
using symbol_table::SymbolAttribute;

TEST_CASE("get_symbol registers a new symbol and reports its qualified name",
          "[symbol_table][symbol]") {
  const Symbol s = get_symbol("test_ns_register", "x");
  REQUIRE(symbol_name(s) == "test_ns_register::x");
}

TEST_CASE("get_symbol is idempotent for repeated registration with matching attributes",
          "[symbol_table][symbol]") {
  const Symbol first = get_symbol("test_ns_idempotent", "y");
  const Symbol second = get_symbol("test_ns_idempotent", "y");
  REQUIRE(first == second);
  REQUIRE(first.id() == second.id());
}

TEST_CASE("get_symbol resolves distinct namespaces to distinct symbols", "[symbol_table][symbol]") {
  const Symbol a = get_symbol("test_ns_one", "shared_name");
  const Symbol b = get_symbol("test_ns_two", "shared_name");
  REQUIRE_FALSE(a == b);
  REQUIRE(symbol_name(a) == "test_ns_one::shared_name");
  REQUIRE(symbol_name(b) == "test_ns_two::shared_name");
}

TEST_CASE("get_symbol rejects redefinition with a conflicting attribute set",
          "[symbol_table][symbol]") {
  const auto symmetric = std::to_array({SymbolAttribute::Symmetric});
  const Symbol original = get_symbol("test_ns_conflict", "f", symmetric);
  (void) original;

  const auto linear = std::to_array({SymbolAttribute::Linear});
  REQUIRE_THROWS_MATCHES(get_symbol("test_ns_conflict", "f", linear),
                         std::logic_error,
                         Catch::Matchers::MessageMatches(
                             Catch::Matchers::ContainsSubstring("test_ns_conflict::f") &&
                             Catch::Matchers::ContainsSubstring("originally registered at")));
}

TEST_CASE("get_symbol accepts re-registration with the same attribute set regardless of order",
          "[symbol_table][symbol]") {
  const auto order_a = std::to_array({SymbolAttribute::Symmetric, SymbolAttribute::Linear});
  const auto order_b = std::to_array({SymbolAttribute::Linear, SymbolAttribute::Symmetric});
  const Symbol first = get_symbol("test_ns_reorder", "g", order_a);
  const Symbol second = get_symbol("test_ns_reorder", "g", order_b);
  REQUIRE(first == second);
}

#ifdef CORE_NAMESPACE
TEST_CASE("SYMBOL_TABLE_SYMBOL resolves within the CMake-injected CORE_NAMESPACE",
          "[symbol_table][symbol]") {
  const Symbol s = SYMBOL_TABLE_SYMBOL("macro_registered");
  REQUIRE(symbol_name(s) == std::string(CORE_NAMESPACE) + "::macro_registered");
}
#endif
