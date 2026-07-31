#include <array>

#include <atom_core/atom.hh>
#include <atom_core/normalize.hh>
#include <atom_core/parser.hh>
#include <atom_core/printer.hh>
#include <numerica_core/rational.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using atom_core::Atom;
using atom_core::AtomStore;
using atom_core::AtomTag;
using atom_core::normalize;
using atom_core::parse;
using atom_core::print;

namespace {
constexpr const char* kNs = "atom_core_normalize_tests";

Atom p(AtomStore& store, std::string_view expr) {
  return parse(expr, store, kNs);
}

// Normalizes both sides and asserts structural equality - this is the
// actual normal-form assertion, independent of surface syntax.
void require_normalizes_to(AtomStore& store, std::string_view input, std::string_view expected) {
  Atom normalized_input = normalize(store, p(store, input).view());
  Atom normalized_expected = normalize(store, p(store, expected).view());
  INFO("input: " << input << " -> " << print(normalized_input.view()) << " (expected: " << expected
                 << ")");
  REQUIRE(normalized_input.view() == normalized_expected.view());
}
} // namespace

// The M3-T4 normalization table required by tasks.yaml.
TEST_CASE("normalize: x + x -> 2x", "[atom_core][normalize]") {
  AtomStore store;
  require_normalizes_to(store, "x + x", "2 * x");
}

TEST_CASE("normalize: x * x -> x^2", "[atom_core][normalize]") {
  AtomStore store;
  require_normalizes_to(store, "x * x", "x^2");
}

TEST_CASE("normalize: (x+1)^0 -> 1", "[atom_core][normalize]") {
  AtomStore store;
  require_normalizes_to(store, "(x + 1)^0", "1");
}

TEST_CASE("normalize: 0*f(x) -> 0", "[atom_core][normalize]") {
  AtomStore store;
  require_normalizes_to(store, "0 * f(x)", "0");
}

TEST_CASE("normalize: additional constant-folding and like-term cases", "[atom_core][normalize]") {
  AtomStore store;
  require_normalizes_to(store, "1 + 2 + 3", "6");
  require_normalizes_to(store, "2 * x + 3 * x", "5 * x");
  require_normalizes_to(store, "x + 2 * x", "3 * x");
  require_normalizes_to(store, "x * x * x", "x^3");
  require_normalizes_to(store, "x^2 * x^3", "x^5");
  require_normalizes_to(store, "x^1", "x");
  require_normalizes_to(store, "x + 0", "x");
  require_normalizes_to(store, "x * 1", "x");
  require_normalizes_to(store, "0 + 0", "0");
}

TEST_CASE("normalize: is idempotent", "[atom_core][normalize]") {
  AtomStore store;
  const std::array<std::string_view, 6> corpus{
      "x + x",
      "x * x",
      "(x + 1)^0",
      "0 * f(x)",
      "2*x + 3*y - x",
      "f(x + x, y * y)",
  };
  for (std::string_view expr : corpus) {
    Atom once = normalize(store, p(store, expr).view());
    Atom twice = normalize(store, once.view());
    INFO("expr: " << expr);
    REQUIRE(once.view() == twice.view());
  }
}

TEST_CASE("normalize: normalizes nested subexpressions (inside Pow base and Fun args)",
          "[atom_core][normalize]") {
  AtomStore store;
  require_normalizes_to(store, "(x + x)^1", "2 * x");
  require_normalizes_to(store, "f(x + x)", "f(2 * x)");
}
