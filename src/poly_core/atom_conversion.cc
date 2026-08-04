#include <stdexcept>
#include <string>
#include <vector>

#include <numerica_core/big_int.hh>
#include <numerica_core/rational.hh>
#include <poly_core/atom_conversion.hh>
#include <poly_core/monomial.hh>

namespace poly_core {

namespace {

using algebra_core::RationalField;
using atom_core::AtomStore;
using atom_core::AtomTag;
using atom_core::AtomView;
using numerica_core::BigInt;
using numerica_core::Rational;

std::size_t find_variable(AtomView var_atom, const std::vector<symbol_table::Symbol>& variables) {
  symbol_table::Symbol symbol = var_atom.as_var();
  for (std::size_t i = 0; i < variables.size(); ++i) {
    if (variables[i] == symbol) {
      return i;
    }
  }
  throw std::invalid_argument("poly_core: atom references a variable not present in the given "
                              "variable list");
}

// atom.as_num() is assumed to already be an integer (denominator == 1) and
// non-negative; the caller validates both before calling this. BigInt
// exposes no direct accessor into a native integer type (only to_string()),
// same rationale as multivariate_gcd.hh's bigint_to_u64().
Exponent bigint_to_exponent(const BigInt& value) {
  return static_cast<Exponent>(std::stoul(value.to_string()));
}

MultivariatePolynomial<RationalField> convert_polynomial(
    AtomView atom, const RationalField& field, const std::vector<symbol_table::Symbol>& variables) {
  switch (atom.tag()) {
    case AtomTag::Num:
      return MultivariatePolynomial<RationalField>::constant(field, variables, atom.as_num());
    case AtomTag::Var: {
      std::size_t index = find_variable(atom, variables);
      std::vector<Exponent> exponents(variables.size(), 0);
      exponents[index] = 1;
      return MultivariatePolynomial<RationalField>::from_term(
          field, variables, Monomial(std::move(exponents)), Rational(1));
    }
    case AtomTag::Add: {
      MultivariatePolynomial<RationalField> result(field, variables);
      for (std::size_t i = 0; i < atom.child_count(); ++i) {
        result = result + convert_polynomial(atom.child(i), field, variables);
      }
      return result;
    }
    case AtomTag::Mul: {
      MultivariatePolynomial<RationalField> result =
          MultivariatePolynomial<RationalField>::constant(field, variables, Rational(1));
      for (std::size_t i = 0; i < atom.child_count(); ++i) {
        result = result * convert_polynomial(atom.child(i), field, variables);
      }
      return result;
    }
    case AtomTag::Pow: {
      MultivariatePolynomial<RationalField> base =
          convert_polynomial(atom.child(0), field, variables);
      AtomView exponent_atom = atom.child(1);
      if (exponent_atom.tag() != AtomTag::Num ||
          !(exponent_atom.as_num().denominator() == BigInt(1))) {
        throw std::invalid_argument(
            "poly_core::atom_to_polynomial: Pow exponent must be an integer Num");
      }
      const BigInt& exponent_value = exponent_atom.as_num().numerator();
      if (exponent_value < BigInt(0)) {
        throw std::invalid_argument("poly_core::atom_to_polynomial: negative exponent - use "
                                    "atom_to_rational_polynomial instead");
      }
      Exponent exponent = bigint_to_exponent(exponent_value);
      MultivariatePolynomial<RationalField> result =
          MultivariatePolynomial<RationalField>::constant(field, variables, Rational(1));
      for (Exponent i = 0; i < exponent; ++i) {
        result = result * base;
      }
      return result;
    }
    case AtomTag::Fun:
      throw std::invalid_argument(
          "poly_core::atom_to_polynomial: function calls are not representable as a polynomial");
  }
  throw std::invalid_argument("poly_core::atom_to_polynomial: unhandled AtomTag");
}

RationalPolynomial convert_rational(AtomView atom,
                                    const RationalField& field,
                                    const std::vector<symbol_table::Symbol>& variables) {
  switch (atom.tag()) {
    case AtomTag::Num:
      return RationalPolynomial::from_polynomial(
          field, MultivariatePolynomial<RationalField>::constant(field, variables, atom.as_num()));
    case AtomTag::Var: {
      std::size_t index = find_variable(atom, variables);
      std::vector<Exponent> exponents(variables.size(), 0);
      exponents[index] = 1;
      return RationalPolynomial::from_polynomial(
          field,
          MultivariatePolynomial<RationalField>::from_term(
              field, variables, Monomial(std::move(exponents)), Rational(1)));
    }
    case AtomTag::Add: {
      // Recurses into convert_rational() (not convert_polynomial()) for
      // each child: a negative exponent can appear anywhere in the tree
      // (e.g. `1 + 1/x` is Add(1, Pow(x,-1))), not just at the top level.
      RationalPolynomial result = RationalPolynomial::from_polynomial(
          field, MultivariatePolynomial<RationalField>(field, variables));
      for (std::size_t i = 0; i < atom.child_count(); ++i) {
        result = result + convert_rational(atom.child(i), field, variables);
      }
      return result;
    }
    case AtomTag::Mul: {
      RationalPolynomial result = RationalPolynomial::from_polynomial(
          field, MultivariatePolynomial<RationalField>::constant(field, variables, Rational(1)));
      for (std::size_t i = 0; i < atom.child_count(); ++i) {
        result = result * convert_rational(atom.child(i), field, variables);
      }
      return result;
    }
    case AtomTag::Pow: {
      RationalPolynomial base = convert_rational(atom.child(0), field, variables);
      AtomView exponent_atom = atom.child(1);
      if (exponent_atom.tag() != AtomTag::Num ||
          !(exponent_atom.as_num().denominator() == BigInt(1))) {
        throw std::invalid_argument(
            "poly_core::atom_to_rational_polynomial: Pow exponent must be an integer Num");
      }
      const BigInt& exponent_value = exponent_atom.as_num().numerator();
      bool negative = exponent_value < BigInt(0);
      BigInt magnitude = negative ? -exponent_value : exponent_value;
      Exponent exponent = bigint_to_exponent(magnitude);

      RationalPolynomial result = RationalPolynomial::from_polynomial(
          field, MultivariatePolynomial<RationalField>::constant(field, variables, Rational(1)));
      for (Exponent i = 0; i < exponent; ++i) {
        result = result * base;
      }
      if (negative) {
        if (result.is_zero()) {
          throw std::domain_error(
              "poly_core::atom_to_rational_polynomial: negative exponent applied to a zero base");
        }
        result = RationalPolynomial(field, result.denominator(), result.numerator());
      }
      return result;
    }
    case AtomTag::Fun:
      throw std::invalid_argument("poly_core::atom_to_rational_polynomial: function calls are not "
                                  "representable as a polynomial");
  }
  throw std::invalid_argument("poly_core::atom_to_rational_polynomial: unhandled AtomTag");
}

} // namespace

MultivariatePolynomial<algebra_core::RationalField>
atom_to_polynomial(atom_core::AtomView atom,
                   const algebra_core::RationalField& field,
                   const std::vector<symbol_table::Symbol>& variables) {
  return convert_polynomial(atom, field, variables);
}

RationalPolynomial atom_to_rational_polynomial(atom_core::AtomView atom,
                                               const algebra_core::RationalField& field,
                                               const std::vector<symbol_table::Symbol>& variables) {
  return convert_rational(atom, field, variables);
}

atom_core::Atom
polynomial_to_atom(const MultivariatePolynomial<algebra_core::RationalField>& polynomial,
                   atom_core::AtomStore& store) {
  if (polynomial.is_zero()) {
    return store.num(Rational(0));
  }
  const std::vector<symbol_table::Symbol>& variables = polynomial.variables();
  std::vector<atom_core::Atom> term_atoms;
  term_atoms.reserve(polynomial.num_terms());

  for (const auto& term : polynomial.terms()) {
    const Monomial& monomial = term.first;
    const Rational& coeff = term.second;

    std::vector<atom_core::Atom> factors;
    for (std::size_t i = 0; i < variables.size(); ++i) {
      Exponent exponent = monomial.exponent(i);
      if (exponent == 0) {
        continue;
      }
      atom_core::Atom var_atom = store.var(variables[i]);
      if (exponent == 1) {
        factors.push_back(var_atom);
      } else {
        factors.push_back(
            store.pow(var_atom, store.num(Rational(BigInt(static_cast<int64_t>(exponent))))));
      }
    }
    if (factors.empty() || !(coeff == Rational(1))) {
      factors.insert(factors.begin(), store.num(coeff));
    }
    term_atoms.push_back(factors.size() == 1 ? factors[0] : store.mul(factors));
  }
  return term_atoms.size() == 1 ? term_atoms[0] : store.add(term_atoms);
}

atom_core::Atom rational_polynomial_to_atom(const RationalPolynomial& polynomial,
                                            atom_core::AtomStore& store) {
  atom_core::Atom numerator_atom = polynomial_to_atom(polynomial.numerator(), store);

  const MultivariatePolynomial<algebra_core::RationalField>& denominator = polynomial.denominator();
  bool denominator_is_one = denominator.num_terms() == 1 &&
                            denominator.leading_term().first.total_degree() == 0 &&
                            denominator.leading_term().second == Rational(1);
  if (denominator_is_one) {
    return numerator_atom;
  }

  atom_core::Atom denominator_atom = polynomial_to_atom(denominator, store);
  atom_core::Atom inv_denominator_atom = store.pow(denominator_atom, store.num(Rational(-1)));
  std::vector<atom_core::Atom> factors{numerator_atom, inv_denominator_atom};
  return store.mul(factors);
}

} // namespace poly_core
