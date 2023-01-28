# Find Imath
#
# This sets:
#   - IMATH_FOUND:  system has Imath
#   - IMATH_INCLUDE_DIRS: the Imath include directories
#   - IMATH_LIBRARIES: the Imath libraries
#   - IMATH_VERSION: the version string for Imath

find_path(IMATH_INCLUDE_DIRS Imath/ImathConfig.h)
find_library(IMATH_IMATH_LIBRARY Imath-3_1)

set(IMATH_LIBRARIES ${IMATH_IMATH_LIBRARY})

file(STRINGS ${IMATH_INCLUDE_DIRS}/Imath/ImathConfig.h version_line
     REGEX "IMATH_VERSION_STRING|PACKAGE_VERSION")
string(REGEX MATCH "[0-9]+.[0-9]+.[0-9]+" IMATH_VERSION_STRING ${version_line})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Imath
    REQUIRED_VARS IMATH_INCLUDE_DIRS IMATH_IMATH_LIBRARY
    VERSION_VAR IMATH_VERSION_STRING
    )

mark_as_advanced(IMATH_IMATH_LIBRARY IMATH_INCLUDE_DIRS)
