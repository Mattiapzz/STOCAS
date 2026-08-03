#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <numerica_core/rational.hh>
#include <symbol_table/symbol.hh>

/// @file
/// @brief `Atom` expression representation (M3-T1), implementing the
/// variant-of-arena-offsets decision recorded by the M3-T0 spike
/// (docs/atom-representation-spike.md): composite nodes reference their
/// children by index into an `AtomStore`'s arena, never by pointer.
namespace atom_core {

/// @brief Discriminates the kind of expression node an `AtomView`/`Atom`
/// refers to.
enum class AtomTag : std::uint8_t { Num, Var, Add, Mul, Pow, Fun };

class AtomStore;

/// @brief Non-owning, read-only view of a node stored in an `AtomStore`'s
/// arena, identified by an arena index rather than a pointer. Cheap to
/// copy (store pointer + index), used everywhere ownership isn't needed.
///
/// Any two `AtomView`s obtained from the same `AtomStore` compare equal
/// (`operator==`) if and only if they reference the same arena slot,
/// because every builder method on `AtomStore` interns (hash-conses):
/// structurally identical subexpressions always resolve to one shared slot.
class AtomView {
public:
  /// @return The kind of node this view refers to.
  [[nodiscard]] AtomTag tag() const;

  /// @throws std::logic_error if tag() != AtomTag::Num.
  [[nodiscard]] const numerica_core::Rational& as_num() const;

  /// @throws std::logic_error if tag() != AtomTag::Var.
  [[nodiscard]] symbol_table::Symbol as_var() const;

  /// @throws std::logic_error if tag() != AtomTag::Fun.
  [[nodiscard]] symbol_table::Symbol function_head() const;

  /// @return Child count for Add/Mul/Pow/Fun; 0 for Num/Var.
  [[nodiscard]] std::size_t child_count() const;

  /// @throws std::out_of_range if @p index >= child_count().
  [[nodiscard]] AtomView child(std::size_t index) const;

  /// @return true iff @p other is structurally equal to this atom (same
  ///         tag, same payload, and structurally equal children).
  [[nodiscard]] bool operator==(const AtomView& other) const;

  /// @brief A strict total order over all `Atom`/`AtomView` values,
  /// regardless of which `AtomStore` they came from: orders first by
  /// AtomTag, then by payload (numeric value / symbol id / function head),
  /// then lexicographically by children. Used to canonicalize Add/Mul
  /// operand order (commutative flattening) during construction.
  [[nodiscard]] std::strong_ordering operator<=>(const AtomView& other) const;

  /// @brief A hash consistent with operator==: two views compare equal iff
  /// they hash equal AND compare bit-for-bit equal via the (slower) exact
  /// check `AtomStore` builders use internally for hash-consing.
  [[nodiscard]] std::size_t structural_hash() const;

  /// @return true iff @p other refers to literally the same arena slot in
  ///         the same store - a stronger, O(1) check than operator==
  ///         (which is a full structural comparison), useful for asserting
  ///         that hash-consing actually deduplicated two builder calls.
  [[nodiscard]] bool same_slot(const AtomView& other) const noexcept;

private:
  friend class AtomStore;
  friend class Atom;

  AtomView(const AtomStore* store, std::uint32_t index) noexcept : store_(store), index_(index) {}

  const AtomStore* store_;
  std::uint32_t index_;
};

/// @brief An owning handle to an interned node in an `AtomStore`. Identical
/// in representation to `AtomView` (both are a store pointer + arena
/// index - "ownership" here means "backed by the arena for its lifetime",
/// not "individually freed"; the arena as a whole is torn down with its
/// `AtomStore`). Returned by `AtomStore`'s builder methods; implicitly
/// convertible to `AtomView` for passing to read-only APIs.
class Atom {
public:
  /// @return A non-owning view of this atom.
  [[nodiscard]] AtomView view() const noexcept { return AtomView(store_, index_); }

  /// @brief Implicit conversion to `AtomView`, for passing an `Atom`
  /// directly to APIs that take a read-only view.
  operator AtomView() const noexcept { return view(); } // NOLINT(google-explicit-constructor)

  /// @return true iff @p other is structurally equal to this atom (see
  ///         `AtomView::operator==`).
  [[nodiscard]] bool operator==(const Atom& other) const { return view() == other.view(); }

private:
  friend class AtomStore;

  Atom(const AtomStore* store, std::uint32_t index) noexcept : store_(store), index_(index) {}

  const AtomStore* store_;
  std::uint32_t index_;
};

/// @brief Owning arena of interned `Atom` nodes: a contiguous, index-stable
/// node vector plus a CSR-style child-index pool (the production version of
/// `benchmarks/atom_core_spike/spike_bench.cc`'s `offsets_repr` prototype).
///
/// All builder methods hash-cons: constructing a structurally identical
/// subexpression twice returns the same arena index both times (see
/// `AtomView::same_slot`).
class AtomStore {
public:
  AtomStore();
  ~AtomStore();
  AtomStore(const AtomStore&) = delete;
  AtomStore& operator=(const AtomStore&) = delete;
  AtomStore(AtomStore&&) = delete;
  AtomStore& operator=(AtomStore&&) = delete;

  /// @brief Interns a numeric leaf.
  /// @param value The numeric value.
  /// @return The (possibly pre-existing, hash-consed) atom for @p value.
  [[nodiscard]] Atom num(numerica_core::Rational value);

  /// @brief Interns a variable leaf.
  /// @param symbol The variable's symbol.
  /// @return The (possibly pre-existing, hash-consed) atom for @p symbol.
  [[nodiscard]] Atom var(symbol_table::Symbol symbol);

  /// @brief Builds a canonicalized (sorted, one level flattened) sum.
  /// Nested Add terms among @p terms are spliced into the result rather
  /// than nested, and terms are sorted by AtomView's canonical order.
  [[nodiscard]] Atom add(std::span<const Atom> terms);

  /// @brief Same canonicalization as add(), for multiplication.
  [[nodiscard]] Atom mul(std::span<const Atom> factors);

  /// @brief Builds `base^exponent`. Base and exponent order is preserved
  /// (exponentiation is not commutative).
  /// @param base The base.
  /// @param exponent The exponent.
  /// @return The (possibly pre-existing, hash-consed) power atom.
  [[nodiscard]] Atom pow(Atom base, Atom exponent);

  /// @brief Builds a function call. Argument order is preserved.
  /// @param head The function's symbol.
  /// @param args The call's arguments, in order.
  /// @return The (possibly pre-existing, hash-consed) call atom.
  [[nodiscard]] Atom fun(symbol_table::Symbol head, std::span<const Atom> args);

  /// @return Node count currently held (for tests/diagnostics only).
  [[nodiscard]] std::size_t node_count() const noexcept;

  /// @brief Recovers an owning `Atom` handle for a view into this same
  /// store (e.g. a child obtained via `AtomView::child()`) - safe because
  /// every node an `AtomView` can reference is already interned in this
  /// store's arena for the store's lifetime; this performs no new
  /// allocation or interning, just reconstructs the handle.
  [[nodiscard]] Atom as_atom(AtomView view) const noexcept;

private:
  friend class AtomView;

  struct Impl;
  std::unique_ptr<Impl> impl_;

  [[nodiscard]] std::vector<std::uint32_t> flatten_operands(std::span<const Atom> operands,
                                                            AtomTag flatten_tag);
  [[nodiscard]] Atom build_variadic(AtomTag tag, std::span<const Atom> operands);
};

} // namespace atom_core
