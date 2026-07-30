# src/algebra_core/CMakeLists.txt

file(GLOB algebra_core_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
add_library(algebra_core ${algebra_core_sources})
add_library(stocas::algebra_core ALIAS algebra_core)

target_include_directories(
  algebra_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)

target_compile_features(algebra_core PUBLIC cxx_std_20)

# numerica_core is PUBLIC (not PRIVATE, unlike numerica_core's own GMP/MPFR
# link): rings.hh's IntegerRing/RationalField/FiniteFieldRing<T> expose
# BigInt/Rational/FiniteField<T> directly as their Element types, so callers
# including <algebra_core/rings.hh> need numerica_core's headers too.
target_link_libraries(
  algebra_core
  PUBLIC stocas::numerica_core
  PRIVATE $<BUILD_INTERFACE:stocas::project_options> $<BUILD_INTERFACE:stocas::sanitizers>)

include(GNUInstallDirs)
install(TARGETS algebra_core EXPORT algebra_coreTargets)
install(DIRECTORY include/algebra_core DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT algebra_coreTargets
  FILE algebra_coreTargets.cmake
  NAMESPACE stocas::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/algebra_core)
