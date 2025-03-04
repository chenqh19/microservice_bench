cmake_minimum_required(VERSION 3.10)
project(service2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)

# Add cpp-httplib
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
)
FetchContent_MakeAvailable(httplib)

add_executable(service2 
    main.cpp 
    ${CMAKE_CURRENT_BINARY_DIR}/service.pb.cc
)

target_link_libraries(service2
    ${Protobuf_LIBRARIES}
    httplib::httplib
)

target_include_directories(service2 PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/../protos
) 