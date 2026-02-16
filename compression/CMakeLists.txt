# Docker-oriented build for compress_frontend_service (see docker-compose.yml).
# Context: repo root. Copy compression/, scheduler/, utils/, config.h into image.
cmake_minimum_required(VERSION 3.16)
project(compress_frontend_service)

add_definitions(-std=c++14 -O3 -march=native)
add_definitions(-Wall -Wextra -Wformat -Wformat-security)
add_definitions(-fstack-protector -fPIE -fPIC)
add_definitions(-D_FORTIFY_SOURCE=2)
add_definitions(-g)

find_package(Threads REQUIRED)
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
)
FetchContent_MakeAvailable(httplib)

link_directories(/app/ser1de/qpl_install_dir/lib)
include_directories(/app/ser1de/qpl_install_dir/include/)

add_executable(compress_frontend_service main.cpp)

# Local: scheduler at repo root ../scheduler. Docker: COPY scheduler ./scheduler so scheduler is alongside compression contents.
if(EXISTS ${CMAKE_SOURCE_DIR}/scheduler)
  set(SCHEDULER_DIR ${CMAKE_SOURCE_DIR}/scheduler)
else()
  set(SCHEDULER_DIR ${CMAKE_SOURCE_DIR}/../scheduler)
endif()
target_include_directories(compress_frontend_service PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/..
    ${SCHEDULER_DIR}
)

target_link_libraries(compress_frontend_service
    httplib::httplib
    Threads::Threads
    qpl
)
