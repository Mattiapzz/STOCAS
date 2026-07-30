file(GLOB symbol_table_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

# test_concurrency.cc's TSan stress test uses std::thread, which needs
# explicit Threads::Threads linkage on some platforms (notably Linux/glibc).
find_package(Threads REQUIRED)

stocas_add_catch_test(
  NAME symbol_table_tests
  SOURCES ${symbol_table_test_sources}
  LINK_LIBRARIES stocas::symbol_table Threads::Threads
  LABELS symbol_table)

# Exercises the SYMBOL_TABLE_SYMBOL()/kCoreNamespace path (symbol.hh), which
# only compiles in when the consuming target injects CORE_NAMESPACE - real
# consumers (e.g. atom_core, M3+) will set this to their own module name.
target_compile_definitions(symbol_table_tests PRIVATE CORE_NAMESPACE="symbol_table_tests")

# M2-T5: shared-library singleton identity test (separate subdirectory since
# it builds its own SHARED copy of symbol_table's sources - see its
# CMakeLists.txt for why).
add_subdirectory(shared_singleton)
