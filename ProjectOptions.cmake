# ProjectOptions.cmake
#
# All build-wide option()/cache-variable toggles live here, defined once.
# Never define compiler flags, sanitizer logic, or dependency URLs in the
# top-level CMakeLists.txt (per CLAUDE.md §1/§6) - this file and
# Dependencies.cmake are the only places that do.

option(STOCAS_BUILD_TESTS "Build Catch2 test suites" ON)
option(STOCAS_BUILD_EXAMPLES "Build examples/ targets" ON)
option(STOCAS_ENABLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(STOCAS_ENABLE_CLANG_TIDY "Run clang-tidy during the build" OFF)
option(STOCAS_ENABLE_CPPCHECK "Run cppcheck during the build" OFF)
option(STOCAS_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(STOCAS_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(STOCAS_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# BUILD_SHARED_LIBS is a standard CMake cache variable (not redefined here);
# add_library() calls across the project must omit STATIC/SHARED so this
# variable controls library type project-wide, per CLAUDE.md §6.

# --- A single interface target carrying project-wide compiler settings ---
add_library(stocas_project_options INTERFACE)
add_library(stocas::project_options ALIAS stocas_project_options)
target_compile_features(stocas_project_options INTERFACE cxx_std_20)

if(MSVC)
  target_compile_options(stocas_project_options INTERFACE /W4)
  if(STOCAS_ENABLE_WARNINGS_AS_ERRORS)
    target_compile_options(stocas_project_options INTERFACE /WX)
  endif()
else()
  target_compile_options(stocas_project_options INTERFACE -Wall -Wextra)
  if(STOCAS_ENABLE_WARNINGS_AS_ERRORS)
    target_compile_options(stocas_project_options INTERFACE -Werror)
  endif()
endif()

# --- Sanitizers: opt-in, Debug-oriented, not on by default (CLAUDE.md §10) ---
add_library(stocas_sanitizers INTERFACE)
add_library(stocas::sanitizers ALIAS stocas_sanitizers)

if(NOT MSVC)
  set(_stocas_sanitizer_flags "")
  if(STOCAS_ENABLE_ASAN)
    list(APPEND _stocas_sanitizer_flags "-fsanitize=address")
  endif()
  if(STOCAS_ENABLE_UBSAN)
    list(APPEND _stocas_sanitizer_flags "-fsanitize=undefined")
  endif()
  if(STOCAS_ENABLE_TSAN)
    list(APPEND _stocas_sanitizer_flags "-fsanitize=thread")
  endif()
  if(_stocas_sanitizer_flags)
    target_compile_options(stocas_sanitizers INTERFACE ${_stocas_sanitizer_flags} -fno-omit-frame-pointer)
    target_link_options(stocas_sanitizers INTERFACE ${_stocas_sanitizer_flags})
  endif()
endif()

# --- clang-tidy / cppcheck: opt-in local targets, not warnings-as-errors ---
if(STOCAS_ENABLE_CLANG_TIDY)
  find_program(STOCAS_CLANG_TIDY_EXE NAMES clang-tidy)
  if(STOCAS_CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${STOCAS_CLANG_TIDY_EXE}")
  endif()
endif()

if(STOCAS_ENABLE_CPPCHECK)
  find_program(STOCAS_CPPCHECK_EXE NAMES cppcheck)
  if(STOCAS_CPPCHECK_EXE)
    set(CMAKE_CXX_CPPCHECK "${STOCAS_CPPCHECK_EXE}" "--enable=all" "--inline-suppr")
  endif()
endif()
