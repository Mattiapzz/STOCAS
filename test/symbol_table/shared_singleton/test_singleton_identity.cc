#include <cstddef>

#include <catch2/catch_test_macros.hpp>

// M2-T5: symbol_table is a process-wide singleton (GlobalTable::instance()).
// plugin_a and plugin_b (see CMakeLists.txt in this directory) are two
// independent shared libraries that both call into the *same* symbol_table
// shared library (symbol_table_shared_for_singleton_test) rather than each
// statically embedding their own copy of it. This is the configuration
// documented as required in symbol.hh / roadmap.md's M2-T5 entry: a symbol
// registered through one shared library must resolve to the identical id
// when looked up through a different shared library, proving there is
// exactly one GlobalTable instance across the shared-library boundary.
//
// Decision recorded here (gates M9-T3): symbol_table must always be
// consumed as a single shared library that every other shared library
// dynamically links against - never statically embedded into more than one
// .so in the same process. Two .so's each statically linking symbol_table
// would each get their own private copy of GlobalTable's function-local
// static, silently splitting the "single global table" invariant this
// module exists to provide.
extern "C" std::size_t plugin_a_register(const char* name);
extern "C" std::size_t plugin_b_lookup(const char* name);

TEST_CASE("A symbol registered via one shared library resolves identically via another",
          "[symbol_table][singleton][shared_libs]") {
  const std::size_t id_from_a = plugin_a_register("shared_boundary_probe");
  const std::size_t id_from_b = plugin_b_lookup("shared_boundary_probe");
  REQUIRE(id_from_a == id_from_b);
}
