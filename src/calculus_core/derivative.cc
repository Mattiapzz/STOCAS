#include <array>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include <calculus_core/derivative.hh>

namespace calculus_core {

namespace {

using atom_core::Atom;
using atom_core::AtomStore;
using atom_core::AtomTag;
using atom_core::AtomView;
using numerica_core::Rational;
using symbol_table::Symbol;

// Symbol has no operator< (only identity/equality is guaranteed by the
// symbol table contract) - same ordering trick as pattern_core's
// SymbolIdLess, duplicated locally rather than shared since calculus_core
// doesn't otherwise depend on pattern_core.
struct SymbolIdLess {
  [[nodiscard]] bool operator()(const Symbol& lhs, const Symbol& rhs) const noexcept {
    return lhs.id() < rhs.id();
  }
};

// Process-wide registry of per-function-head derivative hooks.
//
// Concurrency: a std::shared_mutex guards the map - concurrent
// derivative() calls (e.g. from pattern_core::parallel_replace_all-style
// multi-threaded use) only ever take the shared (read) lock and never
// block each other; register_derivative_hook() takes the exclusive lock,
// same shape as any reader/writer split elsewhere in the codebase.
class HookRegistry {
public:
  static HookRegistry& instance() {
    static HookRegistry registry;
    return registry;
  }

  void set(Symbol head, DerivativeHook hook) {
    const std::unique_lock lock(mutex_);
    hooks_[head] = std::move(hook);
  }

