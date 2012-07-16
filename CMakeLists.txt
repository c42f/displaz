project(qtlasview)

cmake_minimum_required(VERSION 2.8)

find_package(OpenGL)
find_package(Qt4 4.6.2 COMPONENTS QtCore QtGui QtOpenGL)

if(NOT QT_FOUND OR NOT OPENGL_FOUND)
	message(SEND_ERROR "Qt or opengl not found")
endif()

include_directories(BEFORE
	/home/chris/work/roames/ROAMES_svn/Components/ThirdParty/ilmbase-1.0.2/src/include
	/home/chris/work/roames/ROAMES_svn/Components/ThirdParty/laslib-2.1.0/src/include
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
	argparse.cpp
)

link_directories(
	/home/chris/work/roames/ROAMES_svn/Components/ThirdParty/laslib-2.1.0/src/build
)
add_executable(qtlasview ${srcs})

set(CMAKE_CXX_FLAGS "-Wall -std=c++0x")
set(CMAKE_BUILD_TYPE "Release" CACHE STRING
	"Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
)


target_link_libraries(qtlasview
	${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTOPENGL_LIBRARY}
	${OPENGL_gl_LIBRARY}
	laslib
)

