#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
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

// An immutable, fully self-contained view of the table. Never mutated once
// published via GlobalTable::snapshot_ - every insert builds a *new*
// Snapshot (copying the previous one) and atomically swaps the pointer, per
// the concurrency contract documented on GlobalTable below.
struct Snapshot {
  std::vector<Entry> entries;
  std::unordered_map<std::string, std::size_t> index_by_qualified_name;
};

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

[[noreturn]] void throw_conflict(const std::string& qualified, const Entry& existing) {
  throw std::logic_error("symbol_table: '" + qualified +
                         "' redefined with conflicting attributes (originally registered at " +
                         existing.location.file_name() + ":" +
                         std::to_string(existing.location.line()) + ")");
}

// Process-wide singleton backing every get_symbol()/symbol_name() call.
//
// Concurrency contract (M2-T4, the compensating control for losing Rust's
// borrow-checker-enforced race freedom - see roadmap.md's risk register):
//   - Reads (looking up an already-registered symbol, and symbol_name())
//     are lock-free/wait-free: they atomically load the current Snapshot
//     pointer (acquire) and only ever read from that immutable object,
//     never touching write_mutex_.
//   - Writes (registering a new symbol) are serialized against each other
//     by write_mutex_: a writer copies the current Snapshot, appends the
//     new entry to the copy, then atomically publishes it (release) via a
//     single store to snapshot_. Readers concurrent with this either see
//     the old Snapshot or the fully-formed new one - never a partial one.
//   - Old Snapshots are intentionally never freed after being superseded:
//     a concurrent reader may still hold a raw pointer to one across the
//     swap, so freeing it would be a use-after-free. Symbols are
//     process-lifetime and low-cardinality (at most a few thousand
//     distinct names in realistic use), making this bounded, documented
//     leak a deliberate simplification instead of hazard-pointer/
//     epoch-based reclamation (which roadmap.md's task allows as the
//     alternative to atomic snapshot swap - this file picks the latter).
class GlobalTable {
public:
  static GlobalTable& instance() {
    static GlobalTable table;
    return table;
  }

  GlobalTable(const GlobalTable&) = delete;
  GlobalTable& operator=(const GlobalTable&) = delete;

  [[nodiscard]] const Snapshot& read() const noexcept {
    return *snapshot_.load(std::memory_order_acquire);
  }

  // Registers `qualified` if absent, or validates+returns the existing id.
  // Takes write_mutex_ - only reached after a lock-free read already
  // failed to find the entry, so the common (already-registered) case
  // never pays this cost.
  [[nodiscard]] std::size_t insert_or_validate(const std::string& qualified,
                                               std::span<const SymbolAttribute> attributes,
                                               const std::source_location& location) {
    const std::lock_guard<std::mutex> lock(write_mutex_);

    // Re-check under the lock: another writer may have registered this
    // exact name during the race to acquire write_mutex_.
    const Snapshot* current = snapshot_.load(std::memory_order_relaxed);
    const auto it = current->index_by_qualified_name.find(qualified);
    if (it != current->index_by_qualified_name.end()) {
      if (!same_attributes(attributes, current->entries[it->second].attributes)) {
        throw_conflict(qualified, current->entries[it->second]);
      }
      return it->second;
    }

    auto next = std::make_unique<Snapshot>(*current);
    const std::size_t id = next->entries.size();
    next->entries.push_back(Entry{
        qualified, std::vector<SymbolAttribute>(attributes.begin(), attributes.end()), location});
    next->index_by_qualified_name.emplace(qualified, id);

    snapshot_.store(next.release(), std::memory_order_release);
    return id; // `current` is deliberately leaked - see the class comment.
  }

private:
  GlobalTable() : snapshot_(new Snapshot{}) {}

  std::atomic<const Snapshot*> snapshot_;
  std::mutex write_mutex_;
};

std::string qualify(std::string_view namespace_name, std::string_view name) {
  std::string qualified;
  qualified.reserve(namespace_name.size() + 2 + name.size());
  qualified += namespace_name;
  qualified += "::";
  qualified += name;
  return qualified;
}

} // namespace

Symbol get_symbol(std::string_view namespace_name,
                  std::string_view name,
                  std::span<const SymbolAttribute> attributes,
                  std::source_location location) {
  GlobalTable& global_table = GlobalTable::instance();
  const std::string qualified = qualify(namespace_name, name);

  // Lock-free fast path: the overwhelming majority of calls look up a
  // symbol that already exists.
  {
    const Snapshot& snapshot = global_table.read();
    const auto it = snapshot.index_by_qualified_name.find(qualified);
    if (it != snapshot.index_by_qualified_name.end()) {
      if (!same_attributes(attributes, snapshot.entries[it->second].attributes)) {
        throw_conflict(qualified, snapshot.entries[it->second]);
      }
      return Symbol(it->second);
    }
  }

  return Symbol(global_table.insert_or_validate(qualified, attributes, location));
}

std::string symbol_name(Symbol symbol) {
  return GlobalTable::instance().read().entries.at(symbol.id()).qualified_name;
}

} // namespace symbol_table
