project(viewer)

include_directories(BEFORE
    .
    gui
    render
    ../thirdparty
    ${GLEW_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
    ${THIRDPARTY_INSTALL_DIR}/include
)

if(DISPLAZ_USE_QT4)
    set(QT_INCLUDE_DIR_EXTENDED)
    foreach(l ${QT_INCLUDE_DIR})
        list(APPEND QT_INCLUDE_DIR_EXTENDED ${l}/QtCore )
        list(APPEND QT_INCLUDE_DIR_EXTENDED ${l}/QtGui )
        list(APPEND QT_INCLUDE_DIR_EXTENDED ${l}/QtNetwork )
        list(APPEND QT_INCLUDE_DIR_EXTENDED ${l}/QtOpenGL )
    endforeach()
    include_directories(
        ${QT_INCLUDES}
        ${QT_INCLUDE_DIR}
        ${QT_INCLUDE_DIR_EXTENDED}
    )
else()
    include_directories(
        ${Qt5Core_INCLUDE_DIRS}
        ${Qt5GUI_INCLUDE_DIRS}
        ${Qt5Network_INCLUDE_DIRS}
        ${Qt5OpenGL_INCLUDE_DIRS}
        ${Qt5Widgets_INCLUDE_DIRS}
    )
endif()

#------------------------------------------------------------------------------
# Sources

# Generic utility stuff
set(util_srcs
    logger.cpp
    geomfield.cpp
    typespec.cpp
    hcloud.cpp
    util.cpp
    ../thirdparty/argparse.cpp
)

macro(displaz_qt_wrap_cpp moc_out_srcs)
    if(DISPLAZ_USE_QT4)
        qt4_wrap_cpp(${moc_out_srcs} ${ARGN})
    else()
        qt5_wrap_cpp(${moc_out_srcs} ${ARGN})
    endif()
endmacro()

# Interprocess communication
displaz_qt_wrap_cpp(ipc_moc_srcs IpcChannel.h)

set(ipc_srcs
    ${ipc_moc_srcs}
    InterProcessLock.cpp
    IpcChannel.cpp
)

# GUI
displaz_qt_wrap_cpp(gui_moc_srcs
    fileloader.h
    geometrycollection.h
    HookFormatter.h
    HookManager.h

    gui/DataSetUI.h
    gui/DragSpinBox.h
    gui/GLSLHighlighter.h
    gui/HelpDialog.h
    gui/InteractiveCamera.h
    gui/PointViewerMainWindow.h
    gui/QtLogger.h
    gui/ShaderEditor.h

    render/Geometry.h
    render/HCloudView.h
    render/View3D.h
    render/PointArray.h
    render/Shader.h
    render/GeometryMutator.h
)

set(gui_srcs
    ${util_srcs}
    ${ipc_srcs}
    ${gui_moc_srcs}
    DrawCostModel.cpp
    geometrycollection.cpp
    ply_io.cpp
    las_io.cpp
    PolygonBuilder.cpp
    HookFormatter.cpp
    HookManager.cpp

    gui/DataSetUI.cpp
    gui/GLSLHighlighter.cpp
    gui/guimain.cpp
    gui/PointViewerMainWindow.cpp
    gui/QtLogger.cpp
    gui/ShaderEditor.cpp

    render/Geometry.cpp
    render/glutil.cpp
    render/HCloudView.cpp
    render/TriMesh.cpp
    render/PointArray.cpp
    render/View3D.cpp
    render/Shader.cpp
    render/GeometryMutator.cpp
    render/Annotation.cpp

    ../thirdparty/rply/rply.c
    ../thirdparty/polypartition/polypartition.cpp
)

set(cli_srcs
    ${util_srcs}
    ${ipc_srcs}
    main.cpp
)

if(DISPLAZ_USE_QT4)
    qt4_add_resources(RCC_GENERATED resource.qrc)
else()
    qt5_add_resources(RCC_GENERATED resource.qrc)
endif()

if(WIN32)
    list(APPEND gui_srcs ../package/windows_resources.rc)
endif()

if(WIN32)
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS)
endif()

# Create config.h
set(DISPLAZ_VERSION_STRING "${displazVersion}")
configure_file(config.h.in.cmake config.h @ONLY)
include_directories(${PROJECT_BINARY_DIR})


add_executable(displaz ${cli_srcs})
add_executable(displaz-gui WIN32 ${gui_srcs} ${RCC_GENERATED})

if (DISPLAZ_BUILD_DVOX)
    add_executable(dvox
        ${util_srcs}
        dvox.cpp
        pointdbwriter.cpp
        pointdb.cpp
        voxelizer.cpp
    )
