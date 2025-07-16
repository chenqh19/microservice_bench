#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include "padding_utils.h"
#include <httplib.h>
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <iomanip>
#include <sstream>

// Helper function to get current timestamp as string
std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// Helper function to calculate duration in microseconds
template<typename T>
uint64_t getDurationUs(const T& start, const T& end) {
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

class UserService {
private:
    static const int POOL_SIZE = 1024;
    static const int MAX_CONCURRENT_CONNECTIONS = 512;
    std::mutex connection_mutex_;
    std::condition_variable connection_cv_;

    std::unordered_map<std::string, std::string> users_; // username -> password
    std::mutex users_mutex_;

public:
    UserService() {
        InitializeSampleData();
    }

    void InitializeSampleData() {
        // Initialize users from Cornell_0 to Cornell_500
        for (int i = 0; i <= 500; i++) {
            std::string suffix = std::to_string(i);
            std::string username = "Cornell_" + suffix;
            
            // Create password by repeating the suffix 10 times
            std::string password;
            for (int j = 0; j < 10; j++) {
                password += suffix;
            }

            users_[username] = password;
        }
    }

    hotelreservation::UserResponse RegisterUser(const hotelreservation::UserRequest& req) {
        auto app_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] UserService::RegisterUser - Application logic started" << std::endl;
        
        hotelreservation::UserResponse response;
        std::lock_guard<std::mutex> lock(users_mutex_);

        if (users_.find(req.username()) != users_.end()) {
            response.set_message("User already exists");
            response.set_padding(microservice::utils::generate_padding());
            
            auto app_end = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] UserService::RegisterUser - Application logic completed in " 
                      << getDurationUs(app_start, app_end) << "μs (user exists)" << std::endl;
            return response;
        }

        users_[req.username()] = req.password();
        response.set_message("User registered successfully");
        response.set_padding(microservice::utils::generate_padding());
        
        auto app_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] UserService::RegisterUser - Application logic completed in " 
                  << getDurationUs(app_start, app_end) << "μs (user registered)" << std::endl;
        return response;
    }

    bool CheckUser(const hotelreservation::CheckUserRequest& req) {
        auto app_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] UserService::CheckUser - Application logic started" << std::endl;
        
        std::lock_guard<std::mutex> lock(users_mutex_);
        auto it = users_.find(req.username());
        bool result = (it != users_.end() && it->second == req.password());
        
        auto app_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] UserService::CheckUser - Application logic completed in " 
                  << getDurationUs(app_start, app_end) << "μs (result: " << (result ? "true" : "false") << ")" << std::endl;
        return result;
    }
};

int main() {
    httplib::Server svr;
    UserService service;

    // Set up multi-threading options
    svr.new_task_queue = [] { return new httplib::ThreadPool(1024); }; // Match pool size with client pool

    svr.Post("/user", [&](const httplib::Request& req, httplib::Response& res) {
        auto http_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /user endpoint - HTTP request received" << std::endl;

        auto start_time = std::chrono::steady_clock::now();

        auto check_timeout = [&start_time]() -> bool {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            return elapsed > 10; // 10ms timeout
        };

        // Protobuf deserialization timing
        auto pb_deserialize_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /user endpoint - Starting protobuf deserialization" << std::endl;
        
        hotelreservation::UserRequest request;
        if (!microservice::utils::deserialize_message(req.body, request)) {
            auto pb_deserialize_end = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] /user endpoint - Protobuf deserialization failed in " 
                      << getDurationUs(pb_deserialize_start, pb_deserialize_end) << "μs" << std::endl;
            
            res.status = 400;
            res.set_content("{\"error\": \"Failed to deserialize request\"}", "application/json");
            return;
        }
        
        auto pb_deserialize_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /user endpoint - Protobuf deserialization completed in " 
                  << getDurationUs(pb_deserialize_start, pb_deserialize_end) << "μs" << std::endl;

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during deserialization\"}", "application/json");
            return;
        }

        // Application logic timing
        auto app_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /user endpoint - Starting application logic" << std::endl;
        
        auto response = service.RegisterUser(request);
        
        auto app_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /user endpoint - Application logic completed in " 
                  << getDurationUs(app_start, app_end) << "μs" << std::endl;

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during processing\"}", "application/json");
            return;
        }

        // Protobuf serialization timing
        auto pb_serialize_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /user endpoint - Starting protobuf serialization" << std::endl;
        
        std::string serialized_response = microservice::utils::serialize_message(response);
        
        auto pb_serialize_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /user endpoint - Protobuf serialization completed in " 
                  << getDurationUs(pb_serialize_start, pb_serialize_end) << "μs" << std::endl;

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during serialization\"}", "application/json");
            return;
        }

        res.set_content(serialized_response, "application/x-protobuf");
        
        auto http_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /user endpoint - HTTP response sent in " 
                  << getDurationUs(http_start, http_end) << "μs total" << std::endl;
    });

    svr.Post("/check-user", [&](const httplib::Request& req, httplib::Response& res) {
        auto http_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /check-user endpoint - HTTP request received" << std::endl;

        auto start_time = std::chrono::steady_clock::now();

        auto check_timeout = [&start_time]() -> bool {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            return elapsed > 10; // 10ms timeout
        };

        // Protobuf deserialization timing
        auto pb_deserialize_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /check-user endpoint - Starting protobuf deserialization" << std::endl;
        
        hotelreservation::CheckUserRequest request;
        if (!microservice::utils::deserialize_message(req.body, request)) {
            auto pb_deserialize_end = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] /check-user endpoint - Protobuf deserialization failed in " 
                      << getDurationUs(pb_deserialize_start, pb_deserialize_end) << "μs" << std::endl;
            
            res.status = 400;
            res.set_content("{\"error\": \"Failed to deserialize request\"}", "application/json");
            return;
        }
        
        auto pb_deserialize_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /check-user endpoint - Protobuf deserialization completed in " 
                  << getDurationUs(pb_deserialize_start, pb_deserialize_end) << "μs" << std::endl;

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during deserialization\"}", "application/json");
            return;
        }

        // Application logic timing
        auto app_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /check-user endpoint - Starting application logic" << std::endl;
        
        hotelreservation::CheckUserResponse response;
        response.set_exists(service.CheckUser(request));
        response.set_padding(microservice::utils::generate_padding());
        
        auto app_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /check-user endpoint - Application logic completed in " 
                  << getDurationUs(app_start, app_end) << "μs" << std::endl;

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during processing\"}", "application/json");
            return;
        }

        // Protobuf serialization timing
        auto pb_serialize_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /check-user endpoint - Starting protobuf serialization" << std::endl;
        
        std::string serialized_response = microservice::utils::serialize_message(response);
        
        auto pb_serialize_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /check-user endpoint - Protobuf serialization completed in " 
                  << getDurationUs(pb_serialize_start, pb_serialize_end) << "μs" << std::endl;

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during serialization\"}", "application/json");
            return;
        }

        res.set_content(serialized_response, "application/x-protobuf");
        
        auto http_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /check-user endpoint - HTTP response sent in " 
                  << getDurationUs(http_start, http_end) << "μs total" << std::endl;
    });

    std::cout << "User service listening on 0.0.0.0:50054 with 1024 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50054);

    return 0;
}