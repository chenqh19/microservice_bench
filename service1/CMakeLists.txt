cmake_minimum_required(VERSION 3.10)
project(service1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set custom Protobuf paths
set(Protobuf_ROOT /home/czarkos/dev/protobuf)
set(Protobuf_INCLUDE_DIR ${Protobuf_ROOT}/src)
set(Protobuf_LIBRARY ${Protobuf_ROOT}/libprotobuf.a)
set(Protobuf_PROTOC_EXECUTABLE ${Protobuf_ROOT}/protoc)

# Use the custom Protobuf
find_package(Protobuf REQUIRED PATHS ${Protobuf_ROOT} NO_DEFAULT_PATH)

# Use pkg-config to find gRPC
include(FindPkgConfig)
pkg_check_modules(GRPC REQUIRED grpc++)
pkg_check_modules(GRPCPP REQUIRED grpc++_unsecure)

add_executable(service1 
    main.cpp 
    ${CMAKE_CURRENT_BINARY_DIR}/service.grpc.pb.cc 
    ${CMAKE_CURRENT_BINARY_DIR}/service.pb.cc
)

target_link_libraries(service1
    ${GRPCPP_LIBRARIES}
    ${Protobuf_LIBRARIES}
)

target_include_directories(service1 PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${GRPCPP_INCLUDE_DIRS}
    ${Protobuf_INCLUDE_DIRS}
) 