project(viewer)

include_directories(BEFORE
    .
    gui
    render
    shader
    ../thirdparty
    ${GLEW_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
    ${THIRDPARTY_INSTALL_DIR}/include
)

include_directories(
    ${Qt5Core_INCLUDE_DIRS}
    ${Qt5GUI_INCLUDE_DIRS}
    ${Qt5Network_INCLUDE_DIRS}
    ${Qt5OpenGL_INCLUDE_DIRS}
    ${Qt5Widgets_INCLUDE_DIRS}
)

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
    qt5_wrap_cpp(${moc_out_srcs} ${ARGN})
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
    gui/MainWindow.h
    gui/QtLogger.h
    gui/ShaderEditor.h

    render/Geometry.h
    render/HCloudView.h
    render/View3D.h
    render/PointArray.h
    render/GeometryMutator.h

    shader/ShaderProgram.h
)

set(gui_srcs
    ${util_srcs}
    ${ipc_srcs}
    ${gui_moc_srcs}
    main.cpp
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
    gui/MainWindow.cpp
    gui/QtLogger.cpp
    gui/ShaderEditor.cpp

    render/Geometry.cpp
    render/glutil.cpp
    render/HCloudView.cpp
    render/TriMesh.cpp
    render/PointArray.cpp
    render/View3D.cpp
    render/GeometryMutator.cpp
    render/Annotation.cpp

    shader/Enable.cpp
    shader/Shader.cpp
    shader/ShaderProgram.cpp

    ../thirdparty/rply/rply.c
    ../thirdparty/polypartition/polypartition.cpp
)

qt5_add_resources(RCC_GENERATED resource.qrc)

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


add_executable(displaz WIN32 ${gui_srcs} ${RCC_GENERATED})
if (WIN32)
    # For windows, create a version of displaz which is linked as a console
    # program (ie, without WIN32 flag) in order have good behaviour from the
    # windows command line.  This version only needs to talk the IPC protocol,
    # so we disable the GUI code to avoid unnecessary bloat.
    #
    # To get this to be found before displaz.exe proper, we copy the trick used
    # in devenv, and create an executable displaz.com (this is not truly in the
    # old DOS .com format, but windows doesn't care about that).
    #
    # What a mess!  See issues #71, #84 and
    # https://blogs.msdn.microsoft.com/oldnewthing/20090101-00/?p=19643
    add_executable(displaz_com main.cpp ${util_srcs} ${ipc_srcs})
    set_target_properties(displaz_com PROPERTIES
        COMPILE_DEFINITIONS "DISPLAZ_CLI_ONLY"
        RUNTIME_OUTPUT_NAME "displaz"
        SUFFIX ".com"
    )
endif()

#-------------------------------------------------------------------------------
# Library dependencies
target_link_libraries(displaz
    Qt5::Core Qt5::Gui Qt5::OpenGL
    Qt5::Network Qt5::Widgets
    OpenGL::GL ${GLEW_LIBRARIES}
    ${ILMBASE_LIBRARIES}
)
if (TARGET displaz_com)
    target_link_libraries(displaz_com
        Qt5::Core Qt5::Network
    )
endif()
if (Qt5_POSITION_INDEPENDENT_CODE)
    set_target_properties(displaz PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    if (TARGET displaz_com)
        set_target_properties(displaz_com PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    endif()
endif()
# TODO this is a workaround for Ubuntu 16.04 x86-64
if (UNIX)
    add_definitions(-fPIC)
endif()

if(APPLE)
    target_link_libraries(displaz
        ${FOUNDATION_LIBRARY} ${COCOA_LIBRARY}
    )
endif()

if (DISPLAZ_USE_LAS)
    if (DISPLAZ_USE_PDAL)
        target_link_libraries(displaz ${PDAL_LIBRARIES})
    else()
        target_link_libraries(displaz ${LASLIB_LIBRARIES})
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

install(TARGETS displaz DESTINATION "${DISPLAZ_BIN_DIR}")

if (TARGET displaz_com)
    install(TARGETS displaz_com DESTINATION "${DISPLAZ_BIN_DIR}")
endif()

if(WIN32)
    include(DeployQt5)
    install_qt5_executable(${DISPLAZ_BIN_DIR}/displaz${CMAKE_EXECUTABLE_SUFFIX})
endif()

#------------------------------------------------------------------------------
# Create a shader target to copy shaders at build-time
add_custom_target(copy_shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders ${CMAKE_BINARY_DIR}/${DISPLAZ_SHADER_DIR}
) 
add_dependencies(displaz copy_shaders)

#------------------------------------------------------------------------------
# Experimental dvox utility
if (DISPLAZ_BUILD_DVOX)
    add_executable(dvox
        ${util_srcs}
        dvox.cpp
        pointdbwriter.cpp
        pointdb.cpp
        voxelizer.cpp
    )
    target_link_libraries(dvox Qt5::Core ${LASLIB_LIBRARIES})
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

    if (Qt5_POSITION_INDEPENDENT_CODE)
        set_target_properties(unit_tests PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    endif()

    # Interprocess tests require special purpose executables
    add_executable(InterProcessLock_test InterProcessLock_test.cpp util.cpp InterProcessLock.cpp)
    target_link_libraries(InterProcessLock_test Qt5::Core)
    target_link_libraries(unit_tests Qt5::Core)
    add_test(NAME InterProcessLock_test COMMAND InterProcessLock_test master)
endif()
