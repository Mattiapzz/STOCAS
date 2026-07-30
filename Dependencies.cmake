# Dependencies.cmake
#
# All third-party dependency declarations live here exclusively, each
# pinned to a specific release TAG (never a branch, never a submodule),
# per CLAUDE.md §7. Only libraries pre-approved in the project roadmap's
# "Third-party dependencies" table may be added without asking the
# project owner first.
#
# Empty for now (M0-T1) - the Catch2 FetchContent_Declare entry and CTest
# wiring land in M0-T2.

include(FetchContent)
