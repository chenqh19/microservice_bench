cmake_minimum_required(VERSION 3.10)
project(service2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)

# Use pkg-config to find gRPC
include(FindPkgConfig)
pkg_check_modules(GRPC REQUIRED grpc++)
pkg_check_modules(GRPCPP REQUIRED grpc++_unsecure)

add_executable(service2 
    main.cpp 
    ${CMAKE_CURRENT_BINARY_DIR}/service.grpc.pb.cc 
    ${CMAKE_CURRENT_BINARY_DIR}/service.pb.cc
)

target_link_libraries(service2
    ${GRPCPP_LIBRARIES}
    ${Protobuf_LIBRARIES}
)

target_include_directories(service2 PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${GRPCPP_INCLUDE_DIRS}
    ${Protobuf_INCLUDE_DIRS}
) 