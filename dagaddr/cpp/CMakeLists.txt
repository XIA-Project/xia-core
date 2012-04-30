cmake_minimum_required (VERSION 2.6)

project(libdagaddr)

add_definitions(-std=c++0x -O2 -g)

add_library(dagaddr dagaddr.cpp)

add_executable(test test.cpp)
target_link_libraries(test dagaddr)

