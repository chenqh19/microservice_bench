cmake_minimum_required(VERSION 3.16)
project(user_service)

add_definitions(-std=c++14 -O0 -march=native)
add_definitions(-Wall -Wextra -Wformat -Wformat-security)
add_definitions(-fstack-protector -fPIE -fPIC)
add_definitions(-D_FORTIFY_SOURCE=2)
add_definitions(-g)

find_package(GTest CONFIG REQUIRED)

# Add abseil
add_subdirectory(/app/ser1de/protobuf/third_party/abseil-cpp abseil)

include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
)
FetchContent_MakeAvailable(httplib)

# protobuf
include_directories(/app/ser1de/protobuf/src/)
link_directories(/app/ser1de/protobuf/)
link_directories(/app/ser1de/protobuf/third_party/utf8_range)
link_directories(/app/ser1de/protobuf/third_party/abseil-cpp)

# ser1de
include_directories(/app/ser1de/)

# qpl
link_directories(/app/ser1de/qpl_install_dir/lib)
include_directories(/app/ser1de/qpl_install_dir/include/)

set(protobuf_ABSL_USED_TARGETS
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
    absl::log_initialize
    absl::log_severity
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

add_executable(user_service 
    main.cpp 
    ${CMAKE_CURRENT_BINARY_DIR}/hotel_reservation.pb.cc
)

target_include_directories(user_service PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/../protos
) 

target_link_libraries(user_service
    protobuf
    ${protobuf_ABSL_USED_TARGETS}
    httplib::httplib
    pthread
    utf8_validity
    qpl
)