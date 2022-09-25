cmake_minimum_required(VERSION 3.0)
project(CS736 LANGUAGES C)

add_library(include include.c)

add_library(pipe pipe.c)
target_link_libraries(pipe include)

add_library(socket socket.c)
target_link_libraries(socket include pipe)

add_library(shared_mem shared_mem.c)
target_link_libraries(shared_mem include pipe)

add_executable(run run.c)
target_link_libraries(run include pipe socket shared_mem)

