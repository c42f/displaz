# Bare bones build system for LASlib (works at least with version 131105)
project(laslib)
cmake_minimum_required(VERSION 2.8)

if (NOT WIN32)
    set(CMAKE_CXX_FLAGS "-Wno-unused-result -fno-strict-aliasing")
else()
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

add_definitions(-DUNORDERED)

file (GLOB LASlib_srcs LASlib/src/*.cpp)
file (GLOB LASzip_srcs_glob LASzip/src/*.cpp)
foreach (src ${LASzip_srcs_glob})
    if (NOT ${src} MATCHES ".*(laszipper|lasunzipper|laszip_dll).*")
        list(APPEND LASzip_srcs ${src})
    endif()
endforeach()

include_directories(LASlib/inc LASzip/src LASlib/src)

add_library(laslib STATIC
    ${LASlib_srcs}
    ${LASzip_srcs}
)

install(TARGETS laslib DESTINATION lib)
install(DIRECTORY LASlib/inc/ DESTINATION include)
# Install all laszip headers - even internal ones - to make this build system
# less sensitive to changes in laszip.
install(DIRECTORY LASzip/src/ DESTINATION include PATTERN *.hpp)
