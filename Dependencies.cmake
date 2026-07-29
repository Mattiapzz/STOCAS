# Dependencies.cmake
#
# All third-party dependency declarations live here exclusively, each
# pinned to a specific release TAG (never a branch, never a submodule),
# per CLAUDE.md §7. Only libraries pre-approved in the project roadmap's
# "Third-party dependencies" table may be added without asking the
# project owner first.
#
include(FetchContent)

if(STOCAS_BUILD_TESTS)
  # Catch2 v3 - unit/property/templated testing, used by all modules' test/.
  # Pinned tag: v3.7.1. Needed for every Catch2 test binary in test/<module>/.
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1)
  FetchContent_MakeAvailable(Catch2)

  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  include(CTest)
endif()

# GMP - arbitrary-precision integer arithmetic, needed by numerica_core's
# BigInt (M1-T1). LGPLv3+/GPLv2+ dual-licensed; this project links it
# dynamically (LGPL-compatible path), per CLAUDE.md §7 and the roadmap's
# pre-approved dependency table.
#
# find_package-first (CLAUDE.md §7): GMP has no CMake-native build system
# (autotools), so a FetchContent-built fallback is impractical to keep
# portable across all three CI platforms. Require a system-installed GMP
# (e.g. `brew install gmp` / `apt install libgmp-dev` / vcpkg on Windows)
# instead of vendoring a build.
find_package(GMP REQUIRED)
