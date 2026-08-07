file(GLOB pattern_core_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
list(REMOVE_ITEM pattern_core_test_sources "${CMAKE_CURRENT_SOURCE_DIR}/fuzz_matcher.cc")

stocas_add_catch_test(
  NAME pattern_core_tests
  SOURCES ${pattern_core_test_sources}
  LINK_LIBRARIES stocas::pattern_core stocas::atom_core stocas::symbol_table
  LABELS pattern_core)

# M5 fuzz target: opt-in (STOCAS_ENABLE_FUZZING), requires clang's
# libFuzzer (-fsanitize=fuzzer) - not part of the default build or the
# regular Catch2 binary above (fuzz_matcher.cc is excluded from it), same
# shape as atom_core's fuzz_parser.cc (M3-T2).
if(STOCAS_ENABLE_FUZZING)
  add_executable(pattern_core_fuzz_matcher fuzz_matcher.cc)
  target_link_libraries(pattern_core_fuzz_matcher PRIVATE stocas::pattern_core stocas::atom_core
                                                           stocas::numerica_core stocas::symbol_table)
  target_compile_options(pattern_core_fuzz_matcher PRIVATE -fsanitize=fuzzer,address)
  target_link_options(pattern_core_fuzz_matcher PRIVATE -fsanitize=fuzzer,address)
endif()
