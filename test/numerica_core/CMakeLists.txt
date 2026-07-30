file(GLOB numerica_core_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

stocas_add_catch_test(
  NAME numerica_core_tests
  SOURCES ${numerica_core_test_sources}
  LINK_LIBRARIES stocas::numerica_core
  LABELS numerica_core)
