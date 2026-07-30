# FindGMP.cmake
#
# Locates a system-installed GMP (find_package-first per CLAUDE.md §7).
# Defines the imported target GMP::GMP on success.
#
# Result variables:
#   GMP_FOUND
#   GMP_INCLUDE_DIR
#   GMP_LIBRARY

find_path(GMP_INCLUDE_DIR NAMES gmp.h)
find_library(GMP_LIBRARY NAMES gmp libgmp)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP REQUIRED_VARS GMP_LIBRARY GMP_INCLUDE_DIR)

if(GMP_FOUND AND NOT TARGET GMP::GMP)
  add_library(GMP::GMP UNKNOWN IMPORTED)
  set_target_properties(
    GMP::GMP
    PROPERTIES IMPORTED_LOCATION "${GMP_LIBRARY}"
               INTERFACE_INCLUDE_DIRECTORIES "${GMP_INCLUDE_DIR}")
endif()

mark_as_advanced(GMP_INCLUDE_DIR GMP_LIBRARY)
