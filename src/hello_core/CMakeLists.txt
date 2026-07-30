# src/hello_core/CMakeLists.txt
#
# Placeholder module (M0-T4) validating the standard module shape before
# it's cloned for real modules (CLAUDE.md §1/§6).

add_library(hello_core greeter.cc)
add_library(stocas::hello_core ALIAS hello_core)

target_include_directories(
  hello_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)

target_compile_features(hello_core PUBLIC cxx_std_20)
target_link_libraries(hello_core PRIVATE $<BUILD_INTERFACE:stocas::project_options>
                                          $<BUILD_INTERFACE:stocas::sanitizers>)

include(GNUInstallDirs)
install(TARGETS hello_core EXPORT hello_coreTargets)
install(DIRECTORY include/hello_core DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT hello_coreTargets
  FILE hello_coreTargets.cmake
  NAMESPACE stocas::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hello_core)

add_subdirectory(python)
