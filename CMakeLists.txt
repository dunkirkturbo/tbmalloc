cmake_minimum_required(VERSION 3.10)

project(mem_pool)

set(CMAKE_CXX_COMPILER "g++") 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O3")

include_directories(inc)

aux_source_directory(. src)
add_executable(main main.cpp ${src})
target_link_libraries(main pthread)