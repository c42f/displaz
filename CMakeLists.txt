project(displaz)

cmake_minimum_required(VERSION 2.8)

option(DISPLAZ_STATIC "Build against static libraries" FALSE)
option(DISPLAZ_USE_BUILTIN_SHADERS "Package shaders into executable" TRUE)


#-----------------------------------------------------------
# Find laslib
find_path (LASLIB_INCLUDES NAMES lasdefinitions.hpp)
find_library (LASLIB_LIBRARIES NAMES laslib)
if (LASLIB_LIBRARIES AND LASLIB_INCLUDES)
    set (LASLIB_FOUND TRUE)
endif()

#-----------------------------------------------------------
# Find IlmBase - probably temporary dependency

find_path (ILMBASE_INCLUDES NAMES ImathVec.h)
if (ILMBASE_INCLUDES)
    set (ILMBASE_FOUND TRUE)
endif()

#-----------------------------------------------------------

find_package(OpenGL)
find_package(Qt4 4.7 COMPONENTS QtCore QtGui QtOpenGL)

if(NOT QT_FOUND OR NOT OPENGL_FOUND OR NOT LASLIB_FOUND OR NOT ILMBASE_FOUND)
    message(SEND_ERROR "Qt, opengl, glew, laslib and ilmbase are required.")
endif()

include_directories(BEFORE
    ${QT_INCLUDES}
    ${OPENGL_INCLUDE_DIR}
    ${LASLIB_INCLUDES}
    ${ILMBASE_INCLUDES}
)

qt4_wrap_cpp(moc_srcs
    dragspinbox.h
    interactivecamera.h
    mainwindow.h
    ptview.h
    pointarray.h
    shadereditor.h
    shader.h
)
qt4_add_resources(rcc_srcs
    resources.qrc
)
set(srcs
    ${moc_srcs}
    ${rcc_srcs}
    mainwindow.cpp
    ptview.cpp
    pointarray.cpp
    argparse.cpp
    main.cpp
    mesh.cpp
    shadereditor.cpp
    shader.cpp
    rply/rply.c
    util.cpp
)
if(WIN32)
    list(APPEND srcs windows_resources.rc)
endif()

if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "-Wall -std=c++0x")
else()
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS)
endif()

set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
    FORCE
)

# Create config.h
if(DISPLAZ_USE_BUILTIN_SHADERS)
    set(DISPLAZ_SHADER_BASE_PATH ":")
else()
    set(DISPLAZ_SHADER_BASE_PATH ${PROJECT_SOURCE_DIR})
endif()
configure_file(config.h.in.cmake config.h @ONLY)
include_directories(${CMAKE_BINARY_DIR})


add_executable(displaz WIN32 ${srcs})

target_link_libraries(displaz
    ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTOPENGL_LIBRARY}
    ${OPENGL_gl_LIBRARY}
    ${LASLIB_LIBRARIES}
)

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
