# src/calculus_core/CMakeLists.txt

file(GLOB calculus_core_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
add_library(calculus_core ${calculus_core_sources})
add_library(stocas::calculus_core ALIAS calculus_core)

target_include_directories(
  calculus_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)

target_compile_features(calculus_core PUBLIC cxx_std_20)

# atom_core (Atom/AtomView/AtomStore) and symbol_table (Symbol) both appear
# in derivative.hh's public API, so both are PUBLIC.
target_link_libraries(
  calculus_core
  PUBLIC stocas::atom_core stocas::symbol_table
  PRIVATE $<BUILD_INTERFACE:stocas::project_options> $<BUILD_INTERFACE:stocas::sanitizers>)

include(GNUInstallDirs)
install(TARGETS calculus_core EXPORT calculus_coreTargets)
install(DIRECTORY include/calculus_core DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT calculus_coreTargets
  FILE calculus_coreTargets.cmake
  NAMESPACE stocas::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/calculus_core)
