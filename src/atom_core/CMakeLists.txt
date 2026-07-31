# src/atom_core/CMakeLists.txt

file(GLOB atom_core_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
add_library(atom_core ${atom_core_sources})
add_library(stocas::atom_core ALIAS atom_core)

target_include_directories(
  atom_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)

target_compile_features(atom_core PUBLIC cxx_std_20)

target_link_libraries(
  atom_core
  PUBLIC stocas::numerica_core stocas::symbol_table
  PRIVATE $<BUILD_INTERFACE:stocas::project_options> $<BUILD_INTERFACE:stocas::sanitizers>)

include(GNUInstallDirs)
install(TARGETS atom_core EXPORT atom_coreTargets)
install(DIRECTORY include/atom_core DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT atom_coreTargets
  FILE atom_coreTargets.cmake
  NAMESPACE stocas::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom_core)
