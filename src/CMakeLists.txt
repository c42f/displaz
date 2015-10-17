project(viewer)

include_directories(BEFORE
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

# Interprocess communication
if(DISPLAZ_USE_QT4)
    qt4_wrap_cpp(ipc_moc_srcs
        IpcChannel.h
    )
else()
    qt5_wrap_cpp(ipc_moc_srcs
        IpcChannel.h
    )
endif()

set(ipc_srcs
    ${ipc_moc_srcs}
    InterProcessLock.cpp
    IpcChannel.cpp
)

# GUI
set(moc_srcs
    datasetui.h
    dragspinbox.h
    fileloader.h
    geometry.h
    geometrycollection.h
    hcloudview.h
    helpdialog.h
    interactivecamera.h
    qtlogger.h
    mainwindow.h
    view3d.h
    pointarray.h
    shadereditor.h
    shader.h
)

if(DISPLAZ_USE_QT4)
    qt4_wrap_cpp(gui_moc_srcs
        ${moc_srcs}
    )
else()
    qt5_wrap_cpp(gui_moc_srcs
        ${moc_srcs}
    )
endif()

set(gui_srcs
    ${util_srcs}
    ${ipc_srcs}
    ${gui_moc_srcs}
    datasetui.cpp
    DrawCostModel.cpp
    geometry.cpp
    geometrycollection.cpp
    glutil.cpp
    hcloudview.cpp
    qtlogger.cpp
    guimain.cpp
    mainwindow.cpp
    mesh.cpp
    ply_io.cpp
    las_io.cpp
    pointarray.cpp
    PolygonBuilder.cpp
    view3d.cpp
    shadereditor.cpp
    shader.cpp
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

if(APPLE)
    set(objc_srcs
        coreprofileattr.mm
    )
    find_library(COCOA_LIBRARY Cocoa)
    find_library(FOUNDATION_LIBRARY Foundation)
    # add_definitions("-x objective-c++")
    # set(CMAKE_EXE_LINKER_FLAGS "-framework ServiceManagement -framework Foundation -framework Cocoa")
endif()

# Create config.h
set(DISPLAZ_VERSION_STRING "${displazVersion}")
configure_file(config.h.in.cmake config.h @ONLY)
include_directories(${PROJECT_BINARY_DIR})


add_executable(displaz ${cli_srcs})
add_executable(displaz-gui WIN32 ${gui_srcs} ${objc_srcs} ${RCC_GENERATED})

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
        ${Qt5Core_LIBRARIES} ${Qt5Network_LIBRARIES} ${Qt5Widgets_LIBRARIES}
        ${ILMBASE_LIBRARIES}
    )
    target_link_libraries(displaz-gui
        ${Qt5Core_LIBRARIES} ${Qt5Gui_LIBRARIES} ${Qt5OpenGL_LIBRARIES}
        ${Qt5Network_LIBRARIES} ${Qt5Widgets_LIBRARIES}
        ${OPENGL_gl_LIBRARY} ${GLEW_LIBRARIES}
        ${ILMBASE_LIBRARIES}
    )
    if (Qt5_POSITION_INDEPENDENT_CODE)
        set_target_properties(displaz-gui PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
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
        target_link_libraries(dvox ${Qt5Core_LIBRARIES})
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
    # TODO: test windows version - is this still required ?
    if(DISPLAZ_USE_QT4)
        target_link_libraries(displaz-gui
            ${QT_QTMAIN_LIBRARY}
        )
    endif()
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
if (DISPLAZ_BUILD_DVOX)
    install(TARGETS dvox DESTINATION "${DISPLAZ_BIN_DIR}")
endif()

if(WIN32)
    # Install required qt libraries
    if (DISPLAZ_USE_QT4)
        install(CODE "
            include(BundleUtilities)
            fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${DISPLAZ_BIN_DIR}/displaz${CMAKE_EXECUTABLE_SUFFIX}\" \"\" \"${QT_LIBRARY_DIR}\")
        ")
    else()
        install(CODE "
            include(BundleUtilities)
            fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${DISPLAZ_BIN_DIR}/displaz${CMAKE_EXECUTABLE_SUFFIX}\" \"\" \"${Qt5Core_LIBRARY_DIR}\")
        ")
    endif()
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

    # Interprocess tests require special purpose executables
    add_executable(InterProcessLock_test InterProcessLock_test.cpp util.cpp InterProcessLock.cpp)
    if (DISPLAZ_USE_QT4)
        target_link_libraries(InterProcessLock_test ${QT_QTCORE_LIBRARY})
    else ()
        target_link_libraries(InterProcessLock_test ${Qt5Core_LIBRARIES})
    endif ()
    add_test(NAME InterProcessLock_test COMMAND InterProcessLock_test master)
endif()

