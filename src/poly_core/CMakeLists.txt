# src/poly_core/CMakeLists.txt

file(GLOB poly_core_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
add_library(poly_core ${poly_core_sources})
add_library(stocas::poly_core ALIAS poly_core)

target_include_directories(
  poly_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)

target_compile_features(poly_core PUBLIC cxx_std_20)

# algebra_core (Ring/Field concepts, ring-struct coefficient types),
# symbol_table (variable identifiers), and atom_core (atom_conversion.hh's
# public API exposes Atom/AtomView/AtomStore directly) are PUBLIC.
target_link_libraries(
  poly_core
  PUBLIC stocas::algebra_core stocas::symbol_table stocas::atom_core
  PRIVATE $<BUILD_INTERFACE:stocas::project_options> $<BUILD_INTERFACE:stocas::sanitizers>)

include(GNUInstallDirs)
install(TARGETS poly_core EXPORT poly_coreTargets)
install(DIRECTORY include/poly_core DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT poly_coreTargets
  FILE poly_coreTargets.cmake
  NAMESPACE stocas::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/poly_core)
