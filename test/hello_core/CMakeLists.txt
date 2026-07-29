stocas_add_catch_test(
  NAME hello_core_tests
  SOURCES test_greeter.cc
  LINK_LIBRARIES stocas::hello_core
  LABELS hello_core)
