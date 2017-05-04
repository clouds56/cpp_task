cmake_minimum_required(VERSION 3.7)
project(cpp_task)

set(CMAKE_CXX_STANDARD 14)

set(SOURCE_FILES library.cpp library.h common.h)
add_library(cpp_task ${SOURCE_FILES})
add_executable(cpp_task_test ${SOURCE_FILES} test.cpp)