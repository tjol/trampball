cmake_minimum_required(VERSION 3.5.1)

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
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=700")
	
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
                    ${src_dir}/ball.c
                    ${src_dir}/trampoline.c
                    ${src_dir}/interaction.c)
target_link_libraries(game ${SDL2_LIBRARY} ${MATHS_LIBRARY})
