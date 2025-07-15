#pragma once

#include <string>
#include <random>
#include <algorithm>

namespace microservice {
namespace utils {

// Generate a 1000-byte string for padding that is highly compressible
inline std::string generate_padding() {
    return std::string(1000, 'A');
}

// Set padding field for any protobuf message that has a padding field
template<typename T>
void set_padding(T& message) {
    // This will be specialized for each message type that has padding
    // The actual implementation will be in the service files
}

} // namespace utils
} // namespace microservice 