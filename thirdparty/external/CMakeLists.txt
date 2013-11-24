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
    laszip
    URL https://github.com/LASzip/LASzip/archive/v2.2.0.tar.gz
    URL_MD5 5b8a7c713bd79e0d1d7f7cc8fe9e59bb
    CMAKE_ARGS
        -D CMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
        -D CMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -D CMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        -D BUILD_STATIC:BOOL=ON
    INSTALL_DIR ${THIRDPARTY_INSTALL_DIR}
    DOWNLOAD_DIR ${download_dir}/laszip
)

ExternalProject_Add(
    pdal
    # 3732267 =  fixes for static build, as of 2013-11-24
    URL https://github.com/c42f/PDAL/archive/3732267.tar.gz
    URL_MD5 88a71fa4894e553b98305d240e2e517b
    DOWNLOAD_DIR ${download_dir}/pdal
    CMAKE_ARGS
        -D CMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
        -D CMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -D CMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        -D CMAKE_PREFIX_PATH:STRING=${THIRDPARTY_INSTALL_DIR}
        -D PDAL_BUILD_STATIC:BOOL=TRUE
        -D WITH_LIBXML2:BOOL=OFF
        -D WITH_ICONV:BOOL=OFF
        -D WITH_PYTHON:BOOL=OFF
        -D WITH_NITRO:BOOL=OFF
        -D WITH_GEOTIFF:BOOL=OFF
        -D WITH_GDAL:BOOL=OFF
        -D WITH_PGPOINTCLOUD:BOOL=OFF
        -D WITH_TESTS:BOOL=OFF
        -D WITH_APPS:BOOL=OFF
        -D WITH_LASZIP:BOOL=ON
        -D WITH_STATIC_LASZIP:BOOL=ON
        -D PDAL_EMBED_BOOST:BOOL=TRUE
    DEPENDS laszip
    INSTALL_DIR ${THIRDPARTY_INSTALL_DIR}
)

