cmake_minimum_required(VERSION 3.21)

project(
  stocas
  VERSION 0.1.0
  DESCRIPTION "StoCAS - Standalone Template-Oriented Computer Algebra System"
  LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ProjectOptions.cmake)
include(Dependencies.cmake)

if(STOCAS_BUILD_TESTS)
  enable_testing()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
  add_subdirectory(src)
endif()

if(STOCAS_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
  add_subdirectory(test)
endif()

if(STOCAS_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
  add_subdirectory(examples)
endif()
