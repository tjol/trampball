cmake_minimum_required(VERSION 2.8)

project(trampball C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (MSVC)
	# using Visual Studio C++
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Wa")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_RELEASE} /O2")
	set(EXTRA_LIB)
else()
	# Probably GCC-like...
	# Use standard and proper C99
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -pedantic -Wall -Wextra ")
	# Use some UNIX features.
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=500")
	
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
	set(EXTRA_LIB "m")
endif()

set(src_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")

option(ENABLE_MOUSE "Enable mouse control" ON)
option(LIBRARY_BUILD "Build a library instead of an executable" OFF)

if(NOT DEFINED ASSET_ROOT)
    set(ASSET_ROOT "res/")
endif()

configure_file(${src_dir}/config.h.in config.h)

if(NOT SDL2_LIBRARY OR NOT SDL2_INCLUDE_DIR)
	if(LIBRARY_BUILD)
		set(SDL2_BUILDING_LIBRARY ON)
	endif()
	find_package(SDL2 REQUIRED)
endif()

include_directories(${SDL2_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(GUI_APP)

if(WIN32)
	foreach(sdl2_lib ${SDL2_LIBRARY})
		string(REGEX REPLACE "lib$" "dll" SDL2_dll_path ${sdl2_lib})
		if(SDL2_dll_path MATCHES "[/\\\\]SDL2\\.dll$" AND EXISTS ${SDL2_dll_path})
			# Must copy SDL2.dll to the target directory...
			message(STATUS "copying ${SDL2_dll_path} to ${CMAKE_CURRENT_BINARY_DIR}")
			file(COPY ${SDL2_dll_path} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
			set(sdl2_dll_copied_flag 1)
		endif()
	endforeach()
	if(NOT sdl2_dll_copied_flag)
		message(WARNING "could not find SDL2.dll!")
	endif()
	set(GUI_APP "WIN32")
endif()

set(trampball_SOURCES ${src_dir}/trampball.c
                      ${src_dir}/font.c
                      ${src_dir}/game.c
                      ${src_dir}/ball.c
                      ${src_dir}/trampoline.c
                      ${src_dir}/interaction.c)

if(LIBRARY_BUILD)
	add_library(trampball SHARED ${trampball_SOURCES})
else()
	add_executable(trampball ${GUI_APP} ${trampball_SOURCES})
endif()

target_link_libraries(trampball ${SDL2_LIBRARY} ${EXTRA_LIB})

# Copy resource files to build directory
if (NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    set(ASSET_SRC "${CMAKE_CURRENT_SOURCE_DIR}/res")
    set(ASSET_DEST "${CMAKE_CURRENT_BINARY_DIR}/res")
	set(res_target_files "")
	file(GLOB_RECURSE res_files RELATIVE "${ASSET_SRC}" "${ASSET_SRC}/*")
	foreach(res_file IN LISTS res_files)
	    add_custom_command(
	        OUTPUT "${ASSET_DEST}/${res_file}"
	        COMMAND ${CMAKE_COMMAND} -E copy "${ASSET_SRC}/${res_file}" "${ASSET_DEST}/${res_file}"
	        DEPENDS "${ASSET_SRC}/${res_file}"
	    )
	    set(res_target_files ${res_target_files} "${ASSET_DEST}/${res_file}")
	endforeach()

	add_custom_target(resources ALL DEPENDS ${res_target_files})
	add_dependencies(trampball	 resources)
endif()

