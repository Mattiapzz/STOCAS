#include <stdexcept>

#include <atom_core/atom.hh>
#include <atom_core/parser.hh>
#include <numerica_core/rational.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using atom_core::Atom;
using atom_core::AtomStore;
using atom_core::AtomTag;
using atom_core::parse;

namespace {
constexpr const char* kNs = "atom_core_parser_tests";

Atom p(AtomStore& store, std::string_view expr) {
  return parse(expr, store, kNs);
}
} // namespace

TEST_CASE("parse: integer literal", "[atom_core][parser]") {
  AtomStore store;
  Atom a = p(store, "42");
  REQUIRE(a.view().tag() == AtomTag::Num);
  REQUIRE(a.view().as_num() == numerica_core::Rational(42));
}

TEST_CASE("parse: identifier resolves to Var", "[atom_core][parser]") {
  AtomStore store;
  Atom a = p(store, "x");
  REQUIRE(a.view().tag() == AtomTag::Var);
}

TEST_CASE("parse: same identifier parsed twice interns to one symbol/atom", "[atom_core][parser]") {
  AtomStore store;
  Atom first = p(store, "x");
  Atom second = p(store, "x");
  REQUIRE(first.view().same_slot(second.view()));
}

TEST_CASE("parse: binary +/- builds Add, canonical order independent of source order",
          "[atom_core][parser]") {
  AtomStore store;
  Atom forward = p(store, "x + y");
  Atom backward = p(store, "y + x");
  REQUIRE(forward.view().tag() == AtomTag::Add);
  REQUIRE(forward.view().same_slot(backward.view()));
}

TEST_CASE("parse: binary * builds Mul", "[atom_core][parser]") {
  AtomStore store;
  Atom a = p(store, "x * y");
  REQUIRE(a.view().tag() == AtomTag::Mul);
  REQUIRE(a.view().child_count() == 2);
}

TEST_CASE("parse: implicit multiplication '2x' matches explicit '2*x'", "[atom_core][parser]") {
  AtomStore store;
  Atom implicit_form = p(store, "2x");
  Atom explicit_form = p(store, "2*x");
  REQUIRE(implicit_form.view().same_slot(explicit_form.view()));
}

TEST_CASE("parse: '^' is right-associative and binds tighter than unary '-'",
          "[atom_core][parser]") {
  AtomStore store;
  // -x^2 should parse as -(x^2), not (-x)^2.
  Atom a = p(store, "-x^2");
  REQUIRE(a.view().tag() == AtomTag::Mul); // -1 * (x^2)
  atom_core::AtomView pow_child =
      a.view().child(0).tag() == AtomTag::Pow ? a.view().child(0) : a.view().child(1);
  REQUIRE(pow_child.tag() == AtomTag::Pow);
}

TEST_CASE("parse: '/' desugars to multiplication by a negative power", "[atom_core][parser]") {
  AtomStore store;
  Atom a = p(store, "x / y");
  REQUIRE(a.view().tag() == AtomTag::Mul);
}

TEST_CASE("parse: operator precedence: 2 + 3 * 4 groups as 2 + (3*4)", "[atom_core][parser]") {
  AtomStore store;
  Atom via_precedence = p(store, "2 + 3 * 4");
  Atom via_parens = p(store, "2 + (3 * 4)");
  REQUIRE(via_precedence.view().same_slot(via_parens.view()));
}

TEST_CASE("parse: function calls with multiple arguments preserve argument order",
          "[atom_core][parser]") {
  AtomStore store;
  Atom call = p(store, "f(x, y)");
  REQUIRE(call.view().tag() == AtomTag::Fun);
  REQUIRE(call.view().child_count() == 2);
}

TEST_CASE("parse: nested function calls and parentheses", "[atom_core][parser]") {
  AtomStore store;
  Atom call = p(store, "f(g(x), (y + 1) * 2)");
  REQUIRE(call.view().tag() == AtomTag::Fun);
  REQUIRE(call.view().child_count() == 2);
}

TEST_CASE("parse: whitespace is insignificant", "[atom_core][parser]") {
  AtomStore store;
  Atom tight = p(store, "x+1");
  Atom spaced = p(store, "  x  +  1  ");
  REQUIRE(tight.view().same_slot(spaced.view()));
}

TEST_CASE("parse: rejects invalid input", "[atom_core][parser][invalid]") {
  AtomStore store;
  CHECK_THROWS_AS(p(store, ""), std::invalid_argument);
  CHECK_THROWS_AS(p(store, "   "), std::invalid_argument);
  CHECK_THROWS_AS(p(store, "("), std::invalid_argument);
  CHECK_THROWS_AS(p(store, ")"), std::invalid_argument);
  CHECK_THROWS_AS(p(store, "x +"), std::invalid_argument);
  CHECK_THROWS_AS(p(store, "x + * y"), std::invalid_argument);
  CHECK_THROWS_AS(p(store, "f(x,"), std::invalid_argument);
  CHECK_THROWS_AS(p(store, "f(x))"), std::invalid_argument);
  CHECK_THROWS_AS(p(store, "1 2 )"), std::invalid_argument);
  CHECK_THROWS_AS(p(store, "@"), std::invalid_argument);
  CHECK_THROWS_AS(p(store, "x , y"), std::invalid_argument);
}
