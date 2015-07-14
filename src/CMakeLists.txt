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
set(shared_srcs
    logger.cpp
    geomfield.cpp
    typespec.cpp
    hcloud.cpp
    util.cpp
    ../thirdparty/argparse.cpp
)
qt4_wrap_cpp(moc_srcs
    datasetui.h
    dragspinbox.h
    fileloader.h
    geometry.h
    geometrycollection.h
    hcloudview.h
    helpdialog.h
    IpcChannel.h
    interactivecamera.h
    qtlogger.h
    mainwindow.h
    view3d.h
    pointarray.h
    shadereditor.h
    shader.h
)
set(srcs
    ${shared_srcs}
    ${moc_srcs}
    datasetui.cpp
    DrawCostModel.cpp
    geometry.cpp
    geometrycollection.cpp
    glutil.cpp
    hcloudview.cpp
    InterProcessLock.cpp
    IpcChannel.cpp
    interactivecamera.cpp
    qtlogger.cpp
    main.cpp
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

qt4_add_resources(RCC_GENERATED resource.qrc)

if(WIN32)
    list(APPEND srcs ../package/windows_resources.rc)
endif()

if(WIN32)
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS)
endif()

# Create config.h
set(DISPLAZ_VERSION_STRING "${displazVersion}")
configure_file(config.h.in.cmake config.h @ONLY)
include_directories(${PROJECT_BINARY_DIR})


set(allTargets displaz)
add_executable(displaz WIN32 ${srcs} ${RCC_GENERATED})

if (DISPLAZ_BUILD_DVOX)
    list(APPEND allTargets dvox)
    add_executable(dvox
        dvox.cpp
        pointdbwriter.cpp
        pointdb.cpp
        voxelizer.cpp
        ${shared_srcs}
    )
endif()

#-------------------------------------------------------------------------------
# Library dependencies
target_link_libraries(displaz
    ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTOPENGL_LIBRARY}
    ${QT_QTNETWORK_LIBRARY} ${OPENGL_gl_LIBRARY} ${GLEW_LIBRARIES}
    ${ILMBASE_LIBRARIES}
)
if (DISPLAZ_BUILD_DVOX)
    # TODO: Remove this dependency
    target_link_libraries(dvox ${QT_QTCORE_LIBRARY})
endif()

if (DISPLAZ_USE_LAS)
    if (DISPLAZ_USE_PDAL)
        target_link_libraries (displaz ${PDAL_LIBRARIES})
        if (DISPLAZ_BUILD_DVOX)
            message(SEND_ERROR "experimental dvox utility doesn't support pdal as a backend")
        endif()
    else()
        target_link_libraries (displaz ${LASLIB_LIBRARIES})
        if (DISPLAZ_BUILD_DVOX)
            target_link_libraries (dvox ${LASLIB_LIBRARIES})
        endif()
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
if (DISPLAZ_BUILD_DVOX)
    install(TARGETS dvox DESTINATION "${DISPLAZ_BIN_DIR}")
endif()

if(WIN32)
    # Install required qt libraries
    install(CODE "
        include(BundleUtilities)
        fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${DISPLAZ_BIN_DIR}/displaz${CMAKE_EXECUTABLE_SUFFIX}\" \"\" \"${QT_LIBRARY_DIR}\")
    ")
endif()


#------------------------------------------------------------------------------
# Tests
if (DISPLAZ_USE_TESTS)
    add_executable(util_test util_test.cpp)
    add_executable(streampagecache_test streampagecache_test.cpp)
    add_executable(InterProcessLock_test InterProcessLock_test.cpp util.cpp InterProcessLock.cpp)
    target_link_libraries(InterProcessLock_test ${QT_QTCORE_LIBRARY})
    add_test(util_test util_test)
    add_test(streampagecache_test streampagecache_test)
    add_test(InterProcessLock_test InterProcessLock_test master)
    list(APPEND allTargets util_test streampagecache_test)
endif()

