cmake_minimum_required(VERSION 3.0)
project(CS736 LANGUAGES C)

add_library(measurement measurement.c)

add_library(pipe pipe.c)
target_link_libraries(pipe measurement)

add_library(socket socket.c)
target_link_libraries(socket measurement)

add_library(shared_mem shared_mem.c)
target_link_libraries(shared_mem measurement)

add_executable(run run.c)
target_link_libraries(run measurement socket pipe shared_mem)
