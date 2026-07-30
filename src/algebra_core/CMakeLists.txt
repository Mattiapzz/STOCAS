# src/algebra_core/CMakeLists.txt
#
# Header-only (concepts have no implementation to compile), so this is an
# INTERFACE library rather than STATIC/SHARED - CLAUDE.md §6's
# BUILD_SHARED_LIBS-controlled add_library() rule doesn't apply here since
# there's no compiled unit either way.

add_library(algebra_core INTERFACE)
add_library(stocas::algebra_core ALIAS algebra_core)

target_include_directories(
  algebra_core
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)

target_compile_features(algebra_core INTERFACE cxx_std_20)

include(GNUInstallDirs)
install(TARGETS algebra_core EXPORT algebra_coreTargets)
install(DIRECTORY include/algebra_core DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT algebra_coreTargets
  FILE algebra_coreTargets.cmake
  NAMESPACE stocas::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/algebra_core)
