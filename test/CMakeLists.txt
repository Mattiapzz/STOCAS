# test/CMakeLists.txt
#
# Mirrors src/ 1:1 by module (CLAUDE.md §1). The trivial_sanity target below
# only exists to prove the Catch2 + CTest wiring works (M0-T2); it is
# removed once the first real module (hello_core, M0-T4) lands.

stocas_add_catch_test(
  NAME trivial_sanity_tests
  SOURCES test_trivial_sanity.cc
  LABELS sanity)
