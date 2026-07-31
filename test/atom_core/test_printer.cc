#include <array>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include <atom_core/atom.hh>
#include <atom_core/parser.hh>
#include <atom_core/printer.hh>
#include <numerica_core/rational.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using atom_core::Atom;
using atom_core::AtomStore;
using atom_core::parse;
using atom_core::print;

namespace {
constexpr const char* kNs = "atom_core_printer_tests";

Atom p(AtomStore& store, std::string_view expr) {
  return parse(expr, store, kNs);
}

// Round-trips print(parse(expr)) through parse() again and asserts
// structural equality with the first parse - the actual M3-T3 guarantee
// (not string equality, which the printer's normalized form doesn't owe).
void require_round_trip(AtomStore& store, std::string_view expr) {
  Atom original = p(store, expr);
  std::string printed = print(original.view());
  Atom reparsed = p(store, printed);
  INFO("expr: " << expr << " printed: " << printed);
  REQUIRE(original.view() == reparsed.view());
}
} // namespace

TEST_CASE("print: leaves", "[atom_core][printer]") {
  AtomStore store;
  REQUIRE(print(store.num(numerica_core::Rational(5)).view()) == "5");
  REQUIRE(print(store.num(numerica_core::Rational(-5)).view()) == "-5");
  REQUIRE(print(store.var(symbol_table::get_symbol(kNs, "x")).view()) == "x");
}

TEST_CASE("print: Add/Mul use canonical order and no unnecessary parens", "[atom_core][printer]") {
  AtomStore store;
  Atom a = p(store, "y + x");
  REQUIRE(print(a.view()) == print(p(store, "x + y").view()));
}

TEST_CASE("print: Mul wraps an Add child in parens; Add never wraps a Mul child",
          "[atom_core][printer]") {
  AtomStore store;
  Atom a = p(store, "(x + 1) * 2");
  std::string printed = print(a.view());
  REQUIRE(printed.find('(') != std::string::npos);

  Atom b = p(store, "x * 2 + 1");
  std::string printed_b = print(b.view());
  // Top-level is Add; its Mul child needs no parens.
  REQUIRE(printed_b.find('(') == std::string::npos);
}

TEST_CASE("print: Pow wraps a Pow/Mul/Add base but not a Pow exponent", "[atom_core][printer]") {
  AtomStore store;
  Atom a = p(store, "(x^y)^z");
  REQUIRE(print(a.view()).find('(') != std::string::npos);

  Atom b = p(store, "x^y^z"); // right-assoc: x^(y^z)
  REQUIRE(print(b.view()).find('(') == std::string::npos);
}

TEST_CASE("round-trip: hand-written expression corpus", "[atom_core][printer][round-trip]") {
  AtomStore store;
  const std::array<std::string_view, 14> corpus{
      "42",
      "-7",
      "x",
      "x + y",
      "x - y",
      "x * y",
      "x / y",
      "x^2",
      "x^-1",
      "(x + 1)^2",
      "2x + 3y - 1",
      "f(x, y)",
      "f(g(x), (y + 1) * 2)",
      "-x^2 + 1",
  };
  for (std::string_view expr : corpus) {
    require_round_trip(store, expr);
  }
}

TEST_CASE("round-trip: randomized expressions built via the builder API",
          "[atom_core][printer][round-trip][fuzz]") {
  AtomStore store;
  symbol_table::Symbol x = symbol_table::get_symbol(kNs, "rand_x");
  symbol_table::Symbol y = symbol_table::get_symbol(kNs, "rand_y");
  symbol_table::Symbol f = symbol_table::get_symbol(kNs, "rand_f");

  std::mt19937 rng(42u);
  std::uniform_int_distribution<int> kind_dist(0, 5);
  std::uniform_int_distribution<int> value_dist(-20, 20);

  std::vector<Atom> pool{store.var(x), store.var(y)};

  std::function<Atom(int)> build = [&](int depth) -> Atom {
    if (depth <= 0) {
      int kind = kind_dist(rng) % 3;
      if (kind == 0) {
        return store.num(numerica_core::Rational(value_dist(rng)));
      }
      return pool[static_cast<std::size_t>(kind_dist(rng)) % pool.size()];
    }
    switch (kind_dist(rng)) {
      case 0: {
        std::array<Atom, 2> terms{build(depth - 1), build(depth - 1)};
        return store.add(terms);
      }
      case 1: {
        std::array<Atom, 2> factors{build(depth - 1), build(depth - 1)};
        return store.mul(factors);
      }
      case 2: {
        // Small positive integer exponents only - printer/parser round-trip
        // for negative-Num exponents is covered directly by the hand-written
        // corpus above ("x^-1"); this generator targets structural variety.
        return store.pow(build(depth - 1), store.num(numerica_core::Rational(2)));
      }
      case 3: {
        std::array<Atom, 2> args{build(depth - 1), build(depth - 1)};
        return store.fun(f, args);
      }
      default:
        return build(0);
    }
  };

  constexpr int kSamples = 200;
  for (int i = 0; i < kSamples; ++i) {
    Atom sample = build(3);
    std::string printed = print(sample.view());
    Atom reparsed = p(store, printed);
    INFO("printed: " << printed);
    REQUIRE(sample.view() == reparsed.view());
  }
}
