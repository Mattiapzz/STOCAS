# src/numerica_core/CMakeLists.txt

add_library(numerica_core big_int.cc rational.cc)
add_library(stocas::numerica_core ALIAS numerica_core)

target_include_directories(
  numerica_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)

target_compile_features(numerica_core PUBLIC cxx_std_20)
# GMP is an implementation detail (big_int.hh never exposes GMP types), so
# it's linked PRIVATE and wrapped in $<BUILD_INTERFACE:> alongside the other
# non-exported dependencies to keep it out of numerica_coreTargets.cmake.
target_link_libraries(
  numerica_core
  PRIVATE $<BUILD_INTERFACE:GMP::GMP> $<BUILD_INTERFACE:stocas::project_options>
          $<BUILD_INTERFACE:stocas::sanitizers>)

include(GNUInstallDirs)
install(TARGETS numerica_core EXPORT numerica_coreTargets)
install(DIRECTORY include/numerica_core DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT numerica_coreTargets
  FILE numerica_coreTargets.cmake
  NAMESPACE stocas::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/numerica_core)
