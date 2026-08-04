file(GLOB pattern_core_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

stocas_add_catch_test(
  NAME pattern_core_tests
  SOURCES ${pattern_core_test_sources}
  LINK_LIBRARIES stocas::pattern_core stocas::atom_core stocas::symbol_table
  LABELS pattern_core)
