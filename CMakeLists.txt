#------------------------------------------------------------------------------
# User-settable variables and options.
#
# Note that defaults for cmake builtin variables must be set before calling
# project(), otherwise the cmake-provided defaults will get in first!

set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
)

project(displaz)

cmake_minimum_required(VERSION 2.8)

option(DISPLAZ_STATIC "Build against static libraries" FALSE)
option(DISPLAZ_USE_BUILTIN_SHADERS "Package shaders into executable" TRUE)
option(DISPLAZ_USE_PDAL "Use PDAL for reading las files" FALSE)
option(DISPLAZ_USE_TESTS "Build the test cases" TRUE)

#------------------------------------------------------------------------------
# Find third party dependencies

# Find a library to read .las or .laz
if (DISPLAZ_USE_PDAL)
    find_path (PDAL_INCLUDES NAMES pdal/pdal_defines.h)
    find_library (PDAL_LIBRARIES NAMES pdal)
    if (PDAL_LIBRARIES AND PDAL_INCLUDES)
        set (PDAL_FOUND TRUE)
    endif()

    if (PDAL_FOUND)
        find_package (Boost 1.48)
        if (NOT Boost_FOUND)
            message (SEND_ERROR "Boost headers are required when compiling with PDAL")
        endif()
        list (APPEND PDAL_INCLUDES ${Boost_INCLUDE_DIRS})
    endif()
else()
    # laslib support
    find_path (LASLIB_INCLUDES NAMES lasdefinitions.hpp)
    find_library (LASLIB_LIBRARIES NAMES laslib)
    if (LASLIB_LIBRARIES AND LASLIB_INCLUDES)
        set (LASLIB_FOUND TRUE)
    endif()
endif()

if(NOT LASLIB_FOUND AND NOT PDAL_FOUND)
    message(SEND_ERROR "One of PDAL or laslib is required to read point cloud files")
endif()


# Find IlmBase - temporary dependency?
find_path (ILMBASE_INCLUDES NAMES ImathVec.h)
if (ILMBASE_INCLUDES)
    set (ILMBASE_FOUND TRUE)
endif()

find_package(OpenGL REQUIRED)
find_package(Qt4 4.7 COMPONENTS QtCore QtGui QtOpenGL)

if(NOT QT_FOUND)
    message(SEND_ERROR "Qt 4 not found")
endif()

if(DISPLAZ_USE_PDAL)
    add_definitions(-DDISPLAZ_USE_PDAL)
    include_directories(${PDAL_INCLUDES})
else()
    include_directories(${LASLIB_INCLUDES})
endif()

#------------------------------------------------------------------------------

include_directories(BEFORE
    src/thirdparty
    ${QT_INCLUDES}
    ${OPENGL_INCLUDE_DIR}
    ${ILMBASE_INCLUDES}
)

qt4_wrap_cpp(moc_srcs
    src/displazserver.h
    src/dragspinbox.h
    src/interactivecamera.h
    src/mainwindow.h
    src/ptview.h
    src/pointarray.h
    src/shadereditor.h
    src/shader.h
)
qt4_add_resources(rcc_srcs
    package/resources.qrc
)
set(srcs
    ${moc_srcs}
    ${rcc_srcs}
    src/mainwindow.cpp
    src/ptview.cpp
    src/pointarray.cpp
    src/thirdparty/argparse.cpp
    src/main.cpp
    src/mesh.cpp
    src/shadereditor.cpp
    src/shader.cpp
    src/thirdparty/rply/rply.c
    src/util.cpp
    src/glutil.cpp
)
if(WIN32)
    list(APPEND srcs package/windows_resources.rc)
endif()

if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "-Wall -std=c++0x")
else()
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS)
endif()

# Create config.h
if(DISPLAZ_USE_BUILTIN_SHADERS)
    set(DISPLAZ_SHADER_BASE_PATH ":")
else()
    set(DISPLAZ_SHADER_BASE_PATH ${PROJECT_SOURCE_DIR})
endif()
configure_file(src/config.h.in.cmake config.h @ONLY)
include_directories(${CMAKE_BINARY_DIR})


add_executable(displaz WIN32 ${srcs})

target_link_libraries(displaz
    ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTOPENGL_LIBRARY}
    ${QT_QTNETWORK_LIBRARY} ${OPENGL_gl_LIBRARY}
)

if (DISPLAZ_USE_PDAL)
    target_link_libraries (displaz ${PDAL_LIBRARIES})
else()
    target_link_libraries (displaz ${LASLIB_LIBRARIES})
endif()


if(WIN32)
    target_link_libraries(displaz
        ${QT_QTMAIN_LIBRARY}
    )
    if(DISPLAZ_STATIC)
        # These extra libraries are needed on windows when linking against a
        # static Qt-4.8.? which has been built with the default options.
        target_link_libraries(displaz
            imm32.lib Ws2_32.lib Winmm.lib
        )
    endif()
endif()


if(WIN32)
    install(TARGETS displaz DESTINATION "")
else()
    install(TARGETS displaz DESTINATION "bin")
endif()


#------------------------------------------------------------------------------
# Documentation
find_program(RST2HTML rst2html)
if (RST2HTML)
    set (readmeHtml ${PROJECT_BINARY_DIR}/README.html)
    add_custom_command(OUTPUT ${readmeHtml}
        COMMAND rst2html ${PROJECT_SOURCE_DIR}/README.rst > ${readmeHtml}
        DEPENDS README.rst
    )
    add_custom_target(doc ALL DEPENDS ${readmeHtml})
endif()


#------------------------------------------------------------------------------
# Tests
if (DISPLAZ_USE_TESTS)
    enable_testing()
    add_executable(util_test src/util_test.cpp)
    add_test(util_test util_test)
endif()


#------------------------------------------------------------------------------
# Packaging, mainly for windows really

set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.rst")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A viewer for geospatial lidar data")
#set(CPACK_PACKAGE_DESCRIPTION_FILE ".....")
set(CPACK_PACKAGE_ICON "package/displaz.ico")
set(CPACK_MONOLITHIC_INSTALL TRUE)
if (WIN32)
    set(CPACK_GENERATOR NSIS)
else()
    set(CPACK_GENERATOR TGZ)
endif()
set(CPACK_STRIP_FILES TRUE)

include(CPack)

