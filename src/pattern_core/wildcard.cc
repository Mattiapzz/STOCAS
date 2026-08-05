#include <string>

#include <pattern_core/wildcard.hh>

namespace pattern_core {

std::optional<WildcardKind> wildcard_kind(symbol_table::Symbol symbol) {
  std::string name = symbol_table::symbol_name(symbol);
  std::size_t trailing = 0;
  for (auto it = name.rbegin(); it != name.rend() && *it == '_'; ++it) {
    ++trailing;
  }
  if (trailing == 0) {
    return std::nullopt;
  }
  if (trailing == 1) {
    return WildcardKind::Single;
  }
  if (trailing == 2) {
    return WildcardKind::AtLeastOne;
  }
  return WildcardKind::AnyCount;
}

bool is_wildcard(symbol_table::Symbol symbol) {
  return wildcard_kind(symbol).has_value();
}

} // namespace pattern_core
