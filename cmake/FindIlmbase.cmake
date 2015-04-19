# Find ilmbase
#
# This sets:
#   - ILMBASE_FOUND:  system has ilmbase
#   - ILMBASE_INCLUDE_DIRS: the ilmbase include directories
#   - ILMBASE_LIBRARIES: the ilmbase libraries
#   - ILMBASE_VERSION: the version string for ilmbase

find_path(ILMBASE_INCLUDE_DIRS OpenEXR/IlmBaseConfig.h)
find_library(ILMBASE_IMATH_LIBRARY Imath)
find_library(ILMBASE_IEX_LIBRARY Iex)

set(ILMBASE_LIBRARIES ${ILMBASE_IMATH_LIBRARY} ${ILMBASE_IEX_LIBRARY})

file(STRINGS ${ILMBASE_INCLUDE_DIRS}/OpenEXR/IlmBaseConfig.h version_line
     REGEX "ILMBASE_VERSION_STRING|PACKAGE_VERSION")
string(REGEX MATCH "[0-9]+.[0-9]+.[0-9]+" ILMBASE_VERSION_STRING ${version_line})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Ilmbase
    FOUND_VAR ILMBASE_FOUND
    REQUIRED_VARS ILMBASE_INCLUDE_DIRS ILMBASE_IMATH_LIBRARY ILMBASE_IEX_LIBRARY
    VERSION_VAR ILMBASE_VERSION_STRING
    )

mark_as_advanced(ILMBASE_IMATH_LIBRARY ILMBASE_IEX_LIBRARY ILMBASE_INCLUDE_DIRS)
