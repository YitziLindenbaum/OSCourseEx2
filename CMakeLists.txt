cmake_minimum_required(VERSION 3.15)
project(Ex2)

set(CMAKE_CXX_STANDARD 11)

add_library(Ex2 uthreads.h demo_itimer.c demo_jmp.c demo_singInt_handler.c uthreads.cpp thread.cpp thread.h)
