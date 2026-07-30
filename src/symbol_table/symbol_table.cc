#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <symbol_table/symbol.hh>

namespace symbol_table {

namespace {

struct Entry {
  std::string qualified_name;
  std::vector<SymbolAttribute> attributes;
  std::source_location location;
};

// Function-local static: guaranteed thread-safe *initialization* by the
// standard, but concurrent get_symbol() calls are NOT yet safe against each
// other (no lock) - that's M2-T4's job. Single-threaded use only until then.
struct Table {
  std::unordered_map<std::string, std::size_t> index_by_qualified_name;
  std::vector<Entry> entries;
};

Table& table() {
  static Table instance;
  return instance;
}

std::string qualify(std::string_view namespace_name, std::string_view name) {
  std::string qualified;
  qualified.reserve(namespace_name.size() + 2 + name.size());
  qualified += namespace_name;
  qualified += "::";
  qualified += name;
  return qualified;
}

bool same_attributes(std::span<const SymbolAttribute> requested,
                     const std::vector<SymbolAttribute>& existing) {
  if (requested.size() != existing.size()) {
    return false;
  }
  std::vector<SymbolAttribute> sorted_requested(requested.begin(), requested.end());
  std::vector<SymbolAttribute> sorted_existing = existing;
  const auto by_value = [](SymbolAttribute lhs, SymbolAttribute rhs) {
    return static_cast<int>(lhs) < static_cast<int>(rhs);
  };
  std::sort(sorted_requested.begin(), sorted_requested.end(), by_value);
  std::sort(sorted_existing.begin(), sorted_existing.end(), by_value);
  return sorted_requested == sorted_existing;
}

} // namespace

Symbol get_symbol(std::string_view namespace_name,
                  std::string_view name,
                  std::span<const SymbolAttribute> attributes,
                  std::source_location location) {
  Table& global_table = table();
  const std::string qualified = qualify(namespace_name, name);

  const auto it = global_table.index_by_qualified_name.find(qualified);
  if (it != global_table.index_by_qualified_name.end()) {
    const Entry& existing = global_table.entries[it->second];
    if (!same_attributes(attributes, existing.attributes)) {
      throw std::logic_error("symbol_table: '" + qualified +
                             "' redefined with conflicting attributes (originally registered at " +
                             existing.location.file_name() + ":" +
                             std::to_string(existing.location.line()) + ")");
    }
    return Symbol(it->second);
  }

  const std::size_t id = global_table.entries.size();
  global_table.entries.push_back(Entry{
      qualified, std::vector<SymbolAttribute>(attributes.begin(), attributes.end()), location});
  global_table.index_by_qualified_name.emplace(qualified, id);
  return Symbol(id);
}

std::string symbol_name(Symbol symbol) {
  return table().entries.at(symbol.id()).qualified_name;
}

} // namespace symbol_table
