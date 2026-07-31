#pragma once

#include <string_view>

#include <atom_core/atom.hh>

/// @file
/// @brief Recursive-descent / precedence-climbing parser for `Atom`
/// expressions (M3-T2).
namespace atom_core {

/// @brief Parses @p expression into an `Atom` built via @p store.
///
/// Grammar (highest to lowest binding power): `^` (right-assoc), unary `-`,
/// `*` / `/` / implicit multiplication (`2x`, `x(y+1)`) (left-assoc), binary
/// `+` / `-` (left-assoc). Function calls (`f(x, y)`) and parenthesized
/// sub-expressions are handled as primaries. Integer literals only (no
/// decimal/scientific notation yet).
///
/// Every identifier encountered (variable or function head) is registered
/// via `symbol_table::get_symbol(namespace_name, name)` - new names
/// auto-register in @p namespace_name, existing ones resolve to their
/// already-registered handle.
///
/// @param expression The source text to parse.
/// @param store The arena new nodes are built in.
/// @param namespace_name The symbol_table namespace identifiers resolve
///        into (see symbol_table::get_symbol).
/// @throws std::invalid_argument on any syntax error (unexpected token,
///         unbalanced parentheses, empty expression, trailing input, an
///         empty numeric literal, etc.) - the message includes the byte
///         offset of the offending token.
[[nodiscard]] Atom
parse(std::string_view expression, AtomStore& store, std::string_view namespace_name);

} // namespace atom_core
