cmake_minimum_required(VERSION 3.10)
project(frontend_service)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Properly find jsoncpp
find_package(PkgConfig REQUIRED)

# Add abseil
add_subdirectory($ENV{PROTO_PATH}/third_party/abseil-cpp abseil)

# Add Protobuf
include_directories($ENV{PROTO_PATH}/src/)
link_directories($ENV{PROTO_PATH})
link_directories($ENV{PROTO_PATH}/third_party/utf8_range)
link_directories($ENV{PROTO_PATH}/third_party/abseil-cpp)

set(protobuf_ABSL_USED_TARGETS
    # Core Abseil logging targets
    absl::log
    absl::log_entry
    absl::log_initialize
    absl::log_severity
    absl::log_sink
    absl::log_sink_registry
    absl::log_streamer
    # Internal logging components
    absl::log_internal_message
    absl::log_internal_check_op
    absl::log_internal_conditions
    absl::log_internal_format
    absl::log_internal_globals
    absl::log_internal_log_sink_set
    absl::log_internal_nullguard
    absl::log_internal_proto
    # Other required Abseil components
    absl::absl_check
    absl::absl_log
    absl::algorithm
    absl::base
    absl::bind_front
    absl::bits
    absl::btree
    absl::cleanup
    absl::cord
    absl::core_headers
    absl::debugging
    absl::die_if_null
    absl::dynamic_annotations
    absl::flags
    absl::flat_hash_map
    absl::flat_hash_set
    absl::function_ref
    absl::hash
    absl::layout
    absl::memory
    absl::node_hash_map
    absl::node_hash_set
    absl::optional
    absl::span
    absl::status
    absl::statusor
    absl::strings
    absl::synchronization
    absl::time
    absl::type_traits
    absl::utility
    absl::variant
)

pkg_check_modules(JSONCPP REQUIRED jsoncpp)

include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
)
FetchContent_MakeAvailable(httplib)

add_executable(frontend_service 
    main.cpp 
    ${CMAKE_SOURCE_DIR}/../gen-cpp/hotel_reservation.pb.cc
)

target_link_libraries(frontend_service
    PRIVATE
    ${protobuf_ABSL_USED_TARGETS}  # Abseil must come before protobuf
    protobuf
    utf8_range  # Add utf8_range library
    httplib::httplib
    ${JSONCPP_LIBRARIES}
    Threads::Threads
)

target_include_directories(frontend_service PRIVATE 
    ${Protobuf_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/../protos
    ${CMAKE_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/../gen-cpp
)

# Add compile definitions for jsoncpp
target_compile_options(frontend_service PRIVATE ${JSONCPP_CFLAGS_OTHER}) 