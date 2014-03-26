project(viewer)

include_directories(BEFORE
    ../thirdparty
    ${GLEW_INCLUDE_DIR}
    ${QT_INCLUDES}
    ${OPENGL_INCLUDE_DIR}
    ${THIRDPARTY_INSTALL_DIR}/include
)

#------------------------------------------------------------------------------
# Sources
qt4_wrap_cpp(moc_srcs
    datasetui.h
    displazserver.h
    dragspinbox.h
    fileloader.h
    geometry.h
    geometrycollection.h
    helpdialog.h
    interactivecamera.h
    logger.h
    mainwindow.h
    view3d.h
    pointarray.h
    shadereditor.h
    shader.h
)
set(srcs
    ${moc_srcs}
    datasetui.cpp
    geometry.cpp
    geomfield.cpp
    geometrycollection.cpp
    glutil.cpp
    logger.cpp
    main.cpp
    mainwindow.cpp
    mesh.cpp
    ply_io.cpp
    las_io.cpp
    pointarray.cpp
    typespec.cpp
    view3d.cpp
    shadereditor.cpp
    shader.cpp
    util.cpp
    ../thirdparty/argparse.cpp
    ../thirdparty/rply/rply.c
)
if(WIN32)
    list(APPEND srcs ../package/windows_resources.rc)
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
configure_file(config.h.in.cmake config.h @ONLY)
include_directories(${PROJECT_BINARY_DIR})


add_executable(displaz WIN32 ${srcs})


#-------------------------------------------------------------------------------
# Library dependencies
if (DISPLAZ_BUILD_EXTERNAL)
    add_dependencies(displaz ${externalBuilds})
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


# Install
install(TARGETS displaz DESTINATION "${DISPLAZ_BIN_DIR}")


#------------------------------------------------------------------------------
# Tests
if (DISPLAZ_USE_TESTS)
    add_executable(util_test util_test.cpp)
    if (DISPLAZ_BUILD_EXTERNAL)
        add_dependencies(util_test ${externalBuilds})
    endif()
    add_test(util_test util_test)
endif()


