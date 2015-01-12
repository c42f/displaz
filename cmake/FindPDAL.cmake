#---
# File: FindPDAL.cmake
#
# Find the native Point Data Abstractin Library(PDAL) includes and libraries.
#
# This module defines:
#
# PDAL_INCLUDE_DIR, where to find pdal.h, etc.
# PDAL_LIBRARY, libraries to link against to use PDAL.
# PDAL_FOUND, True if found, false if one of the above are not found.
#
#---
# Find include path:  "pdal_defines.h" installs to install "prefix" with pdal
# includes under "pdal" sub directory.
#---
find_path( PDAL_INCLUDE_DIR pdal/pdal_defines.h
           PATHS
           /usr/include
           /usr/local/include )

# Find PDAL library:
find_library( PDAL_LIBRARY NAMES pdalcpp pdal
              PATHS
              /usr/lib64
              /usr/lib
              /usr/local/lib )

#---
# This function sets PDAL_FOUND if variables are valid.
#---
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args( PDAL DEFAULT_MSG
                                   PDAL_LIBRARY
                                   PDAL_INCLUDE_DIR )

if( PDAL_FOUND )
   if( NOT PDAL_FIND_QUIETLY )
      message( STATUS "Found PDAL..." )
   endif( NOT PDAL_FIND_QUIETLY )
else( PDAL_FOUND )
   if( NOT PDAL_FIND_QUIETLY )
      message( WARNING "Could not find PDAL" )
   endif( NOT PDAL_FIND_QUIETLY )
endif( PDAL_FOUND )

if( NOT PDAL_FIND_QUIETLY )
   message( STATUS "PDAL_INCLUDE_DIR=${PDAL_INCLUDE_DIR}" )
   message( STATUS "PDAL_LIBRARY=${PDAL_LIBRARY}" )
endif( NOT PDAL_FIND_QUIETLY )
