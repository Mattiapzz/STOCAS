file(GLOB symbol_table_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

stocas_add_catch_test(
  NAME symbol_table_tests
  SOURCES ${symbol_table_test_sources}
  LINK_LIBRARIES stocas::symbol_table
  LABELS symbol_table)

# Exercises the SYMBOL_TABLE_SYMBOL()/kCoreNamespace path (symbol.hh), which
# only compiles in when the consuming target injects CORE_NAMESPACE - real
# consumers (e.g. atom_core, M3+) will set this to their own module name.
target_compile_definitions(symbol_table_tests PRIVATE CORE_NAMESPACE="symbol_table_tests")
