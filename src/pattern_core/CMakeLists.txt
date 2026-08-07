# src/pattern_core/CMakeLists.txt

file(GLOB pattern_core_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
add_library(pattern_core ${pattern_core_sources})
add_library(stocas::pattern_core ALIAS pattern_core)

target_include_directories(
  pattern_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)

target_compile_features(pattern_core PUBLIC cxx_std_20)

# parallel_rewrite.cc uses std::thread directly, which needs explicit
# Threads::Threads linkage on some platforms (notably Linux/glibc) - same
# rationale as test/symbol_table/CMakeLists.txt's identical find_package.
find_package(Threads REQUIRED)

# atom_core (AtomView is matched against) and symbol_table (wildcard
# symbols) are PUBLIC - both appear in matcher.hh's public API.
target_link_libraries(
  pattern_core
  PUBLIC stocas::atom_core stocas::symbol_table
  PRIVATE Threads::Threads $<BUILD_INTERFACE:stocas::project_options>
          $<BUILD_INTERFACE:stocas::sanitizers>)

include(GNUInstallDirs)
install(TARGETS pattern_core EXPORT pattern_coreTargets)
install(DIRECTORY include/pattern_core DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT pattern_coreTargets
  FILE pattern_coreTargets.cmake
  NAMESPACE stocas::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pattern_core)
