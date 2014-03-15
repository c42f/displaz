#------------------------------------------------------------------------------
# User-settable variables and options.
#
# Note that defaults for cmake builtin variables must be set before calling
# project(), otherwise the cmake-provided defaults will get in first!

set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
)

if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "-Wall -std=c++0x" CACHE STRING "Flags used by the compiler during all build types.")
endif()

project(displaz)

cmake_minimum_required(VERSION 2.8)

set(defaultEmbedGlew FALSE)
if (WIN32)
    set(defaultEmbedGlew TRUE)
endif()

option(DISPLAZ_STATIC "Build against static libraries" FALSE)
option(DISPLAZ_USE_LAS "Build with support for reading las files" TRUE)
option(DISPLAZ_USE_PDAL "Use PDAL for reading las files" FALSE)
option(DISPLAZ_USE_TESTS "Build the test cases" TRUE)
option(DISPLAZ_BUILD_EXTERNAL "Automatically download and build external libraries (ilmbase, laslib)" TRUE)
option(DISPLAZ_EMBED_GLEW "Build GLEW as part of the displaz build process" ${defaultEmbedGlew})

# Install directory for binaries
# (It would seem sensible to avoid a bin directory on windows, but in practise
# the NSIS installer expects binaries to be there.)
set(DISPLAZ_BIN_DIR "bin")
if (WIN32)
    set(DISPLAZ_SHADER_DIR "shaders")
    set(DISPLAZ_DOC_DIR "doc")
else()
    set(DISPLAZ_SHADER_DIR "share/displaz/shaders")
    set(DISPLAZ_DOC_DIR "share/doc/displaz")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

#------------------------------------------------------------------------------
# Find third party dependencies

if (DISPLAZ_BUILD_EXTERNAL)
    set(THIRDPARTY_INSTALL_DIR ${PROJECT_BINARY_DIR}/thirdparty_install)
    set(ILMBASE_INCLUDES ${THIRDPARTY_INSTALL_DIR}/include)
    set(external_builds ilmbase laslib)
    if (DISPLAZ_USE_LAS)
        set(LASLIB_INCLUDES ${THIRDPARTY_INSTALL_DIR}/include)
        set(LASLIB_LIBRARIES ${THIRDPARTY_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}laslib${CMAKE_STATIC_LIBRARY_SUFFIX})
    endif()
    add_subdirectory(thirdparty/external)
else()
    if (DISPLAZ_USE_LAS)
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
    endif()

    # Find IlmBase
    find_path (ILMBASE_INCLUDES NAMES OpenEXR/ImathVec.h)
    if (ILMBASE_INCLUDES)
        set (ILMBASE_FOUND TRUE)
    endif()
endif()

if (DISPLAZ_EMBED_GLEW)
    add_definitions(-DGLEW_STATIC)
    add_library(embeddedGLEW STATIC thirdparty/glew/glew.c)
    set(GLEW_INCLUDE_DIR thirdparty/glew)
    set(GLEW_LIBRARIES embeddedGLEW)
else()
    find_package(GLEW)
endif()

find_package(OpenGL REQUIRED)
find_package(Qt4 4.7 COMPONENTS QtCore QtGui QtOpenGL)

if(NOT QT_FOUND)
    message(SEND_ERROR "Qt 4 not found")
endif()

if (DISPLAZ_USE_LAS)
    add_definitions(-DDISPLAZ_USE_LAS)
    if(DISPLAZ_USE_PDAL)
        add_definitions(-DDISPLAZ_USE_PDAL)
        include_directories(${PDAL_INCLUDES})
    else()
        include_directories(${LASLIB_INCLUDES})
    endif()
endif()

#------------------------------------------------------------------------------

include_directories(BEFORE
    thirdparty
    ${GLEW_INCLUDE_DIR}
    ${QT_INCLUDES}
    ${OPENGL_INCLUDE_DIR}
    ${THIRDPARTY_INSTALL_DIR}/include
)

qt4_wrap_cpp(moc_srcs
    src/datasetui.h
    src/displazserver.h
    src/dragspinbox.h
    src/fileloader.h
    src/geometry.h
    src/geometrycollection.h
    src/helpdialog.h
    src/interactivecamera.h
    src/logger.h
    src/mainwindow.h
    src/ptview.h
    src/pointarray.h
    src/shadereditor.h
    src/shader.h
)
set(srcs
    ${moc_srcs}
    src/datasetui.cpp
    src/geometry.cpp
    src/geometrycollection.cpp
    src/glutil.cpp
    src/logger.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/mesh.cpp
    src/ply_io.cpp
    src/las_io.cpp
    src/pointarray.cpp
    src/pointfield.cpp
    src/ptview.cpp
    src/shadereditor.cpp
    src/shader.cpp
    src/util.cpp
    thirdparty/argparse.cpp
    thirdparty/rply/rply.c
)
if(WIN32)
    list(APPEND srcs package/windows_resources.rc)
endif()

if(WIN32)
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS)
endif()

# Create config.h
find_package(Git)
set(displazVersion "0.2")
if (GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe
                    OUTPUT_VARIABLE displazVersionGit
                    RESULT_VARIABLE gitDescribeResult)
    if (gitDescribeResult STREQUAL "0")
        string(STRIP "${displazVersionGit}" displazVersion)
        # For better or worse, I've been tagging as v0.1 - strip off the "v"
        string(REGEX REPLACE "^v" "" displazVersion "${displazVersion}")
    endif()
else()
    message(WARNING "Git not found - assuming hardcoded version ${displazVersion}")
