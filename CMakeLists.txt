cmake_minimum_required(VERSION 3.5.1)

project(trampball)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	# using Visual Studio C++
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Wa")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_RELEASE} /O2 /fp:fast")
	set(MATHS_LIBRARY "")
else()
	# Probably GCC-like...
	# Use standard and proper C99
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -pedantic -Wall -Wextra ")
	# Use some UNIX features.
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=700")
	
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -ffast-math")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
	set(MATHS_LIBRARY "m")
endif()

set(src_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

add_executable(game ${src_dir}/main.c
                    ${src_dir}/ball.c
                    ${src_dir}/trampoline.c
                    ${src_dir}/interaction.c)
target_link_libraries(game ${SDL2_LIBRARY} ${MATHS_LIBRARY})
