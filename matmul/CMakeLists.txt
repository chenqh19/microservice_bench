# Standalone matmul: one binary, one request. No HTTP, no qpl/scheduler.
# Build: cd matmul/build && cmake .. && make run_matmul && ./run_matmul [M K N]
cmake_minimum_required(VERSION 3.16)
project(run_matmul)

add_definitions(-std=c++14 -O3 -march=native)
add_definitions(-Wall -Wextra)

find_package(Threads REQUIRED)

add_executable(run_matmul run_matmul.cpp amx_matmul.cpp)

target_include_directories(run_matmul PRIVATE ${CMAKE_SOURCE_DIR})

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
  set_source_files_properties(amx_matmul.cpp PROPERTIES COMPILE_FLAGS "-mamx-tile -mamx-int8 -march=native")
endif()

target_link_libraries(run_matmul PRIVATE Threads::Threads)