endif()
set(DISPLAZ_VERSION_STRING "${displazVersion}")
configure_file(src/config.h.in.cmake config.h @ONLY)
include_directories(${CMAKE_BINARY_DIR})


add_executable(displaz WIN32 ${srcs})

if (DISPLAZ_BUILD_EXTERNAL)
    add_dependencies(displaz ${external_builds})
endif()

target_link_libraries(displaz
    ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTOPENGL_LIBRARY}
    ${QT_QTNETWORK_LIBRARY} ${OPENGL_gl_LIBRARY} ${GLEW_LIBRARIES}
)
if (DISPLAZ_USE_LAS)
    if (DISPLAZ_USE_PDAL)
        target_link_libraries (displaz ${PDAL_LIBRARIES})
    else()
        target_link_libraries (displaz ${LASLIB_LIBRARIES})
    endif()
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


install(TARGETS displaz DESTINATION "${DISPLAZ_BIN_DIR}")
install(DIRECTORY shaders/ DESTINATION "${DISPLAZ_SHADER_DIR}")

install(DIRECTORY thirdparty/licenses/ DESTINATION "${DISPLAZ_DOC_DIR}/thirdparty_licenses")

if(WIN32)
    # Install required qt libraries
    install(CODE "
        include(BundleUtilities)
        fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${DISPLAZ_BIN_DIR}/displaz${CMAKE_EXECUTABLE_SUFFIX}\" \"\" \"${QT_LIBRARY_DIR}\")
    ")
endif()


#------------------------------------------------------------------------------
# Documentation
find_program(RST2HTML NAMES rst2html rst2html.py)
if (RST2HTML)
    set (readmeHtml ${PROJECT_BINARY_DIR}/README.html)
    add_custom_command(OUTPUT ${readmeHtml}
        COMMAND ${RST2HTML} ${PROJECT_SOURCE_DIR}/README.rst ${readmeHtml}
        DEPENDS README.rst
    )
    add_custom_target(doc ALL DEPENDS ${readmeHtml})
    install(FILES ${readmeHtml} DESTINATION "${DISPLAZ_DOC_DIR}")
else()
    message(WARNING "rst2html not found - documentation will be installed as text!")
    install(FILES README.rst DESTINATION "${DISPLAZ_DOC_DIR}")
endif()


#------------------------------------------------------------------------------
# Tests
if (DISPLAZ_USE_TESTS)
    enable_testing()
    add_executable(util_test src/util_test.cpp)
    if (DISPLAZ_BUILD_EXTERNAL)
        add_dependencies(util_test ${external_builds})
    endif()
    add_test(util_test util_test)
endif()


#------------------------------------------------------------------------------
# Packaging, mostly just for windows at this stage.

# Ensure we can find NSIS.InstallOptions.ini.in
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/package")

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${DISPLAZ_BIN_DIR}")
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "displaz")
set(CPACK_PACKAGE_VERSION "${displazVersion}")
set(fullVersion "displaz-${displazVersion}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A viewer for geospatial lidar data")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.rst")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.rst")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")
if (WIN32)
    # Some paths need at at least one backslash to workaround NSIS bug
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/package\\\\displaz.ico")
else()
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/package/displaz.ico")
endif()
set(CPACK_PACKAGE_EXECUTABLES "displaz;Displaz")
set(CPACK_MONOLITHIC_INSTALL TRUE)
if (WIN32)
    # NSIS Setup
    set(CPACK_GENERATOR NSIS)
    set(CPACK_NSIS_MODIFY_PATH TRUE)
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "${DISPLAZ_BIN_DIR}")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL TRUE)
    # Registry guff to set up file associations
    set(CPACK_NSIS_DEFINES "
    !include ${PROJECT_SOURCE_DIR}\\\\package\\\\FileAssoc.nsh
    Var ASSOCIATE_FILES
    ")
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
    !insertmacro MUI_INSTALLOPTIONS_READ \\\$ASSOCIATE_FILES \\\"NSIS.InstallOptions.ini\\\" \\\"Field 6\\\" \\\"State\\\"

    StrCmp \\\"$ASSOCIATE_FILES\\\" \\\"0\\\" fileAssocSkip
    !insertmacro APP_ASSOCIATE \\\"laz\\\" \\\"${fullVersion}.laz\\\" \\\"Laser point cloud\\\" \\\"\\\$INSTDIR\\\\bin\\\\displaz.exe,0\\\" \\\"Open with displaz\\\" \\\"\\\$INSTDIR\\\\bin\\\\displaz.exe \\\$\\\\\\\"%1\\\$\\\\\\\"\\\"
    !insertmacro APP_ASSOCIATE \\\"las\\\" \\\"${fullVersion}.las\\\" \\\"Laser point cloud\\\" \\\"\\\$INSTDIR\\\\bin\\\\displaz.exe,0\\\" \\\"Open with displaz\\\" \\\"\\\$INSTDIR\\\\bin\\\\displaz.exe \\\$\\\\\\\"%1\\\$\\\\\\\"\\\"
    !insertmacro UPDATEFILEASSOC
    fileAssocSkip:
    ")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
    !insertmacro APP_UNASSOCIATE \\\"laz\\\" \\\"${fullVersion}.laz\\\"
    !insertmacro APP_UNASSOCIATE \\\"las\\\" \\\"${fullVersion}.las\\\"
    !insertmacro UPDATEFILEASSOC
    ")
    # cpack bug workaround: avoid installing into "C:\Program Files (x86)"
    set(CPACK_NSIS_INSTALL_ROOT "C:\\\\Program Files")
else()
    set(CPACK_GENERATOR TGZ)
endif()
set(CPACK_STRIP_FILES TRUE)

include(CPack)

