cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

# Force multi-configuration generator (Visual Studio) into single-configuration
set(CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})

set(CMAKE_SUPPRESS_REGENERATION true)
project("LAStools")

if (!MSVC)
	add_compile_options(-O3 -Wall -Wno-strict-aliasing)
else()
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(LASlib/src)
#add_subdirectory(src)
if (EXISTS src_full)
	add_subdirectory(src_full)
endif()
