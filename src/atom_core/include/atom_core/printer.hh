#pragma once

#include <string>

#include <atom_core/atom.hh>

/// @file
/// @brief Canonical string-form printer for `Atom` expressions (M3-T3).
namespace atom_core {

/// @brief Prints @p atom to a canonical string form parseable by
/// atom_core::parse() with the same @p namespace_name used to build it,
/// reproducing a structurally equal atom (round-trip guarantee):
/// `parse(print(atom), store, ns)` is structurally equal to `atom` whenever
/// `atom` was itself built via the public `AtomStore`/parser API in @p ns.
///
/// Parentheses are inserted wherever needed for that guarantee to hold
/// (not necessarily the minimal set a human would write).
///
/// @param atom The atom to print.
/// @return The canonical string form.
[[nodiscard]] std::string print(AtomView atom);

} // namespace atom_core
