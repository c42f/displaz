project(external_libs)

include(ExternalProject)

set(download_dir ${PROJECT_SOURCE_DIR}/downloads)

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
    URL http://download.savannah.nongnu.org/releases/openexr/ilmbase-2.0.1.tar.gz
    URL_MD5 74c0d0d2873960bd0dc1993f8e03f0ae
    PATCH_COMMAND "${patch_executable}" -p1 < "${PROJECT_SOURCE_DIR}/ilmbase-2.0.1-build.patch"
    CMAKE_ARGS
        -D CMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
        -D CMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -D CMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
    INSTALL_DIR "${THIRDPARTY_INSTALL_DIR}"
    DOWNLOAD_DIR "${download_dir}"
)

ExternalProject_Add(
    laslib
    URL http://www.cs.unc.edu/~isenburg/lastools/download/lastools.zip
    PATCH_COMMAND cmake -E copy ${PROJECT_SOURCE_DIR}/laslib-CMakeLists.txt CMakeLists.txt && "${patch_executable}" -p1 < "${PROJECT_SOURCE_DIR}/lastools-131105-build.patch"
    CMAKE_ARGS
        -D CMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
        -D CMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -D CMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
    INSTALL_DIR "${THIRDPARTY_INSTALL_DIR}"
    DOWNLOAD_DIR "${download_dir}"
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
