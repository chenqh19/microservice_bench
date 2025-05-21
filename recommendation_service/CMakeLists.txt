cmake_minimum_required(VERSION 3.10)
project(recommendation_service)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)

include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
)
FetchContent_MakeAvailable(httplib)

# Generate protobuf files
execute_process(
    COMMAND protoc -I${CMAKE_SOURCE_DIR}/../protos --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/../protos/hotel_reservation.proto
    RESULT_VARIABLE PROTOC_RESULT
)
if(NOT PROTOC_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate protobuf files")
endif()

add_executable(recommendation_service 
    main.cpp 
    ${CMAKE_CURRENT_BINARY_DIR}/hotel_reservation.pb.cc
)

target_link_libraries(recommendation_service
    ${Protobuf_LIBRARIES}
    httplib::httplib
    ${JSONCPP_LIBRARIES}
    Threads::Threads
)

target_include_directories(recommendation_service PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/../protos
) 