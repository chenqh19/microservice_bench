#pragma once

#include <string>
#include "hotel_reservation.pb.h"
#include <chrono>
#include <fstream>
#include <mutex>
#include <sys/stat.h>
#include <typeinfo>
#include "../config.h"

// Configuration options are now defined in config.h
// USE_SER1DE and ENABLE_TIMING are defined there

#if USE_SER1DE
#include <ser1de/ser1de_re.h>
#else
// Dummy struct so the interface is always the same
struct Ser1de_re {};
#endif

namespace microservice {
namespace utils {

namespace detail {
    static std::mutex log_mutex;
    static bool logs_dir_created = false;
    
#if ENABLE_TIMING
    static void ensure_logs_dir() {
        if (!logs_dir_created) {
            struct stat st = {};
            if (stat("/logs", &st) == -1) {
                mkdir("/logs", 0777);
            }
            logs_dir_created = true;
        }
    }
#endif
    template<typename T>
    std::string get_type_name() {
        return typeid(T).name();
    }
}

template<typename T>
std::string serialize_message(Ser1de_re& ser1de, const T& message) {
    using namespace std::chrono;
    
    std::string serialized;
    
#if ENABLE_TIMING
    auto start = high_resolution_clock::now();
#endif

#if USE_SER1DE
    T& non_const_message = const_cast<T&>(message);
    ser1de.SerializeToString(non_const_message, &serialized);
#else
    (void)ser1de; // Suppress unused parameter warning
    message.SerializeToString(&serialized);
#endif

#if ENABLE_TIMING
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<nanoseconds>(end - start).count();
    std::string type_name = detail::get_type_name<T>();
    std::string log_file = "/logs/" + type_name + "Se.txt";
    {
        std::lock_guard<std::mutex> lock(detail::log_mutex);
        detail::ensure_logs_dir();
        std::ofstream ofs(log_file, std::ios::app);
        ofs << duration << std::endl;
    }
#endif
    
    return serialized;
}

template<typename T>
bool deserialize_message(Ser1de_re& ser1de, const std::string& data, T& message) {
    using namespace std::chrono;
    
    bool result = false;
    
#if ENABLE_TIMING
    auto start = high_resolution_clock::now();
#endif

#if USE_SER1DE
    try {
        ser1de.ParseFromString(data, &message);
        result = true;
    } catch (const std::exception& e) {
        result = false;
    }
#else
    (void)ser1de; // Suppress unused parameter warning
    result = message.ParseFromString(data);
#endif

#if ENABLE_TIMING
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<nanoseconds>(end - start).count();
    std::string type_name = detail::get_type_name<T>();
    std::string log_file = "/logs/" + type_name + "De.txt";
    {
        std::lock_guard<std::mutex> lock(detail::log_mutex);
        detail::ensure_logs_dir();
        std::ofstream ofs(log_file, std::ios::app);
        ofs << duration << std::endl;
    }
#endif
    
    return result;
}

// Function to log request timing data
inline void log_request_timing(const std::string& endpoint, 
                              const std::chrono::steady_clock::time_point& start_time,
                              const std::chrono::steady_clock::time_point& end_time) {
#if ENABLE_TIMING
    using namespace std::chrono;
    auto duration = duration_cast<nanoseconds>(end_time - start_time).count();
    std::string log_file = "/logs/frontend_" + endpoint + "_request.txt";
    {
        std::lock_guard<std::mutex> lock(detail::log_mutex);
        detail::ensure_logs_dir();
        std::ofstream ofs(log_file, std::ios::app);
        ofs << duration << std::endl;
    }
#endif
}

} // namespace utils
} // namespace microservice 