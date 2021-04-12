# Find LASlib
#
# This sets:
#   - LASLIB_FOUND:  system has LASlib
#   - LASLIB_INCLUDE_DIRS: the LASlib include directories
#   - LASLIB_LIBRARIES: the LASlib library
#   - LASLIB_VERSION: the version string for LASlib

find_path (LASLIB_INCLUDE_DIRS NAMES lasdefinitions.hpp)
find_library (LASLIB_LIBRARY NAMES LASlib)

set (LASLIB_LIBRARIES ${LASLIB_LIBRARY})

if (LASLIB_INCLUDE_DIRS)
     file(STRINGS ${LASLIB_INCLUDE_DIRS}/lasdefinitions.hpp version_line
          REGEX "#define *LAS_TOOLS_VERSION")
     string(REGEX MATCH "[0-9]+" LASLIB_VERSION_STRING ${version_line})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LASlib
    REQUIRED_VARS LASLIB_INCLUDE_DIRS LASLIB_LIBRARY
    VERSION_VAR LASLIB_VERSION_STRING
)

mark_as_advanced(LASLIB_LIBRARY LASLIB_LIBRARIES LASLIB_INCLUDE_DIRS)
