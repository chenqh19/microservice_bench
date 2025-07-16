#pragma once

#include <string>
#include "hotel_reservation.pb.h"

#define USE_SER1DE 1

#if USE_SER1DE
#include <ser1de/ser1de_re.h>

namespace microservice {
namespace utils {

// Serialize protobuf message to string using ser1de
template<typename T>
std::string serialize_message(Ser1de_re& ser1de, const T& message) {
    //std::cout << "Using SERenaDE for serialization" << std::endl;
    std::string serialized;
    // Cast to base Message type for ser1de
    T& non_const_message = const_cast<T&>(message);
    ser1de.SerializeToString(non_const_message, &serialized);
    //std::cout << "Used SERenaDE for serialization successfully!" << std::endl;
    return serialized;
}

// Deserialize string to protobuf message using ser1de
template<typename T>
bool deserialize_message(Ser1de_re& ser1de, const std::string& data, T& message) {
    //std::cout << "Using SERenaDE for deserialization" << std::endl;
    try {
        ser1de.ParseFromString(data, &message);
        //std::cout << "Used SERenaDE for deserialization successfully!" << std::endl;
        return true;
    } catch (const std::exception& e) {
        //std::cout << "Failed to deserialize message using SERenaDE" << std::endl;
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
std::string serialize_message(Ser1de_re& ser1de, const T& message) {
    std::string serialized;
    message.SerializeToString(&serialized);
    return serialized;
}

// Deserialize string to protobuf message using standard protobuf
template<typename T>
bool deserialize_message(Ser1de_re& ser1de, const std::string& data, T& message) {
    return message.ParseFromString(data);
}

} // namespace utils
} // namespace microservice

#endif // USE_SER1DE 