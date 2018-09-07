cmake_minimum_required(VERSION 2.8)

project(trampball)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (MSVC)
	# using Visual Studio C++
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Wa")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_RELEASE} /O2")
	set(MATHS_LIBRARY "")
else()
	# Probably GCC-like...
	# Use standard and proper C99
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -pedantic -Wall -Wextra ")
	# Use some UNIX features.
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=500")
	
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
	set(MATHS_LIBRARY "m")
endif()

set(src_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

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
endif()

add_executable(game ${src_dir}/main.c
                    ${src_dir}/font.c
                    ${src_dir}/game.c
                    ${src_dir}/ball.c
                    ${src_dir}/trampoline.c
                    ${src_dir}/interaction.c)
target_link_libraries(game ${SDL2_LIBRARY} ${MATHS_LIBRARY})

# Copy resource files to build directory
if (NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
	set(res_target_files "")
	file(GLOB_RECURSE res_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/res/*")
	foreach(res_file IN LISTS res_files)
	    add_custom_command(
	        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${res_file}"
	        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${res_file}" "${CMAKE_CURRENT_BINARY_DIR}/${res_file}"
	        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${res_file}"
	    )
	    set(res_target_files ${res_target_files} "${CMAKE_CURRENT_BINARY_DIR}/${res_file}")
	endforeach()

	add_custom_target(res-target ALL DEPENDS ${res_target_files})
endif()

