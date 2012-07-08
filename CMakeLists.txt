project(qtlasview)

cmake_minimum_required(VERSION 2.8)

find_package(OpenGL)
find_package(Qt4 4.6.2 COMPONENTS QtCore QtGui QtOpenGL)
find_package(Boost 1.34.1 COMPONENTS program_options)

if(NOT QT_FOUND OR NOT OPENGL_FOUND)
	message(SEND_ERROR "Qt or opengl not found")
endif()

include_directories(BEFORE
	/home/chris/work/roames/ROAMES_svn/Components/ThirdParty/ilmbase-1.0.2/src/include
	/home/chris/work/roames/ROAMES_svn/Components/ThirdParty/laslib-2.1.0/src/include
	${Boost_INCLUDE_DIRS}
	${QT_INCLUDES}
	${OPENGL_INCLUDE_DIR}
)

qt4_wrap_cpp(moc_srcs
	ptview.h
	interactivecamera.h
)
set(srcs
	${moc_srcs}
	ptview.cpp
)

link_directories(
	/home/chris/work/roames/ROAMES_svn/Components/ThirdParty/laslib-2.1.0/src/build
)
add_executable(qtlasview ${srcs})

target_link_libraries(qtlasview
	${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTOPENGL_LIBRARY}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${OPENGL_gl_LIBRARY}
	laslib
)

