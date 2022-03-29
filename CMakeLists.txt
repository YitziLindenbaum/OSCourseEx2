cmake_minimum_required(VERSION 3.20)
project(Ex2)

set(CMAKE_CXX_STANDARD 11)

add_library(Ex2 library.cpp uthreads.h demo_itimer.c demo_jmp.c demo_singInt_handler.c)
