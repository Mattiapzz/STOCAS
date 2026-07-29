# CatchHelpers.cmake
#
# Reusable helper so per-module test/<module>/CMakeLists.txt files don't
# each repeat the Catch2 target/discovery boilerplate (CLAUDE.md §8: one
# test binary per module, registered via catch_discover_tests).
#
# Usage:
#   stocas_add_catch_test(
#     NAME <module>_tests
#     SOURCES test_widget.cc test_gadget.cc
#     LINK_LIBRARIES stocas::<module>
#     LABELS <module>
#   )

include("${catch2_SOURCE_DIR}/extras/Catch.cmake")

function(stocas_add_catch_test)
  set(options)
  set(one_value_args NAME)
  set(multi_value_args SOURCES LINK_LIBRARIES LABELS)
  cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

  add_executable(${ARG_NAME} ${ARG_SOURCES})
  target_link_libraries(${ARG_NAME} PRIVATE Catch2::Catch2WithMain ${ARG_LINK_LIBRARIES}
                                             stocas::project_options stocas::sanitizers)

  catch_discover_tests(${ARG_NAME} TEST_PREFIX "${ARG_NAME}::" PROPERTIES LABELS "${ARG_LABELS}")
endfunction()
