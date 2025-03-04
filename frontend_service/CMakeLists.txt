cmake_minimum_required(VERSION 3.10)
project(frontend_service)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)

include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
)
FetchContent_MakeAvailable(httplib)

add_executable(frontend_service 
    main.cpp 
    ${CMAKE_CURRENT_BINARY_DIR}/hotel_reservation.pb.cc
)

target_link_libraries(frontend_service
    ${Protobuf_LIBRARIES}
    httplib::httplib
    ${JSONCPP_LIBRARIES}
)

target_include_directories(frontend_service PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/../protos
) 