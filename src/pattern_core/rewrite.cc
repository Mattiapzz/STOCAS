#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

#include <numerica_core/rational.hh>
#include <pattern_core/rewrite.hh>
#include <pattern_core/wildcard.hh>
#include <symbol_table/symbol.hh>

namespace pattern_core {

namespace {

using atom_core::Atom;
using atom_core::AtomStore;
using atom_core::AtomTag;
using atom_core::AtomView;
using symbol_table::Symbol;

Atom substitute_impl(AtomStore& store, AtomView node, const MatchBindings& bindings) {
  switch (node.tag()) {
    case AtomTag::Num:
      return store.num(node.as_num());
    case AtomTag::Var: {
      Symbol symbol = node.as_var();
      auto existing = bindings.find(symbol);
      if (existing != bindings.end()) {
        const auto* bound_atom = std::get_if<AtomView>(&existing->second);
        if (bound_atom == nullptr) {
          throw std::invalid_argument("pattern_core::substitute: sequence wildcard '" +
                                      symbol_table::symbol_name(symbol) +
                                      "' used in a non-sequence position");
        }
        return store.as_atom(*bound_atom);
      }
      if (is_wildcard(symbol)) {
        throw std::invalid_argument("pattern_core::substitute: replacement references wildcard '" +
                                    symbol_table::symbol_name(symbol) + "' that was never bound");
      }
      return store.var(symbol);
    }
    case AtomTag::Pow: {
      Atom base = substitute_impl(store, node.child(0), bindings);
      Atom exponent = substitute_impl(store, node.child(1), bindings);
      return store.pow(base, exponent);
    }
    case AtomTag::Add:
    case AtomTag::Mul:
    case AtomTag::Fun: {
      std::vector<Atom> children;
      children.reserve(node.child_count());
      for (std::size_t i = 0; i < node.child_count(); ++i) {
        AtomView child = node.child(i);
        if (child.tag() == AtomTag::Var) {
          auto existing = bindings.find(child.as_var());
          if (existing != bindings.end()) {
            const auto* bound_sequence = std::get_if<std::vector<AtomView>>(&existing->second);
            if (bound_sequence != nullptr) {
              for (AtomView element : *bound_sequence) {
                children.push_back(store.as_atom(element));
              }
              continue;
            }
          }
        }
        children.push_back(substitute_impl(store, child, bindings));
      }
      if (node.tag() == AtomTag::Fun) {
        return store.fun(node.function_head(), children);
      }
      if (children.empty()) {
        return store.num(numerica_core::Rational(node.tag() == AtomTag::Add ? 0 : 1));
      }
      if (children.size() == 1) {
        return children[0];
      }
      return node.tag() == AtomTag::Add ? store.add(children) : store.mul(children);
    }
  }
  throw std::logic_error("pattern_core::substitute: unreachable AtomTag");
}

Atom replace_all_impl(AtomStore& store, const Rule& rule, AtomView node) {
  Atom rebuilt(store.as_atom(node));
  switch (node.tag()) {
    case AtomTag::Num:
    case AtomTag::Var:
      break;
    case AtomTag::Pow: {
      Atom base = replace_all_impl(store, rule, node.child(0));
      Atom exponent = replace_all_impl(store, rule, node.child(1));
      rebuilt = store.pow(base, exponent);
      break;
    }
    case AtomTag::Add:
    case AtomTag::Mul:
    case AtomTag::Fun: {
      std::vector<Atom> children;
      children.reserve(node.child_count());
      for (std::size_t i = 0; i < node.child_count(); ++i) {
        children.push_back(replace_all_impl(store, rule, node.child(i)));
      }
      if (node.tag() == AtomTag::Fun) {
        rebuilt = store.fun(node.function_head(), children);
      } else {
        rebuilt = node.tag() == AtomTag::Add ? store.add(children) : store.mul(children);
      }
      break;
    }
  }

  std::optional<MatchBindings> bindings = match(rule.pattern, rebuilt.view());
  if (bindings.has_value() && (!rule.guard || rule.guard(*bindings))) {
    return substitute_impl(store, rule.replacement, *bindings);
  }
  return rebuilt;
}

} // namespace

Atom substitute(AtomStore& store, AtomView replacement, const MatchBindings& bindings) {
  return substitute_impl(store, replacement, bindings);
}

Atom replace_all(AtomStore& store, const Rule& rule, AtomView target) {
  return replace_all_impl(store, rule, target);
}

Atom replace_all_multiple(AtomStore& store,
                          const Rule& rule,
                          AtomView target,
                          std::size_t max_iterations) {
  Atom current = store.as_atom(target);
  for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
    Atom next = replace_all(store, rule, current.view());
    if (next.view().same_slot(current.view())) {
      return next;
    }
    current = next;
  }
  throw std::runtime_error(
      "pattern_core::replace_all_multiple: exceeded max_iterations without converging - the "
      "rule set may not terminate");
}

} // namespace pattern_core
