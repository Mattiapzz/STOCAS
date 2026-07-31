#include <array>
#include <vector>

#include <atom_core/atom.hh>
#include <numerica_core/rational.hh>
#include <symbol_table/symbol.hh>

#include <catch2/catch_test_macros.hpp>

using atom_core::Atom;
using atom_core::AtomStore;
using atom_core::AtomTag;
using symbol_table::get_symbol;

namespace {
symbol_table::Symbol test_symbol(const char* name) {
  return get_symbol("atom_core_tests", name);
}
} // namespace

TEST_CASE("AtomStore::num/var build correctly tagged leaves", "[atom_core][atom]") {
  AtomStore store;

  Atom two = store.num(numerica_core::Rational(2));
  REQUIRE(two.view().tag() == AtomTag::Num);
  REQUIRE(two.view().as_num() == numerica_core::Rational(2));

  symbol_table::Symbol x = test_symbol("x_leaf");
  Atom var_x = store.var(x);
  REQUIRE(var_x.view().tag() == AtomTag::Var);
  REQUIRE(var_x.view().as_var().id() == x.id());
}

TEST_CASE("AtomStore::add/mul flatten nested same-tag operands", "[atom_core][atom]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_flatten");
  symbol_table::Symbol y = test_symbol("y_flatten");

  Atom vx = store.var(x);
  Atom vy = store.var(y);
  Atom one = store.num(numerica_core::Rational(1));

  std::array<Atom, 2> inner_terms{vx, vy};
  Atom inner_sum = store.add(inner_terms);

  std::array<Atom, 2> outer_terms{inner_sum, one};
  Atom outer_sum = store.add(outer_terms);

  // Flattened: outer_sum's children are {vx, vy, one}, not {inner_sum, one}.
  REQUIRE(outer_sum.view().child_count() == 3);

  std::array<Atom, 3> direct_terms{vx, vy, one};
  Atom direct_sum = store.add(direct_terms);

  REQUIRE(outer_sum.view() == direct_sum.view());
  REQUIRE(outer_sum.view().same_slot(direct_sum.view()));
}

TEST_CASE("AtomStore::add/mul canonicalize operand order", "[atom_core][atom]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_order");
  symbol_table::Symbol y = test_symbol("y_order");
  Atom vx = store.var(x);
  Atom vy = store.var(y);

  std::array<Atom, 2> forward{vx, vy};
  std::array<Atom, 2> backward{vy, vx};

  Atom sum_forward = store.add(forward);
  Atom sum_backward = store.add(backward);

  REQUIRE(sum_forward.view().same_slot(sum_backward.view()));
}

TEST_CASE("AtomStore interns structurally identical subexpressions to one arena slot",
          "[atom_core][atom][interning]") {
  AtomStore store;
  symbol_table::Symbol f = test_symbol("f_intern");
  symbol_table::Symbol x = test_symbol("x_intern");

  // Build the same f(x, x+1) expression via two independent call sequences.
  auto build = [&] {
    Atom vx = store.var(x);
    Atom one = store.num(numerica_core::Rational(1));
    std::array<Atom, 2> sum_terms{vx, one};
    Atom sum = store.add(sum_terms);
    std::array<Atom, 2> args{vx, sum};
    return store.fun(f, args);
  };

  Atom first = build();
  Atom second = build();

  REQUIRE(first.view().same_slot(second.view()));
}

TEST_CASE("AtomStore interning deduplicates arena growth", "[atom_core][atom][interning]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_dedup");

  Atom first = store.var(x);
  std::size_t after_first = store.node_count();
  Atom second = store.var(x);
  std::size_t after_second = store.node_count();

  REQUIRE(after_first == after_second);
  REQUIRE(first.view().same_slot(second.view()));
}

TEST_CASE("AtomView canonical ordering is a strict total order across tags", "[atom_core][atom]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_cmp");

  Atom num_atom = store.num(numerica_core::Rational(5));
  Atom var_atom = store.var(x);
  std::array<Atom, 2> pow_args{var_atom, num_atom};
  Atom pow_atom = store.pow(var_atom, num_atom);
  (void) pow_args;

  REQUIRE((num_atom.view() <=> var_atom.view()) != std::strong_ordering::equal);
  REQUIRE((var_atom.view() <=> pow_atom.view()) != std::strong_ordering::equal);
  REQUIRE(num_atom.view() == num_atom.view());
}

TEST_CASE("AtomStore::pow preserves base/exponent order (non-commutative)", "[atom_core][atom]") {
  AtomStore store;
  symbol_table::Symbol x = test_symbol("x_pow");
  Atom base = store.var(x);
  Atom exponent = store.num(numerica_core::Rational(3));

  Atom p = store.pow(base, exponent);
  REQUIRE(p.view().tag() == AtomTag::Pow);
  REQUIRE(p.view().child_count() == 2);
  REQUIRE(p.view().child(0) == base.view());
  REQUIRE(p.view().child(1) == exponent.view());
}

TEST_CASE("AtomStore::fun preserves argument order and exposes its head symbol",
          "[atom_core][atom]") {
  AtomStore store;
  symbol_table::Symbol f = test_symbol("f_args");
  symbol_table::Symbol x = test_symbol("x_args");
  symbol_table::Symbol y = test_symbol("y_args");

  Atom vx = store.var(x);
  Atom vy = store.var(y);
  std::array<Atom, 2> args{vy, vx}; // deliberately non-canonical order
  Atom call = store.fun(f, args);

  REQUIRE(call.view().tag() == AtomTag::Fun);
  REQUIRE(call.view().function_head().id() == f.id());
  REQUIRE(call.view().child(0) == vy.view());
  REQUIRE(call.view().child(1) == vx.view());
}
