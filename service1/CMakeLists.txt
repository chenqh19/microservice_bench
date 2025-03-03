cmake_minimum_required(VERSION 3.13)
project(service1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages from local builds
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/../protobuf/cmake/build")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/../grpc/build")

find_package(Protobuf REQUIRED CONFIG)
find_package(gRPC REQUIRED CONFIG)
find_package(absl REQUIRED CONFIG)

# Generate protobuf and gRPC code
set(PROTO_PATH "${CMAKE_SOURCE_DIR}/../protos/service.proto")
set(GENERATED_PROTO_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

add_custom_command(
    OUTPUT
        "${GENERATED_PROTO_DIR}/service.pb.cc"
        "${GENERATED_PROTO_DIR}/service.pb.h"
        "${GENERATED_PROTO_DIR}/service.grpc.pb.cc"
        "${GENERATED_PROTO_DIR}/service.grpc.pb.h"
    COMMAND protobuf::protoc
    ARGS
        --grpc_out="${GENERATED_PROTO_DIR}"
        --cpp_out="${GENERATED_PROTO_DIR}"
        --plugin=protoc-gen-grpc="$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
        -I "${CMAKE_SOURCE_DIR}/../protos"
        "${PROTO_PATH}"
    DEPENDS "${PROTO_PATH}"
)

# Create a library for the generated code
add_library(proto_lib
    ${GENERATED_PROTO_DIR}/service.pb.cc
    ${GENERATED_PROTO_DIR}/service.grpc.pb.cc
)

target_link_libraries(proto_lib
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
        absl::synchronization
        absl::time
        absl::strings
        absl::status
)

target_include_directories(proto_lib
    PUBLIC
        ${GENERATED_PROTO_DIR}
)

# Create the main executable
add_executable(service1 
    main.cpp
)

target_link_libraries(service1
    PRIVATE
        proto_lib
        gRPC::grpc++
        protobuf::libprotobuf
        absl::synchronization
        absl::time
        absl::strings
        absl::status
) 