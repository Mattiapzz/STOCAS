// libFuzzer harness for pattern_core::match (M5's remaining Testing
// checklist item: fuzz the matcher with randomly generated pattern/target
// pairs, asserting no crashes). Opt-in target, built only when
// STOCAS_ENABLE_FUZZING is ON (see ProjectOptions.cmake) - not part of the
// default build or the regular Catch2 test binary, mirroring
// atom_core/fuzz_parser.cc's (M3-T2) established shape exactly.
//
// match() is expected to reject a malformed/misused pattern (more than one
// direct sequence-wildcard child in an Add/Mul/Fun argument list, or one
// used outside that context) via a thrown std::invalid_argument - not a
// fuzzer finding, since that's documented, intentional behavior (see
// matcher.hh's file comment). Anything else (a crash, an unhandled
// exception of a different type, an ASan/UBSan report) propagates and
// crashes the fuzzer, which is the point.
//
// Same symbol_table growth concern as fuzz_parser.cc: every registered
// symbol name is process-lifetime and never freed. This harness sidesteps
// it entirely (rather than bounding a charset) by registering a small,
// fixed pool of symbols exactly once (function-local static) and reusing
// them across every input - no new symbol is ever registered mid-fuzz.
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <atom_core/atom.hh>
#include <numerica_core/rational.hh>
#include <pattern_core/matcher.hh>
#include <symbol_table/symbol.hh>

namespace {

using atom_core::Atom;
using atom_core::AtomStore;

struct SymbolPool {
  symbol_table::Symbol plain_x = symbol_table::get_symbol("pattern_core_fuzz", "x");
  symbol_table::Symbol plain_y = symbol_table::get_symbol("pattern_core_fuzz", "y");
  symbol_table::Symbol single_wildcard = symbol_table::get_symbol("pattern_core_fuzz", "w_");
  symbol_table::Symbol sequence_wildcard = symbol_table::get_symbol("pattern_core_fuzz", "s__");
  symbol_table::Symbol any_count_wildcard = symbol_table::get_symbol("pattern_core_fuzz", "a___");
  symbol_table::Symbol plain_fun = symbol_table::get_symbol("pattern_core_fuzz", "f");
  symbol_table::Symbol symmetric_fun = symbol_table::get_symbol(
      "pattern_core_fuzz",
      "g",
      std::array<symbol_table::SymbolAttribute, 1>{symbol_table::SymbolAttribute::Symmetric});
};

const SymbolPool& symbol_pool() {
  static const SymbolPool pool;
  return pool;
}

// Reads fuzzer input bytes as a stream of pseudo-random decisions; past
// the end of the buffer, every read is 0 (deterministic, never UB).
class ByteCursor {
public:
  ByteCursor(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

  std::uint8_t next() {
    if (pos_ >= size_) {
      return 0;
    }
    return data_[pos_++];
  }

private:
  const std::uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

Atom build_random_atom(AtomStore& store, ByteCursor& cursor, int depth, bool allow_wildcards) {
  const SymbolPool& pool = symbol_pool();
  int leaf_choices = allow_wildcards ? 5 : 3;
  int total_choices = allow_wildcards ? 8 : 6;
  std::uint8_t choice = cursor.next() % (depth <= 0 ? leaf_choices : total_choices);

  switch (choice) {
    case 0:
      return store.num(numerica_core::Rational(static_cast<int>(cursor.next()) - 128));
    case 1:
      return store.var(pool.plain_x);
    case 2:
      return store.var(pool.plain_y);
    case 3:
      return store.var(pool.single_wildcard);
    case 4:
      return store.var(cursor.next() % 2 == 0 ? pool.sequence_wildcard : pool.any_count_wildcard);
    default: {
      int child_count = 1 + (cursor.next() % 3);
      std::vector<Atom> children;
      children.reserve(static_cast<std::size_t>(child_count));
      for (int i = 0; i < child_count; ++i) {
        children.push_back(build_random_atom(store, cursor, depth - 1, allow_wildcards));
      }
      switch (cursor.next() % 4) {
        case 0:
          return store.add(children);
        case 1:
          return store.mul(children);
        case 2: {
          Atom exponent = children.size() > 1 ? children[1] : store.num(numerica_core::Rational(2));
          return store.pow(children[0], exponent);
        }
        default: {
          symbol_table::Symbol head = cursor.next() % 2 == 0 ? pool.plain_fun : pool.symmetric_fun;
          return store.fun(head, children);
        }
      }
    }
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  ByteCursor cursor(data, size);
  AtomStore pattern_store;
  AtomStore target_store;
  Atom pattern = build_random_atom(pattern_store, cursor, /*depth=*/4, /*allow_wildcards=*/true);
  Atom target = build_random_atom(target_store, cursor, /*depth=*/4, /*allow_wildcards=*/false);

  try {
    (void) pattern_core::match(pattern.view(), target.view());
  } catch (const std::invalid_argument&) {
    // Expected outcome for pattern-authoring misuse (see matcher.hh) - not
    // a fuzzer finding.
  }
  return 0;
}
