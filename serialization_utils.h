#pragma once

#include <string>
#include "hotel_reservation.pb.h"

#define USE_SER1DE 0

#ifdef USE_SER1DE
#include <ser1de/ser1de.h>

namespace microservice {
namespace utils {

// Static global ser1de instance
static Ser1de& get_ser1de_instance() {
    static Ser1de instance; // Default constructor uses hardware path
    return instance;
}

// Serialize protobuf message to string using ser1de
template<typename T>
std::string serialize_message(const T& message) {
    std::string serialized;
    // Cast to base Message type for ser1de
    T& non_const_message = const_cast<T&>(message);
    get_ser1de_instance().SerializeToString(non_const_message, &serialized);
    return serialized;
}

// Deserialize string to protobuf message using ser1de
template<typename T>
bool deserialize_message(const std::string& data, T& message) {
    try {
        get_ser1de_instance().ParseFromString(data, &message);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

} // namespace utils
} // namespace microservice

#else

namespace microservice {
namespace utils {

// Serialize protobuf message to string using standard protobuf
template<typename T>
std::string serialize_message(const T& message) {
    std::string serialized;
    message.SerializeToString(&serialized);
    return serialized;
}

// Deserialize string to protobuf message using standard protobuf
template<typename T>
bool deserialize_message(const std::string& data, T& message) {
    return message.ParseFromString(data);
}

} // namespace utils
} // namespace microservice

#endif // USE_SER1DE 