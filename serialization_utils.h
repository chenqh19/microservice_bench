#pragma once

#include <string>
#include "service.pb.h"

namespace microservice {
namespace utils {

// Serialize protobuf message to string
template<typename T>
std::string serialize_message(const T& message) {
    std::string serialized;
    message.SerializeToString(&serialized);
    return serialized;
}

// Deserialize string to protobuf message
template<typename T>
bool deserialize_message(const std::string& data, T& message) {
    return message.ParseFromString(data);
}

} // namespace utils
} // namespace microservice 