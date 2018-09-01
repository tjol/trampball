cmake_minimum_required(VERSION 3.5.1)

project(trampball)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "cmake")

# Use standard and proper C99
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -pedantic -Wall -Wextra ")
# Use some UNIX features.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=700")


find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

add_executable(game src/main.c
                    src/ball.c
                    src/trampoline.c
                    src/interaction.c)
target_link_libraries(game ${SDL2_LIBRARY} m)
