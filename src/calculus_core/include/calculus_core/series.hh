#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include <atom_core/atom.hh>
#include <calculus_core/derivative.hh>
#include <numerica_core/big_int.hh>
#include <numerica_core/dual.hh>
#include <numerica_core/hyper_dual.hh>
#include <numerica_core/rational.hh>
#include <symbol_table/symbol.hh>

/// @file
/// @brief Taylor series expansion (M6-T2), via `numerica_core::HyperDual`'s
/// forward-mode automatic differentiation rather than repeated symbolic
/// differentiation: `taylor_series<F, Order>(expr, variable, point)`
/// evaluates @p expr with @p variable bound to a `HyperDual<F, Order>`
/// centered at @p point, producing the first `Order + 1` Taylor
/// coefficients directly (no symbolic `derivative()` calls, no
/// factorial-division bookkeeping - `HyperDual` already tracks Taylor
/// coefficients, not raw derivatives).
///
/// @p expr must be built from `Num`, exactly one free `Var` (@p variable
/// itself - any other free variable throws `std::invalid_argument`,
/// since there is no value to evaluate it at), `Add`, `Mul`, `Pow`, and
/// unary `Fun` calls to `sin`/`cos`/`exp`/`ln` (`calculus_core::derivative`'s
/// four built-in hooks - see derivative.hh). Unlike `derivative()`, this
/// intentionally does **not** offer a hook registry for arbitrary
/// functions: a numeric AD evaluator needs a concrete implementation for
/// every function it evaluates, not just a symbolic partial derivative, so
/// extending it means adding a case here rather than registering a
/// callback - a deliberate, narrower scope than `derivative()`'s.
///
/// `Pow` is evaluated specially: an integer-valued `Num` exponent (either
/// sign) is computed by exact repeated squaring (correct even when the
/// base's value is `0`, e.g. `x^2` expanded around `x=0`); any other
/// exponent falls back to `exp(exponent * log(base))`.
///
/// **Taylor only, not Laurent**: this inherits `HyperDual`'s "no poles at
/// the expansion point" restriction (see hyper_dual.hh) - expanding e.g.
/// `1/x` around `x=0` throws `std::domain_error` rather than silently
/// producing `Inf`/`NaN` coefficients. Automatic pole-order detection (to
/// support genuine Laurent series) is a documented, out-of-scope gap for
/// this task.
///
/// `Num` leaves are converted to `F` via `Rational::to_double()`, so
/// results computed with `F = numerica_core::Float` are still limited to
/// double precision for their input constants (a documented, honest
/// simplification - `F64` is the primary intended scalar type here).
namespace calculus_core {

/// @brief A truncated Taylor series of some expression around `point()`,
/// with coefficients in `F` (see `taylor_series()`).
template <numerica_core::TrigDualScalar F, std::size_t Order> class Series {
public:
  Series(F point, std::array<F, Order + 1> coefficients)
    : point_(std::move(point)), coefficients_(std::move(coefficients)) {}

  /// @return The truncation order (the series is exact through
  ///         `(x - point())^Order`; the implicit remainder is
  ///         `O((x - point())^(Order + 1))`).
  [[nodiscard]] static constexpr std::size_t order() noexcept { return Order; }

  [[nodiscard]] const F& point() const noexcept { return point_; }

  /// @param k Which coefficient, `0 <= k <= order()`.
  /// @return The coefficient of `(x - point())^k`.
  [[nodiscard]] const F& coefficient(std::size_t k) const { return coefficients_.at(k); }

  /// @brief Evaluates the truncated series `sum_k coefficient(k) * (x -
  /// point())^k` at @p x, via Horner's method.
  [[nodiscard]] F evaluate(const F& x) const {
    const F delta = x - point_;
    F result = coefficients_[Order];
    for (std::size_t k = Order; k-- > 0;) {
      result = result * delta + coefficients_[k];
    }
    return result;
  }

private:
  F point_;
  std::array<F, Order + 1> coefficients_;
};

namespace detail {

// Exact integer power via repeated squaring - unlike exp(exponent *
// log(base)), this is correct even when base's constant term is 0 (e.g.
// x^2 expanded around x=0). Negative exponents go through
// HyperDual::operator/ (still pole-checked - see hyper_dual.hh).
template <numerica_core::TrigDualScalar F, std::size_t Order>
[[nodiscard]] numerica_core::HyperDual<F, Order>
integer_power(const numerica_core::HyperDual<F, Order>& base, numerica_core::BigInt exponent) {
  using HD = numerica_core::HyperDual<F, Order>;
  const bool negative = exponent < numerica_core::BigInt(0);
  numerica_core::BigInt magnitude = negative ? -exponent : exponent;

  HD result = HD::constant(F(1.0));
  HD power = base;
  while (magnitude > numerica_core::BigInt(0)) {
    if (magnitude % numerica_core::BigInt(2) == numerica_core::BigInt(1)) {
      result = result * power;
    }
    power = power * power;
    magnitude = magnitude / numerica_core::BigInt(2);
  }
  return negative ? (HD::constant(F(1.0)) / result) : result;
}

template <numerica_core::TrigDualScalar F, std::size_t Order>
[[nodiscard]] numerica_core::HyperDual<F, Order>
evaluate_hyper_dual(atom_core::AtomView expr,
                    symbol_table::Symbol variable,
                    const numerica_core::HyperDual<F, Order>& variable_value) {
  using HD = numerica_core::HyperDual<F, Order>;
  switch (expr.tag()) {
    case atom_core::AtomTag::Num:
      return HD::constant(F(expr.as_num().to_double()));
    case atom_core::AtomTag::Var:
      if (expr.as_var() == variable) {
        return variable_value;
      }
      throw std::invalid_argument(
          "calculus_core::taylor_series: expression contains a free variable other "
          "than the expansion variable ('" +
          symbol_table::symbol_name(expr.as_var()) + "')");
    case atom_core::AtomTag::Add: {
      HD sum = HD::constant(F(0.0));
      for (std::size_t i = 0; i < expr.child_count(); ++i) {
        sum = sum + evaluate_hyper_dual(expr.child(i), variable, variable_value);
      }
      return sum;
    }
    case atom_core::AtomTag::Mul: {
      HD product = HD::constant(F(1.0));
      for (std::size_t i = 0; i < expr.child_count(); ++i) {
        product = product * evaluate_hyper_dual(expr.child(i), variable, variable_value);
      }
      return product;
    }
    case atom_core::AtomTag::Pow: {
      const atom_core::AtomView base = expr.child(0);
      const atom_core::AtomView exponent = expr.child(1);
      const HD base_hd = evaluate_hyper_dual(base, variable, variable_value);
      if (exponent.tag() == atom_core::AtomTag::Num &&
          exponent.as_num().denominator() == numerica_core::BigInt(1)) {
        return integer_power(base_hd, exponent.as_num().numerator());
      }
      const HD exponent_hd = evaluate_hyper_dual(exponent, variable, variable_value);
      return HD::exp(exponent_hd * HD::log(base_hd));
    }
    case atom_core::AtomTag::Fun: {
      const symbol_table::Symbol head = expr.function_head();
      if (expr.child_count() != 1) {
        throw std::invalid_argument(
            "calculus_core::taylor_series: only unary functions are supported ('" +
            symbol_table::symbol_name(head) + "' has " + std::to_string(expr.child_count()) +
            " argument(s))");
      }
      const HD arg = evaluate_hyper_dual(expr.child(0), variable, variable_value);
      if (head == sin_symbol()) {
        return HD::sin(arg);
      }
      if (head == cos_symbol()) {
        return HD::cos(arg);
      }
      if (head == exp_symbol()) {
        return HD::exp(arg);
      }
      if (head == ln_symbol()) {
        return HD::log(arg);
      }
      throw std::invalid_argument(
          "calculus_core::taylor_series: no series-expansion support for function '" +
          symbol_table::symbol_name(head) + "' (only sin/cos/exp/ln are supported)");
    }
  }
  return HD::constant(F(0.0)); // unreachable, silences -Wreturn-type
}

} // namespace detail

/// @brief Expands @p expr in a Taylor series around @p variable == @p
/// point, through order `Order`, via forward-mode automatic
/// differentiation. See this file's comment for the full contract
/// (supported node/function shapes, the integer-`Pow` special case, and
/// the Taylor-only/no-Laurent limitation).
/// @tparam F The scalar type coefficients are computed in (typically
///           `numerica_core::F64`).
/// @tparam Order The truncation order (`Order + 1` coefficients are
///           computed, for `(x - point)^0` through `(x - point)^Order`).
/// @param expr The expression to expand.
/// @param variable The symbol the expansion is taken with respect to.
/// @param point The expansion point.
/// @throws std::invalid_argument if @p expr contains a free variable other
///         than @p variable, a `Fun` call to an unsupported function, or a
///         `Fun` call with other than one argument.
/// @throws std::domain_error if evaluation hits a pole (division by a
///         zero-constant-term `HyperDual`) or a `log` of a non-positive
///         constant term - see `numerica_core::HyperDual`'s file comment.
template <numerica_core::TrigDualScalar F, std::size_t Order>
[[nodiscard]] Series<F, Order>
taylor_series(atom_core::AtomView expr, symbol_table::Symbol variable, F point) {
  const numerica_core::HyperDual<F, Order> variable_value =
      numerica_core::HyperDual<F, Order>::variable(point);
  const numerica_core::HyperDual<F, Order> result =
      detail::evaluate_hyper_dual(expr, variable, variable_value);

  std::array<F, Order + 1> coefficients{};
  for (std::size_t k = 0; k <= Order; ++k) {
    coefficients[k] = result.coefficient(k);
  }
  return Series<F, Order>(std::move(point), std::move(coefficients));
}

} // namespace calculus_core
