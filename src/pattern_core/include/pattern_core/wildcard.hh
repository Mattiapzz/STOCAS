#pragma once

#include <optional>

#include <symbol_table/symbol.hh>

/// @file
/// @brief Wildcard symbol convention (M5-T1, extended M5-T2): a symbol is a
/// wildcard iff its registered name ends in one or more `_`; the trailing
/// run length picks the kind, mirroring the original design's convention:
///   - `x_`   (one trailing `_`)   - WildcardKind::Single: matches exactly
///     one atom.
///   - `x__`  (two trailing `_`)   - WildcardKind::AtLeastOne: matches one
///     or more sibling atoms in an Add/Mul/Fun argument list.
///   - `x___` (three or more `_`)  - WildcardKind::AnyCount: matches zero
///     or more sibling atoms in an Add/Mul/Fun argument list.
/// Matching against wildcards lives in matcher.hh; this header only
/// classifies a given symbol.
namespace pattern_core {

/// @brief The three wildcard flavors, distinguished by trailing-underscore
/// count (see the file comment).
enum class WildcardKind { Single, AtLeastOne, AnyCount };

/// @brief Classifies @p symbol as a wildcard, or not.
/// @param symbol The symbol to check.
/// @return The wildcard kind if @p symbol's name ends in `_`, else
///         `std::nullopt`.
[[nodiscard]] std::optional<WildcardKind> wildcard_kind(symbol_table::Symbol symbol);

/// @return true iff @p symbol is a wildcard of any kind (equivalent to
///         `wildcard_kind(symbol).has_value()`).
[[nodiscard]] bool is_wildcard(symbol_table::Symbol symbol);

} // namespace pattern_core
