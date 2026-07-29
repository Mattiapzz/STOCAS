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
