file(GLOB atom_core_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

stocas_add_catch_test(
  NAME atom_core_tests
  SOURCES ${atom_core_test_sources}
  LINK_LIBRARIES stocas::atom_core stocas::numerica_core stocas::symbol_table
  LABELS atom_core)
