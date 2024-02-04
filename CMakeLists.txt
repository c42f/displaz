#------------------------------------------------------------------------------
# User-settable variables and options.
#
# Note that defaults for cmake builtin variables must be set before calling
# project(), otherwise the cmake-provided defaults will get in first!

set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
)

if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "-Wall" CACHE STRING "Flags used by the compiler during all build types.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(displaz)

cmake_minimum_required(VERSION 3.5)

if (WIN32)
    # cmake >= 2.8.10 needs this for Qt5 on Windows
    cmake_policy(SET CMP0020 NEW)
endif()

# cmake >= 3.11 prefer GLVND libraries for OpenGL
cmake_policy(SET CMP0072 NEW)

option(DISPLAZ_STATIC "Build against static libraries" FALSE)
option(DISPLAZ_USE_LAS "Build with support for reading las files" TRUE)
option(DISPLAZ_USE_PDAL "Use PDAL for reading las files" FALSE)
option(DISPLAZ_USE_TESTS "Build the test cases" TRUE)
option(DISPLAZ_BUILD_DVOX "Build experimential dvox utility" FALSE)
option(DISPLAZ_EMBED_GLEW "Build GLEW as part of the displaz build process" TRUE)

set(DISPLAZ_EXTERNAL_DIR "${PROJECT_BINARY_DIR}/../build_external/dist"
    CACHE PATH "Directory containing installed third party libraries (laslib, ilmbase, ...)")

set(DISPLAZ_BUILD_NUMBER "dev"
    CACHE STRING "Build version number suffix for automated builds")
mark_as_advanced(DISPLAZ_BUILD_NUMBER)

#------------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# More handy location for intermediate build artifacts
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (DISPLAZ_USE_TESTS)
    enable_testing()
endif()

#------------------------------------------------------------------------------
# Find third party dependencies

if (EXISTS ${DISPLAZ_EXTERNAL_DIR})
    list (APPEND CMAKE_PREFIX_PATH ${DISPLAZ_EXTERNAL_DIR})
else()
    message(STATUS "DISPLAZ_EXTERNAL_DIR (= \"${DISPLAZ_EXTERNAL_DIR}\") does not exist.  If you would like LASlib and ilmbase to be built for you, please run the cmake build system in thirdparty/external (see build instructions for details)")
endif()

find_package(Imath 1.0.1 REQUIRED)

if (DISPLAZ_USE_LAS)
    add_definitions(-DDISPLAZ_USE_LAS)
    # Find a library to read .las or .laz
    if (DISPLAZ_USE_PDAL)
        find_package(PDAL REQUIRED)
        find_package(Boost 1.48 REQUIRED)
        add_definitions(-DDISPLAZ_USE_PDAL)
        include_directories(${PDAL_INCLUDE_DIRS})
    else()
        # laslib support
        find_package(LASlib REQUIRED)
        include_directories(${LASLIB_INCLUDE_DIRS})
    endif()
endif()

if (DISPLAZ_EMBED_GLEW)
    add_definitions(-DGLEW_STATIC -DGLEW_NO_GLU)
    set(GLEW_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/glew)
    include_directories(BEFORE ${GLEW_INCLUDE_DIR})
    add_library(embeddedGLEW STATIC thirdparty/glew/glew.c)
    set(GLEW_LIBRARIES embeddedGLEW)
else()
    find_package(GLEW)
endif()

find_package(OpenGL REQUIRED)

find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(Qt5Widgets REQUIRED)
add_definitions(${Qt5Core_DEFINITIONS})
add_definitions(${Qt5Gui_DEFINITIONS})
add_definitions(${Qt5Network_DEFINITIONS})
add_definitions(${Qt5Widgets_DEFINITIONS})

if (NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS} ${Qt5Gui_EXECUTABLE_COMPILE_FLAGS} ${Qt5Network_EXECUTABLE_COMPILE_FLAGS} ${Qt5OpenGL_EXECUTABLE_COMPILE_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
endif()


include_directories(${IMATH_INCLUDE_DIRS})


#------------------------------------------------------------------------------
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


#------------------------------------------------------------------------------
# Determine current displaz version
include(GitUtils)
set(displazVersion "0.3")
if (GIT_FOUND)
    git_describe(displazVersionGit)
    if (displazVersionGit)
        # For better or worse, I've been tagging as v0.1 - strip off the "v"
        string(REGEX REPLACE "^v" "" displazVersion "${displazVersionGit}")
    endif()
    message(STATUS "Building displaz version ${displazVersion}")
else()
    message(WARNING "Git not found - assuming hardcoded version ${displazVersion}")
endif()


#------------------------------------------------------------------------------
# Build sources
add_subdirectory(src)


#------------------------------------------------------------------------------
# Create remainder of install tree
install(DIRECTORY test/ DESTINATION "${DISPLAZ_DOC_DIR}/examples")
install(DIRECTORY shaders/ DESTINATION "${DISPLAZ_SHADER_DIR}")
install(DIRECTORY thirdparty/licenses/ DESTINATION "${DISPLAZ_DOC_DIR}/thirdparty_licenses")


#------------------------------------------------------------------------------
# Documentation
find_program(RST2HTML NAMES rst2html rst2html.py)
if (RST2HTML)
    set (readmeHtml ${PROJECT_BINARY_DIR}/README.html)
    add_custom_command(OUTPUT ${readmeHtml}
        COMMAND ${RST2HTML} ${PROJECT_SOURCE_DIR}/README.rst ${readmeHtml}
        DEPENDS README.rst
    )
    set (userguideHtml ${PROJECT_BINARY_DIR}/userguide.html)
    add_custom_command(OUTPUT ${userguideHtml}
        COMMAND ${RST2HTML} ${PROJECT_SOURCE_DIR}/doc/userguide.rst ${userguideHtml}
        DEPENDS doc/userguide.rst
    )
    add_custom_target(doc ALL DEPENDS ${readmeHtml} ${userguideHtml})
    install(FILES ${readmeHtml} ${userguideHtml} DESTINATION "${DISPLAZ_DOC_DIR}")
else()
    message(WARNING "rst2html not found - documentation will be installed as text!")
    install(FILES README.rst DESTINATION "${DISPLAZ_DOC_DIR}")
    install(FILES doc/userguide.rst DESTINATION "${DISPLAZ_DOC_DIR}")
endif()


#------------------------------------------------------------------------------
# Packaging, mostly just for windows at this stage.

# Ensure we can find NSIS.InstallOptions.ini.in
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/package")

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${DISPLAZ_BIN_DIR}")
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "displaz")
set(CPACK_PACKAGE_VENDOR "Displaz Team")
set(CPACK_PACKAGE_VERSION "${displazVersion}+${DISPLAZ_BUILD_NUMBER}")
set(fullVersion "displaz-${displazVersion}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A viewer for geospatial lidar data")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.rst")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.rst")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Displaz")
if (WIN32)
    # Some paths need at at least one backslash to workaround NSIS bug
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/package\\\\displaz.ico")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/package\\\\displaz.ico")
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

