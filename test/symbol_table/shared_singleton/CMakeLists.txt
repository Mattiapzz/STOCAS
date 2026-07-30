# M2-T5: shared-library singleton identity test.
#
# Compiles symbol_table's own sources into a dedicated SHARED library
# (independent of the project-wide BUILD_SHARED_LIBS default, which governs
# stocas::symbol_table itself) and links two independent plugin shared
# libraries against it dynamically. This is the configuration the M2-T5
# decision requires in production: every consumer of symbol_table in a
# process that loads more than one shared library must link against the
# *same* symbol_table shared library, never statically embed their own copy.

file(GLOB symbol_table_sources CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/src/symbol_table/*.cc")

add_library(symbol_table_shared_for_singleton_test SHARED ${symbol_table_sources})
target_include_directories(
  symbol_table_shared_for_singleton_test
  PUBLIC "${CMAKE_SOURCE_DIR}/src/symbol_table/include")
target_compile_features(symbol_table_shared_for_singleton_test PUBLIC cxx_std_20)
target_link_libraries(symbol_table_shared_for_singleton_test
                       PRIVATE $<BUILD_INTERFACE:stocas::project_options>)
set_target_properties(symbol_table_shared_for_singleton_test
                       PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(singleton_plugin_a SHARED plugin_a.cc)
target_link_libraries(singleton_plugin_a PRIVATE symbol_table_shared_for_singleton_test)
set_target_properties(singleton_plugin_a PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(singleton_plugin_b SHARED plugin_b.cc)
target_link_libraries(singleton_plugin_b PRIVATE symbol_table_shared_for_singleton_test)
set_target_properties(singleton_plugin_b PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

stocas_add_catch_test(
  NAME symbol_table_singleton_tests
  SOURCES test_singleton_identity.cc
  LINK_LIBRARIES singleton_plugin_a singleton_plugin_b
  LABELS symbol_table)
