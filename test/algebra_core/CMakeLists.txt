file(GLOB algebra_core_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

stocas_add_catch_test(
  NAME algebra_core_tests
  SOURCES ${algebra_core_test_sources}
  LINK_LIBRARIES stocas::algebra_core
  LABELS algebra_core)
