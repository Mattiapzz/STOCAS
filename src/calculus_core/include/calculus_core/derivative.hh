#pragma once

#include <functional>
#include <span>
#include <vector>

#include <atom_core/atom.hh>
#include <symbol_table/symbol.hh>

/// @file
/// @brief Symbolic differentiation (M6-T1): derivative rules per `AtomTag`,
/// plus a registry of per-function derivative hooks so user-defined
/// functions can be differentiated through (mirrors the original design's
/// "function that's called when a derivative of its arguments gets
/// taken").
///
/// Rules applied (`store` must be the same `AtomStore` @p expr came from):
///  - `Num`: derivative is `0`.
///  - `Var`: derivative is `1` if it's the differentiation variable, `0`
///    otherwise.
///  - `Add`: derivative of a sum is the sum of derivatives.
///  - `Mul`: the product rule, summed over each factor.
///  - `Pow` (`f^g`): the generalized power rule, chosen based on which of
///    `f`/`g` actually depend on the differentiation variable (their
///    derivatives are structurally `0` otherwise):
///      - neither depends on it: `0`.
///      - only `f` does: `g * f^(g-1) * f'` (the ordinary power rule).
///      - only `g` does: `f^g * ln(f) * g'` (exponential differentiation).
///      - both do: `f^g * (g' * ln(f) + g * f' / f)` (logarithmic
///        differentiation - the fully general case, needing `ln`, hence
///        `ln_symbol()`'s pre-registered hook below).
///  - `Fun` (`head(args...)`): the chain rule, `sum_i d(head)/d(arg_i) *
///    d(arg_i)/dx`, where the partials `d(head)/d(arg_i)` come from
///    `head`'s registered `DerivativeHook`. Throws `std::invalid_argument`
///    if no hook is registered for `head`.
///
/// `sin`/`cos`/`exp`/`ln` are pre-registered (see their `*_symbol()`
/// accessors) since the generalized `Pow` rule already needs `ln`, and
/// M6's series-expansion subtask needs all four.
///
/// Derivative results are built without simplification (no constant
/// folding, no like-term combination) - the same division of labor as
/// `pattern_core::substitute()`'s unsimplified rewrites; pass the result
/// through `atom_core::normalize()` to get a canonical simplified form.
namespace calculus_core {

/// @return The symbol for `sin`, pre-registered with a derivative hook
///         (`sin(x)' -> cos(x)`).
[[nodiscard]] symbol_table::Symbol sin_symbol();

/// @return The symbol for `cos`, pre-registered with a derivative hook
///         (`cos(x)' -> -sin(x)`).
[[nodiscard]] symbol_table::Symbol cos_symbol();

/// @return The symbol for `exp`, pre-registered with a derivative hook
///         (`exp(x)' -> exp(x)`).
[[nodiscard]] symbol_table::Symbol exp_symbol();

/// @return The symbol for the natural logarithm `ln`, pre-registered with
///         a derivative hook (`ln(x)' -> x^-1`). Also used internally by
///         `derivative()`'s generalized `Pow` rule.
[[nodiscard]] symbol_table::Symbol ln_symbol();

/// @brief Computes the partial derivative of a function call `head(args)`
/// with respect to each of its arguments, as an expression built in
/// @p store that may reference @p args themselves (e.g. `cos(x)` for
/// `sin`'s hook given `args = {x}`).
/// @param store The arena to build the partials in - the same store
///        @p args came from.
/// @param args The call's arguments, in order.
/// @return One partial derivative per entry of @p args, in the same order.
using DerivativeHook = std::function<std::vector<atom_core::Atom>(
    atom_core::AtomStore& store, std::span<const atom_core::AtomView> args)>;

/// @brief Registers (or overwrites) the derivative hook for function calls
/// headed by @p head, so `derivative()` can differentiate through them via
/// the chain rule. Thread-safe with respect to concurrent `derivative()`
/// calls and other `register_derivative_hook()` calls.
/// @param head The function symbol calls are matched against.
/// @param hook Computes @p head's partial derivatives given its arguments;
///        see `DerivativeHook`'s contract. Must return exactly as many
///        partials as it's given arguments.
void register_derivative_hook(symbol_table::Symbol head, DerivativeHook hook);

/// @brief Differentiates @p expr with respect to @p variable.
/// @param store The arena new (derivative) nodes are built in - must be
///        the same store @p expr came from.
/// @param expr The expression to differentiate.
/// @param variable The symbol differentiation is taken with respect to.
/// @return The (unsimplified) derivative, built in @p store.
/// @throws std::invalid_argument if @p expr contains a `Fun` call headed
///         by a symbol with no registered derivative hook.
/// @throws std::logic_error if a registered hook returns a different
///         number of partials than the arguments it was given.
[[nodiscard]] atom_core::Atom
derivative(atom_core::AtomStore& store, atom_core::AtomView expr, symbol_table::Symbol variable);

} // namespace calculus_core
