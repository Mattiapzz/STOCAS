#include <array>
#include <map>
#include <vector>

#include <atom_core/normalize.hh>
#include <numerica_core/rational.hh>

namespace atom_core {

namespace {

struct AtomViewLess {
  bool operator()(AtomView lhs, AtomView rhs) const {
    return (lhs <=> rhs) == std::strong_ordering::less;
  }
};

// If `atom` is `Mul(Num(c), rest)` (a numeric coefficient times exactly one
// other factor - AtomStore's canonical child order always sorts a Num
// factor first, per AtomTag's declaration order, and this is the only
// coefficient shape normalize_mul() ever produces), returns {c, rest}.
// Otherwise returns {1, atom}: `atom` itself is the term, coefficient 1.
std::pair<numerica_core::Rational, AtomView> split_coefficient(AtomView atom) {
  if (atom.tag() == AtomTag::Mul && atom.child_count() == 2 &&
      atom.child(0).tag() == AtomTag::Num) {
    return {atom.child(0).as_num(), atom.child(1)};
  }
  return {numerica_core::Rational(1), atom};
}

// If `atom` is `Pow(base, Num(e))`, returns {base, e}. Otherwise returns
// {atom, 1}: `atom` itself is the base, exponent 1.
std::pair<AtomView, numerica_core::Rational> split_power(AtomView atom) {
  if (atom.tag() == AtomTag::Pow && atom.child(1).tag() == AtomTag::Num) {
    return {atom.child(0), atom.child(1).as_num()};
  }
  return {atom, numerica_core::Rational(1)};
}

Atom normalize_add(AtomStore& store, AtomView atom) {
  std::map<AtomView, numerica_core::Rational, AtomViewLess> terms;
  numerica_core::Rational constant(0);

  for (std::size_t i = 0; i < atom.child_count(); ++i) {
    AtomView child = normalize(store, atom.child(i)).view();
    if (child.tag() == AtomTag::Num) {
      constant = constant + child.as_num();
      continue;
    }
    auto [coefficient, term] = split_coefficient(child);
    auto [it, inserted] = terms.try_emplace(term, numerica_core::Rational(0));
    it->second = it->second + coefficient;
  }

  std::vector<Atom> children;
  children.reserve(terms.size() + 1);
  for (auto& [term, coefficient] : terms) {
    if (coefficient == numerica_core::Rational(0)) {
      continue;
    }
    if (coefficient == numerica_core::Rational(1)) {
      children.push_back(store.as_atom(term));
    } else {
      std::array<Atom, 2> factors{store.num(coefficient), store.as_atom(term)};
      children.push_back(store.mul(factors));
    }
  }
  if (constant != numerica_core::Rational(0) || children.empty()) {
    children.push_back(store.num(constant));
  }

  if (children.size() == 1) {
    return children.front();
  }
  return store.add(children);
}

Atom normalize_mul(AtomStore& store, AtomView atom) {
  std::map<AtomView, numerica_core::Rational, AtomViewLess> bases;
  numerica_core::Rational constant(1);

  for (std::size_t i = 0; i < atom.child_count(); ++i) {
    AtomView child = normalize(store, atom.child(i)).view();
    if (child.tag() == AtomTag::Num) {
      if (child.as_num() == numerica_core::Rational(0)) {
        return store.num(numerica_core::Rational(0)); // 0 * anything -> 0
      }
      constant = constant * child.as_num();
      continue;
    }
    auto [base, exponent] = split_power(child);
    auto [it, inserted] = bases.try_emplace(base, numerica_core::Rational(0));
    it->second = it->second + exponent;
  }

  std::vector<Atom> factors;
  factors.reserve(bases.size() + 1);
  for (auto& [base, exponent] : bases) {
    if (exponent == numerica_core::Rational(0)) {
      continue; // base^0 -> 1, contributes nothing to the product
    }
    if (exponent == numerica_core::Rational(1)) {
      factors.push_back(store.as_atom(base));
    } else {
      factors.push_back(store.pow(store.as_atom(base), store.num(exponent)));
    }
  }
  if (constant != numerica_core::Rational(1) || factors.empty()) {
    factors.push_back(store.num(constant));
    // Keep the numeric factor first for split_coefficient()'s assumption -
    // store.mul() re-sorts canonically anyway (Num always sorts first), so
    // this ordering doesn't actually matter here, but stated for clarity.
  }

  if (factors.size() == 1) {
    return factors.front();
  }
  return store.mul(factors);
}

Atom normalize_pow(AtomStore& store, AtomView atom) {
  Atom base = normalize(store, atom.child(0));
  Atom exponent = normalize(store, atom.child(1));

  if (exponent.view().tag() == AtomTag::Num) {
    numerica_core::Rational e = exponent.view().as_num();
    if (e == numerica_core::Rational(0)) {
      return store.num(numerica_core::Rational(1)); // (x+1)^0 -> 1
    }
    if (e == numerica_core::Rational(1)) {
      return base; // x^1 -> x
    }
    // General Num^Num constant-folding (e.g. 2^3 -> 8) is deferred - not
    // required by M3-T4's normalization table.
  }
  return store.pow(base, exponent);
}

Atom normalize_fun(AtomStore& store, AtomView atom) {
  std::vector<Atom> args;
  args.reserve(atom.child_count());
  for (std::size_t i = 0; i < atom.child_count(); ++i) {
    args.push_back(normalize(store, atom.child(i)));
  }
  return store.fun(atom.function_head(), args);
}

} // namespace

Atom normalize(AtomStore& store, AtomView atom) {
  switch (atom.tag()) {
    case AtomTag::Num:
      return store.num(atom.as_num());
    case AtomTag::Var:
      return store.var(atom.as_var());
    case AtomTag::Add:
      return normalize_add(store, atom);
    case AtomTag::Mul:
      return normalize_mul(store, atom);
    case AtomTag::Pow:
      return normalize_pow(store, atom);
    case AtomTag::Fun:
      return normalize_fun(store, atom);
  }
  return store.as_atom(atom);
}

} // namespace atom_core
