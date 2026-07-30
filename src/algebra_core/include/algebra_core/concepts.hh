#pragma once

#include <concepts>
#include <utility>

/// @file
/// @brief Ring-hierarchy concepts: Semiring -> Ring -> EuclideanDomain ->
/// Field, mirroring the Rust trait chain Set -> Ring -> EuclideanDomain ->
/// Field from the original design.
///
/// HARD RULE (coherence-risk mitigation, see roadmap.md's risk register):
/// types satisfying these concepts must implement the required operations
/// as member functions of a dedicated ring-struct type (e.g. `IntegerRing`,
/// `FiniteFieldRing<T>`), never as free functions found via ADL on a
/// foreign type. Unlike Rust's trait coherence rules, nothing stops two
/// unrelated headers from both defining an ADL `add`/`mul` for the same
/// third-party type with conflicting semantics; requiring ring operations
/// to live on a type this project owns sidesteps that hazard entirely.
namespace algebra_core {

/// @brief A semiring: an `Element` type with associative, commutative
/// addition and multiplication, an additive identity (`zero`), and a
/// multiplicative identity (`one`). No subtraction or division is assumed.
/// `Element` must be equality-comparable - generic algorithms (e.g. `gcd`,
/// see generic_algorithms.hh) need to test elements against `zero()`.
template <typename R>
concept Semiring =
    requires(const R& ring, const typename R::Element& a, const typename R::Element& b) {
      typename R::Element;
      { ring.zero() } -> std::same_as<typename R::Element>;
      { ring.one() } -> std::same_as<typename R::Element>;
      { ring.add(a, b) } -> std::same_as<typename R::Element>;
      { ring.mul(a, b) } -> std::same_as<typename R::Element>;
      { a == b } -> std::convertible_to<bool>;
    };

/// @brief A ring: a semiring with additive inverses, i.e. subtraction and
/// negation are always defined.
template <typename R>
concept Ring = Semiring<R> &&
               requires(const R& ring, const typename R::Element& a, const typename R::Element& b) {
                 { ring.sub(a, b) } -> std::same_as<typename R::Element>;
                 { ring.neg(a) } -> std::same_as<typename R::Element>;
               };

/// @brief A Euclidean domain: a ring with division-with-remainder, the
/// operation `gcd`/`extended_gcd` are built from generically (see
/// generic_algorithms.hh, M2-T2). `div_rem` returns `{quotient, remainder}`.
template <typename R>
concept EuclideanDomain =
    Ring<R> && requires(const R& ring, const typename R::Element& a, const typename R::Element& b) {
      { ring.div_rem(a, b) } -> std::same_as<std::pair<typename R::Element, typename R::Element>>;
    };

/// @brief A field: a ring in which every nonzero element has a
/// multiplicative inverse (`inv`). Division is `a * ring.inv(b)`.
template <typename R>
concept Field = Ring<R> && requires(const R& ring, const typename R::Element& a) {
  { ring.inv(a) } -> std::same_as<typename R::Element>;
};

} // namespace algebra_core
