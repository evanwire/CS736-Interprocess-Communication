cmake_minimum_required(VERSION 3.0)
project(CS736 LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS_RELEASE "-O3")

add_library(proj_common common.c measurement.c)

add_library(pipe pipe.c)
target_link_libraries(pipe proj_common)

add_library(socket socket.c)
target_link_libraries(socket proj_common)

add_library(shared_mem shared_mem.c)
target_link_libraries(shared_mem proj_common)

add_executable(run run.c)
target_link_libraries(run proj_common socket pipe shared_mem)

