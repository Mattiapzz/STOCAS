// libFuzzer harness for atom_core::parse (M3-T2 exit criteria: fuzzed for a
// fixed CPU-time budget with zero crashes). Opt-in target, built only when
// STOCAS_ENABLE_FUZZING is ON (see ProjectOptions.cmake) - not part of the
// default build or the regular Catch2 test binary.
//
// The parser is expected to reject malformed input via a thrown
// std::invalid_argument - libFuzzer only flags crashes (segfaults, ASan
// reports, unhandled non-std::invalid_argument exceptions terminating via
// std::terminate), not rejections, so we explicitly swallow
// std::invalid_argument and let anything else propagate and crash the
// fuzzer (which is the point: any other exception type, or a native crash,
// is a real bug).
//
// symbol_table's GlobalTable (M2-T3) is a process-wide, append-only,
// intentionally-never-freed registration table - not a leak, but that means
// a long fuzzing run that feeds it millions of *distinct* random identifier
// names will legitimately OOM regardless of parser correctness, which isn't
// what this harness is meant to find. To keep the fuzz target actually
// exercising parser logic (precedence, malformed tokens, unbalanced
// parens, etc.) instead of symbol-table growth, every input byte is mapped
// through a small fixed charset AND consecutive alnum characters are
// forcibly split with a space so every identifier/number token is exactly
// one character wide - bounding the total distinct symbols this process
// can ever register to the charset size, however long the fuzzing run.
//
// Even bounded, every insert into GlobalTable deliberately leaks its
// superseded Snapshot (see symbol_table.cc's GlobalTable::insert_or_validate)
// - that leak is already __lsan_ignore_object()'d for LeakSanitizer's normal
// end-of-process check, but libFuzzer runs its own periodic leak check that
// re-flags it regardless (its `-detect_leaks=0` flag does not suppress
// this on its own). Run this harness with `ASAN_OPTIONS=detect_leaks=0` in
// the environment to see only genuine crashes/UB, not this known leak.
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <atom_core/atom.hh>
#include <atom_core/parser.hh>

namespace {

constexpr std::array<char, 24> kCharset{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '-',
                                        '*', '/', '^', '(', ')', ',', ' ', 'x', 'y', 'z', 'f', 'g'};

bool is_alnum(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  std::string text;
  text.reserve(size * 2);
  char previous = '\0';
  for (std::size_t i = 0; i < size; ++i) {
    char c = kCharset[data[i] % kCharset.size()]; // NOLINT
    if (is_alnum(previous) && is_alnum(c)) {
      text.push_back(' '); // force every identifier/number to be one char
    }
    text.push_back(c);
    previous = c;
  }

  atom_core::AtomStore store;
  try {
    (void) atom_core::parse(text, store, "atom_core_fuzz");
  } catch (const std::invalid_argument&) {
    // Expected outcome for malformed input - not a fuzzer finding.
  }
  return 0;
}
