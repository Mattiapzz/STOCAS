file(GLOB calculus_core_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

stocas_add_catch_test(
  NAME calculus_core_tests
  SOURCES ${calculus_core_test_sources}
  LINK_LIBRARIES stocas::calculus_core stocas::atom_core stocas::symbol_table
  LABELS calculus_core)
