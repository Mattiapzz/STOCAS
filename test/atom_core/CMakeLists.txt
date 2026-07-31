file(GLOB atom_core_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
list(REMOVE_ITEM atom_core_test_sources "${CMAKE_CURRENT_SOURCE_DIR}/fuzz_parser.cc")

stocas_add_catch_test(
  NAME atom_core_tests
  SOURCES ${atom_core_test_sources}
  LINK_LIBRARIES stocas::atom_core stocas::numerica_core stocas::symbol_table
  LABELS atom_core)

# M3-T2 fuzz target: opt-in (STOCAS_ENABLE_FUZZING), requires clang's
# libFuzzer (-fsanitize=fuzzer) - not part of the default build or the
# regular Catch2 binary above (fuzz_parser.cc is excluded from it).
if(STOCAS_ENABLE_FUZZING)
  add_executable(atom_core_fuzz_parser fuzz_parser.cc)
  target_link_libraries(atom_core_fuzz_parser PRIVATE stocas::atom_core stocas::numerica_core
                                                       stocas::symbol_table)
  target_compile_options(atom_core_fuzz_parser PRIVATE -fsanitize=fuzzer,address)
  target_link_options(atom_core_fuzz_parser PRIVATE -fsanitize=fuzzer,address)
endif()
