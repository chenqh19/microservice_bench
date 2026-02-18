# matmul: matmul_workload (thread pool + fixed RPS, exp inter-arrival). Optional: run_matmul (one-shot).
# Build: cd matmul/build && cmake .. && make
# Workload: ./matmul_workload [rps] [duration_sec] [num_workers] [M K N]
cmake_minimum_required(VERSION 3.16)
project(matmul)

add_definitions(-std=c++14 -O3 -march=native)
add_definitions(-Wall -Wextra)

find_package(Threads REQUIRED)

add_executable(matmul_workload matmul_workload.cpp amx_matmul.cpp)
target_include_directories(matmul_workload PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(matmul_workload PRIVATE Threads::Threads)

if(EXISTS ${CMAKE_SOURCE_DIR}/run_matmul.cpp)
  add_executable(run_matmul run_matmul.cpp amx_matmul.cpp)
  target_include_directories(run_matmul PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(run_matmul PRIVATE Threads::Threads)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
  set_source_files_properties(amx_matmul.cpp PROPERTIES COMPILE_FLAGS "-mamx-tile -mamx-int8 -march=native")
endif()
