# src/symbol_table/CMakeLists.txt

file(GLOB symbol_table_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
add_library(symbol_table ${symbol_table_sources})
add_library(stocas::symbol_table ALIAS symbol_table)

target_include_directories(
  symbol_table
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)

target_compile_features(symbol_table PUBLIC cxx_std_20)

target_link_libraries(
  symbol_table
  PRIVATE $<BUILD_INTERFACE:stocas::project_options> $<BUILD_INTERFACE:stocas::sanitizers>)

include(GNUInstallDirs)
install(TARGETS symbol_table EXPORT symbol_tableTargets)
install(DIRECTORY include/symbol_table DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT symbol_tableTargets
  FILE symbol_tableTargets.cmake
  NAMESPACE stocas::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/symbol_table)
