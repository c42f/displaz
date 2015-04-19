# Build system for a subset of third party libraries required by displaz.

set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
)

if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "-std=c++0x"
        CACHE STRING "Flags used by the compiler during all build types.")
endif()

project(external_libs)

cmake_minimum_required(VERSION 2.8)

set(DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/downloads"
    CACHE PATH "Location for downloaded source archives")
set(LASLIB_URL "http://www.cs.unc.edu/~isenburg/lastools/download/LASlib.zip"
    CACHE STRING "URL of LASlib archive file (override if you want to use a local version)")
set(THIRDPARTY_INSTALL_PREFIX "dist"
    CACHE PATH "Location to install third party libraries")
set(CMAKE_INSTALL_PREFIX ${THIRDPARTY_INSTALL_PREFIX})

include(ExternalProject)

if (WIN32)
    find_package(Git)
    if (GIT_FOUND)
        get_filename_component(git_path "${GIT_EXECUTABLE}" PATH)
        set(patch_path_hint HINTS ${git_path}/../bin)
    endif()
endif()
find_program(patch_executable
    NAMES patch patch.exe
    ${patch_path_hint}
)
if (NOT patch_executable)
    message(SEND_ERROR
        "patch command not found - if you're on windows, try installing MinGW or git for windows.")
endif()

ExternalProject_Add(
    ilmbase
    URL http://download.savannah.nongnu.org/releases/openexr/ilmbase-2.2.0.tar.gz
    URL_MD5 b540db502c5fa42078249f43d18a4652
    CMAKE_ARGS
        -D CMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
        -D CMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -D CMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        -D NAMESPACE_VERSIONING:BOOL=OFF
        -D BUILD_SHARED_LIBS:BOOL=OFF
    INSTALL_DIR "${CMAKE_INSTALL_PREFIX}"
    DOWNLOAD_DIR "${DOWNLOAD_DIR}"
)

ExternalProject_Add(
    laslib
    URL ${LASLIB_URL}
    # Download URL isn't versioned => no URL_MD5 :-(
    PATCH_COMMAND cmake -E copy ${PROJECT_SOURCE_DIR}/laslib-CMakeLists.txt CMakeLists.txt && "${patch_executable}" -p1 < "${PROJECT_SOURCE_DIR}/lastools-131105-build.patch"
    CMAKE_ARGS
        -D CMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
        -D CMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -D CMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
    INSTALL_DIR "${CMAKE_INSTALL_PREFIX}"
    DOWNLOAD_DIR "${DOWNLOAD_DIR}"
)
# Tell cmake about build dependency
ExternalProject_Add_Step(laslib patchdeps
    DEPENDERS patch
    DEPENDS laslib-CMakeLists.txt lastools-131105-build.patch
)

# Don't build Qt here, because it's massive and available elsewhere on various
# platforms.  However, a nice win64 binary distribution of Qt < 5.0 isn't
# available.  Here's how you would configure it for win64 with msvc2012
#
# configure -release -no-webkit -nomake demos -nomake examples -opensource -platform win32-msvc2012