endif()

#-------------------------------------------------------------------------------
# Library dependencies
if (DISPLAZ_USE_QT4)
    target_link_libraries(displaz
        ${QT_QTCORE_LIBRARY} ${QT_QTNETWORK_LIBRARY}
        ${ILMBASE_LIBRARIES}
    )
    target_link_libraries(displaz-gui
        ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTOPENGL_LIBRARY}
        ${QT_QTNETWORK_LIBRARY} ${QT_QTWIDGETS_LIBRARY}
        ${OPENGL_gl_LIBRARY} ${GLEW_LIBRARIES}
        ${ILMBASE_LIBRARIES}
    )
else()
    target_link_libraries(displaz
        Qt5::Core Qt5::Network Qt5::Widgets
        ${ILMBASE_LIBRARIES}
    )
    target_link_libraries(displaz-gui
        Qt5::Core Qt5::Gui Qt5::OpenGL
        Qt5::Network Qt5::Widgets
        ${OPENGL_gl_LIBRARY} ${GLEW_LIBRARIES}
        ${ILMBASE_LIBRARIES}
    )
    if (Qt5_POSITION_INDEPENDENT_CODE)
        set_target_properties(displaz-gui PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    endif()
    if (UNIX)
        # TODO this is a workaround for Ubuntu 16.04 x86-64 to fix issue #87
        add_definitions(-fPIC)
        # TODO this is a workaround for Ubuntu 16.10 x86-64 to fix issue #158
        add_definitions(-DQT_NO_VERSION_TAGGING)
    endif()
endif()

if(APPLE)
    target_link_libraries(displaz-gui
        ${FOUNDATION_LIBRARY} ${COCOA_LIBRARY}
    )
endif()

if (DISPLAZ_BUILD_DVOX)
    # TODO: Remove this dependency
    if (DISPLAZ_USE_QT4)
        target_link_libraries(dvox ${QT_QTCORE_LIBRARY})
    else()
        target_link_libraries(dvox Qt5::Core)
    endif()
endif()

if (DISPLAZ_USE_LAS)
    if (DISPLAZ_USE_PDAL)
        target_link_libraries (displaz-gui ${PDAL_LIBRARIES})
        if (DISPLAZ_BUILD_DVOX)
            message(SEND_ERROR "experimental dvox utility doesn't support pdal as a backend")
        endif()
    else()
        target_link_libraries (displaz-gui ${LASLIB_LIBRARIES})
        if (DISPLAZ_BUILD_DVOX)
            target_link_libraries (dvox ${LASLIB_LIBRARIES})
        endif()
    endif()
endif()


if(WIN32)
    if(DISPLAZ_STATIC)
        # These extra libraries are needed on windows when linking against a
        # static Qt-4.8.? which has been built with the default options.
        target_link_libraries(displaz
            imm32.lib Ws2_32.lib Winmm.lib
        )
    endif()
endif()


# Install
install(TARGETS displaz     DESTINATION "${DISPLAZ_BIN_DIR}")
install(TARGETS displaz-gui DESTINATION "${DISPLAZ_BIN_DIR}")

if(WIN32)
    include(DeployQt5)
    install_qt5_executable(${DISPLAZ_BIN_DIR}/displaz-gui${CMAKE_EXECUTABLE_SUFFIX})
endif()

if (DISPLAZ_BUILD_DVOX)
    install(TARGETS dvox DESTINATION "${DISPLAZ_BIN_DIR}")
endif()

#------------------------------------------------------------------------------
# Tests
if (DISPLAZ_USE_TESTS)
    add_executable(unit_tests
        ${util_srcs}
        streampagecache_test.cpp
        util_test.cpp
        test_main.cpp
    )
    add_test(NAME unit_tests COMMAND unit_tests)

    if (DISPLAZ_USE_QT4)
    else ()
        if (Qt5_POSITION_INDEPENDENT_CODE)
            set_target_properties(unit_tests PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
        endif()
    endif ()

    # Interprocess tests require special purpose executables
    add_executable(InterProcessLock_test InterProcessLock_test.cpp util.cpp InterProcessLock.cpp)
    if (DISPLAZ_USE_QT4)
        target_link_libraries(InterProcessLock_test ${QT_QTCORE_LIBRARY})
        target_link_libraries(unit_tests ${QT_QTCORE_LIBRARY})
    else ()
        target_link_libraries(InterProcessLock_test Qt5::Core)
        target_link_libraries(unit_tests Qt5::Core)
    endif ()
    add_test(NAME InterProcessLock_test COMMAND InterProcessLock_test master)
endif()
