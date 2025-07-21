#pragma once

#include <string>
#include <vector>
#include <random>
#include <algorithm>

namespace microservice {
namespace utils {

// Generate a vector of 8 padding fields, each 1000 bytes of a different letter from 'A' to 'H'
inline std::vector<std::string> generate_padding_fields() {
    std::vector<std::string> fields;
    for (char c = 'A'; c <= 'H'; ++c) {
        fields.emplace_back(100, c);
    }
    return fields;
}

// For backward compatibility, generate_padding returns the first field (1000 'A's)
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