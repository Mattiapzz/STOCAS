file(GLOB poly_core_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

stocas_add_catch_test(
  NAME poly_core_tests
  SOURCES ${poly_core_test_sources}
  LINK_LIBRARIES stocas::poly_core stocas::algebra_core stocas::numerica_core stocas::symbol_table
  LABELS poly_core)
