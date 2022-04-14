cmake_minimum_required(VERSION 3.15)
project(Ex2)

set(CMAKE_CXX_STANDARD 11)

add_library(Ex2 uthreads.cpp thread.cpp)
add_executable(ex2_exe main.cpp)
target_link_libraries(ex2_exe Ex2)
