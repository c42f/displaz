project(displaz)

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

if(NOT QT_FOUND OR NOT OPENGL_FOUND OR NOT GLEW_FOUND)
	message(SEND_ERROR "Qt, opengl and glew are required.")
endif()

if(WIN32)
	include_directories(BEFORE
		D:/ROAMES_git/Components/ThirdParty/ilmbase-1.0.2/src/include
		D:/ROAMES_git/Components/ThirdParty/laslib-2.1.0/src/include
	)
else()
	include_directories(BEFORE
		/home/chris/work/roames/svn/Components/ThirdParty/ilmbase-1.0.2/src/include
		/home/chris/work/roames/svn/Components/ThirdParty/laslib-2.1.0/src/include
	)
endif()

include_directories(BEFORE
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
if(WIN32)
	list(APPEND srcs windows_resources.rc)
endif()

if(NOT WIN32)
	set(CMAKE_CXX_FLAGS "-Wall -std=c++0x")
endif()

set(CMAKE_BUILD_TYPE "Release" CACHE STRING
	"Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
)

if(WIN32)
	link_directories(
		D:/ROAMES_git/Components/ThirdParty/laslib-2.1.0/src/lib/Release
	)
else()
	link_directories(
		/home/chris/work/roames/svn/Components/ThirdParty/laslib-2.1.0/src/build
	)
endif()

add_executable(displaz WIN32 ${srcs})

target_link_libraries(displaz
	${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTOPENGL_LIBRARY}
	${OPENGL_gl_LIBRARY}
	${GLEW_LIBRARIES}
	laslib
)

if(WIN32)
	target_link_libraries(displaz
		${QT_QTMAIN_LIBRARY}
	)
endif()
