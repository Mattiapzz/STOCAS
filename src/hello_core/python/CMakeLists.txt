# src/hello_core/python/CMakeLists.txt
#
# C API + ctypes wrapper for hello_core, per CLAUDE.md §9. The C API is
# always built as a shared library regardless of BUILD_SHARED_LIBS, since
# ctypes.CDLL() requires a dynamic library to load at runtime.

add_library(hello_core_c_api SHARED hello_core_c_api.cc)
add_library(stocas::hello_core_c_api ALIAS hello_core_c_api)

target_link_libraries(hello_core_c_api PRIVATE stocas::hello_core stocas::project_options
                                                stocas::sanitizers)
target_compile_definitions(hello_core_c_api PRIVATE HELLO_CORE_BUILDING_SHARED)
set_target_properties(hello_core_c_api PROPERTIES CXX_VISIBILITY_PRESET hidden)

include(GNUInstallDirs)
install(TARGETS hello_core_c_api DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES hello_core.py DESTINATION ${CMAKE_INSTALL_LIBDIR}/hello_core)

# Copy the wrapper + compiled C API next to each other in the build tree so
# `hello_core.py`'s _load_library() finds the library without installing.
add_custom_command(
  TARGET hello_core_c_api
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/hello_core.py
          $<TARGET_FILE_DIR:hello_core_c_api>/hello_core.py)

if(STOCAS_BUILD_TESTS)
  add_test(
    NAME hello_core_python_tests
    COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=$<TARGET_FILE_DIR:hello_core_c_api> python3 -m
            pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests -q)
  set_tests_properties(hello_core_python_tests PROPERTIES LABELS "hello_core;python")
endif()
