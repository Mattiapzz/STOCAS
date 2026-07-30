file(GLOB hello_core_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

stocas_add_catch_test(
  NAME hello_core_tests
  SOURCES ${hello_core_test_sources}
  LINK_LIBRARIES stocas::hello_core
  LABELS hello_core)
