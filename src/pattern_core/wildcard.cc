#include <string>

#include <pattern_core/wildcard.hh>

namespace pattern_core {

bool is_wildcard(symbol_table::Symbol symbol) {
  std::string name = symbol_table::symbol_name(symbol);
  return !name.empty() && name.back() == '_';
}

} // namespace pattern_core
