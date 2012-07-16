project(qtlasview)

cmake_minimum_required(VERSION 2.8)

#-----------------------------------------------------------
# find GLEW Snippet Pinched from OIIO
set (GLEW_VERSION 1.5.1)
find_library (GLEW_LIBRARIES NAMES GLEW)
find_path (GLEW_INCLUDES
           NAMES glew.h
           PATH_SUFFIXES GL)
if (GLEW_INCLUDES AND GLEW_LIBRARIES)
    set (GLEW_FOUND TRUE)
    message (STATUS "GLEW includes = ${GLEW_INCLUDES}")
    message (STATUS "GLEW library = ${GLEW_LIBRARIES}")
else ()
    message (STATUS "GLEW not found")
endif ()
#-----------------------------------------------------------

find_package(OpenGL)
find_package(Qt4 4.6.2 COMPONENTS QtCore QtGui QtOpenGL)

if(NOT QT_FOUND OR NOT OPENGL_FOUND)
	message(SEND_ERROR "Qt or opengl not found")
endif()

include_directories(BEFORE
	D:/ROAMES_git/Components/ThirdParty/ilmbase-1.0.2/src/include
	D:/ROAMES_git/Components/ThirdParty/laslib-2.1.0/src/include
	${QT_INCLUDES}
	${OPENGL_INCLUDE_DIR}
	${GLEW_INCLUDES}
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
	D:/ROAMES_git/Components/ThirdParty/laslib-2.1.0/src/lib/Release
)
add_executable(qtlasview ${srcs})

#set(CMAKE_CXX_FLAGS "-Wall -std=c++0x")
set(CMAKE_BUILD_TYPE "Release" CACHE STRING
	"Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
)


target_link_libraries(qtlasview
	${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTOPENGL_LIBRARY}
	${OPENGL_gl_LIBRARY}
	${GLEW_LIBRARIES}
	laslib
)

