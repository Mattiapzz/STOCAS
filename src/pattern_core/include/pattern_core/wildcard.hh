#pragma once

#include <symbol_table/symbol.hh>

/// @file
/// @brief Wildcard symbol convention (M5-T1): a symbol is a wildcard iff its
/// registered name ends in `_` (e.g. `x_`), mirroring the original design's
/// convention. Matching against wildcards lives in matcher.hh; this header
/// only decides whether a given symbol counts as one.
namespace pattern_core {

/// @brief Whether @p symbol is a wildcard, i.e. its name (as returned by
/// `symbol_table::symbol_name()`) ends in `_`.
/// @param symbol The symbol to check.
/// @return true iff @p symbol's name ends in `_`.
[[nodiscard]] bool is_wildcard(symbol_table::Symbol symbol);

} // namespace pattern_core