  [[nodiscard]] std::optional<DerivativeHook> get(Symbol head) const {
    const std::shared_lock lock(mutex_);
    const auto it = hooks_.find(head);
    if (it == hooks_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

private:
  mutable std::shared_mutex mutex_;
  std::map<Symbol, DerivativeHook, SymbolIdLess> hooks_;
};

[[nodiscard]] bool is_zero(AtomView atom) {
  return atom.tag() == AtomTag::Num && atom.as_num() == Rational(0);
}

[[nodiscard]] bool register_builtin_hooks() {
  register_derivative_hook(sin_symbol(), [](AtomStore& store, std::span<const AtomView> args) {
    if (args.size() != 1) {
      throw std::invalid_argument("calculus_core: sin() takes exactly one argument");
    }
    return std::vector<Atom>{store.fun(cos_symbol(), std::array<Atom, 1>{store.as_atom(args[0])})};
  });

  register_derivative_hook(cos_symbol(), [](AtomStore& store, std::span<const AtomView> args) {
    if (args.size() != 1) {
      throw std::invalid_argument("calculus_core: cos() takes exactly one argument");
    }
    const Atom sin_arg = store.fun(sin_symbol(), std::array<Atom, 1>{store.as_atom(args[0])});
    return std::vector<Atom>{store.mul(std::array<Atom, 2>{store.num(Rational(-1)), sin_arg})};
  });

  register_derivative_hook(exp_symbol(), [](AtomStore& store, std::span<const AtomView> args) {
    if (args.size() != 1) {
      throw std::invalid_argument("calculus_core: exp() takes exactly one argument");
    }
    return std::vector<Atom>{store.fun(exp_symbol(), std::array<Atom, 1>{store.as_atom(args[0])})};
  });

  register_derivative_hook(ln_symbol(), [](AtomStore& store, std::span<const AtomView> args) {
    if (args.size() != 1) {
      throw std::invalid_argument("calculus_core: ln() takes exactly one argument");
    }
    return std::vector<Atom>{store.pow(store.as_atom(args[0]), store.num(Rational(-1)))};
  });

  return true;
}

void ensure_builtin_hooks_registered() {
  static const bool registered = register_builtin_hooks();
  (void) registered;
}

Atom derivative_impl(AtomStore& store, AtomView expr, Symbol variable);

Atom derivative_add(AtomStore& store, AtomView expr, Symbol variable) {
  std::vector<Atom> terms;
  terms.reserve(expr.child_count());
  for (std::size_t i = 0; i < expr.child_count(); ++i) {
    terms.push_back(derivative_impl(store, expr.child(i), variable));
  }
  return store.add(terms);
}

// Product rule: d/dx(f1*f2*...*fn) = sum_i (fi' * prod_{j!=i} fj).
Atom derivative_mul(AtomStore& store, AtomView expr, Symbol variable) {
  const std::size_t child_count = expr.child_count();
  std::vector<Atom> terms;
  terms.reserve(child_count);
  for (std::size_t i = 0; i < child_count; ++i) {
    std::vector<Atom> factors;
    factors.reserve(child_count);
    factors.push_back(derivative_impl(store, expr.child(i), variable));
    for (std::size_t j = 0; j < child_count; ++j) {
      if (j != i) {
        factors.push_back(store.as_atom(expr.child(j)));
      }
    }
    terms.push_back(store.mul(factors));
  }
  return store.add(terms);
}

// Generalized power rule for f^g - see derivative.hh's file comment for
// the case breakdown (this mirrors it exactly).
Atom derivative_pow(AtomStore& store, AtomView expr, Symbol variable) {
  const AtomView base = expr.child(0);
  const AtomView exponent = expr.child(1);
  const Atom base_derivative = derivative_impl(store, base, variable);
  const Atom exponent_derivative = derivative_impl(store, exponent, variable);
  const bool base_constant = is_zero(base_derivative.view());
  const bool exponent_constant = is_zero(exponent_derivative.view());

  if (base_constant && exponent_constant) {
    return store.num(Rational(0));
  }

  const Atom base_atom = store.as_atom(base);
  const Atom exponent_atom = store.as_atom(exponent);

  if (exponent_constant) {
    const Atom exponent_minus_one =
        exponent.tag() == AtomTag::Num
            ? store.num(exponent.as_num() - Rational(1))
            : store.add(std::array<Atom, 2>{exponent_atom, store.num(Rational(-1))});
    const Atom power_term = store.pow(base_atom, exponent_minus_one);
    return store.mul(std::array<Atom, 3>{exponent_atom, power_term, base_derivative});
  }

  const Atom power_term = store.pow(base_atom, exponent_atom);
  const Atom ln_base = store.fun(ln_symbol(), std::array<Atom, 1>{base_atom});
  if (base_constant) {
    return store.mul(std::array<Atom, 3>{power_term, ln_base, exponent_derivative});
  }

  const Atom term1 = store.mul(std::array<Atom, 2>{exponent_derivative, ln_base});
  const Atom base_inverse = store.pow(base_atom, store.num(Rational(-1)));
  const Atom term2 = store.mul(std::array<Atom, 3>{exponent_atom, base_derivative, base_inverse});
  const Atom inner = store.add(std::array<Atom, 2>{term1, term2});
  return store.mul(std::array<Atom, 2>{power_term, inner});
}

// Chain rule: d/dx head(a1,...,an) = sum_i d(head)/d(ai) * d(ai)/dx, with
// the partials d(head)/d(ai) coming from head's registered DerivativeHook.
Atom derivative_fun(AtomStore& store, AtomView expr, Symbol variable) {
  const Symbol head = expr.function_head();
  const std::optional<DerivativeHook> hook = HookRegistry::instance().get(head);
  if (!hook.has_value()) {
    throw std::invalid_argument("calculus_core::derivative: no derivative hook registered for '" +
                                symbol_table::symbol_name(head) +
                                "' - register one with register_derivative_hook()");
  }

  std::vector<AtomView> args;
  args.reserve(expr.child_count());
  for (std::size_t i = 0; i < expr.child_count(); ++i) {
    args.push_back(expr.child(i));
  }

  std::vector<Atom> partials = (*hook)(store, args);
  if (partials.size() != args.size()) {
    throw std::logic_error("calculus_core::derivative: derivative hook for '" +
                           symbol_table::symbol_name(head) + "' returned " +
                           std::to_string(partials.size()) + " partial(s) for " +
                           std::to_string(args.size()) + " argument(s)");
  }
  if (args.empty()) {
    return store.num(Rational(0));
  }

  std::vector<Atom> terms;
  terms.reserve(args.size());
  for (std::size_t i = 0; i < args.size(); ++i) {
    const Atom argument_derivative = derivative_impl(store, args[i], variable);
    terms.push_back(store.mul(std::array<Atom, 2>{partials[i], argument_derivative}));
  }
  return store.add(terms);
}

Atom derivative_impl(AtomStore& store, AtomView expr, Symbol variable) {
  switch (expr.tag()) {
    case AtomTag::Num:
      return store.num(Rational(0));
    case AtomTag::Var:
      return store.num(Rational(expr.as_var() == variable ? 1 : 0));
    case AtomTag::Add:
      return derivative_add(store, expr, variable);
    case AtomTag::Mul:
      return derivative_mul(store, expr, variable);
    case AtomTag::Pow:
      return derivative_pow(store, expr, variable);
    case AtomTag::Fun:
      return derivative_fun(store, expr, variable);
  }
  return store.num(Rational(0)); // unreachable, silences -Wreturn-type
}

} // namespace

symbol_table::Symbol sin_symbol() {
  static const Symbol symbol = symbol_table::get_symbol("calculus_core", "sin");
  return symbol;
}

symbol_table::Symbol cos_symbol() {
  static const Symbol symbol = symbol_table::get_symbol("calculus_core", "cos");
  return symbol;
}

symbol_table::Symbol exp_symbol() {
  static const Symbol symbol = symbol_table::get_symbol("calculus_core", "exp");
  return symbol;
}

symbol_table::Symbol ln_symbol() {
  static const Symbol symbol = symbol_table::get_symbol("calculus_core", "ln");
  return symbol;
}

void register_derivative_hook(symbol_table::Symbol head, DerivativeHook hook) {
  HookRegistry::instance().set(head, std::move(hook));
}

atom_core::Atom
derivative(atom_core::AtomStore& store, atom_core::AtomView expr, symbol_table::Symbol variable) {
  ensure_builtin_hooks_registered();
  return derivative_impl(store, expr, variable);
}

} // namespace calculus_core
