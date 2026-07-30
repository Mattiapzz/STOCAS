# FindMPFR.cmake
#
# Locates a system-installed MPFR (find_package-first per CLAUDE.md §7).
# Defines the imported target MPFR::MPFR on success. Depends on GMP.
#
# Result variables:
#   MPFR_FOUND
#   MPFR_INCLUDE_DIR
#   MPFR_LIBRARY

find_path(MPFR_INCLUDE_DIR NAMES mpfr.h)
find_library(MPFR_LIBRARY NAMES mpfr libmpfr)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MPFR REQUIRED_VARS MPFR_LIBRARY MPFR_INCLUDE_DIR)

if(MPFR_FOUND AND NOT TARGET MPFR::MPFR)
  add_library(MPFR::MPFR UNKNOWN IMPORTED)
  set_target_properties(
    MPFR::MPFR
    PROPERTIES IMPORTED_LOCATION "${MPFR_LIBRARY}"
               INTERFACE_INCLUDE_DIRECTORIES "${MPFR_INCLUDE_DIR}"
               INTERFACE_LINK_LIBRARIES GMP::GMP)
endif()

mark_as_advanced(MPFR_INCLUDE_DIR MPFR_LIBRARY)
