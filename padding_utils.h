#pragma once

#include <string>
#include <random>
#include <algorithm>

namespace microservice {
namespace utils {

// Generate a 1000-byte random string for padding
inline std::string generate_padding() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(32, 126); // Printable ASCII characters
    
    std::string padding;
    padding.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        padding += static_cast<char>(dis(gen));
    }
    
    return padding;
}

// Set padding field for any protobuf message that has a padding field
template<typename T>
void set_padding(T& message) {
    // This will be specialized for each message type that has padding
    // The actual implementation will be in the service files
}

} // namespace utils
} // namespace microservice 