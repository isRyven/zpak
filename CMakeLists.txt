cmake_minimum_required(VERSION 3.12)
project(zpak LANGUAGES C)
option(ZPAK_BUILD_ARCHIVER "Build zpak archiver executable" OFF)

add_library(zpak-header INTERFACE)
target_include_directories(zpak-header INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(zpak STATIC zpak.c zpak.h)
target_link_libraries(zpak lzs zpak-header)
add_subdirectory(lzs)

if (ZPAK_BUILD_ARCHIVER)
	add_executable(zpak-exe main.c)
	set_target_properties(zpak-exe PROPERTIES OUTPUT_NAME "zpak")
	target_link_libraries(zpak-exe zpak)
endif()

if(PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	enable_testing()
	add_subdirectory(tests)
endif()
