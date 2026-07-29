stocas_add_catch_test(
  NAME numerica_core_tests
  SOURCES test_big_int.cc test_rational.cc test_finite_field.cc
  LINK_LIBRARIES stocas::numerica_core
  LABELS numerica_core)
