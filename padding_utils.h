#pragma once

#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include "hotel_reservation.pb.h"

namespace microservice {
namespace utils {

inline const hotelreservation::M& generate_person_padding() {
    static hotelreservation::M m;
    static bool initialized = false;
    if (!initialized) {
        // Set all fields to non-default values for real padding
        m.set_f1(1);
        m.set_f2(2);
        m.set_f3(3);
        m.set_f4(4);
        m.set_f5(5);
        m.set_f6(6);
        m.set_f7(7);
        m.set_f8(8);
        m.set_f9(9);
        m.set_f10(10);
        m.set_f11(11);
        m.set_f12(12);
        m.set_f13(std::string(200, 'A'));
        m.set_f14(std::string(200, 'B'));
        m.set_f15(std::string(200, 'C'));
        // Optionally, set nested messages if you want even more padding
        initialized = true;
    }
    return m;
}

// Set padding field for any protobuf message that has a padding field
template<typename T>
void set_padding(T& message) {
    // This will be specialized for each message type that has padding
    // The actual implementation will be in the service files
}

} // namespace utils
} // namespace microservice 