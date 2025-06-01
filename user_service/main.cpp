#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable>

class UserService {
private:
    static const int POOL_SIZE = 1024;
    static const int MAX_CONCURRENT_CONNECTIONS = 512;
    std::atomic<size_t> active_connections_{0};
    std::atomic<size_t> successful_requests_{0};
    std::atomic<size_t> total_requests_{0};
    std::mutex connection_mutex_;
    std::condition_variable connection_cv_;

    std::unordered_map<std::string, std::string> users_; // username -> password
    std::mutex users_mutex_;

    void monitorResources() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::cout << "Resource Usage - Active Connections: " << active_connections_ 
                      << ", Total Requests: " << total_requests_ 
                      << ", Successful Requests: " << successful_requests_ << std::endl;
        }
    }

public:
    UserService() {
        InitializeSampleData();

        // Start resource monitoring thread
        std::thread monitor_thread(&UserService::monitorResources, this);
        monitor_thread.detach();
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
        total_requests_++;
        hotelreservation::UserResponse response;
        std::lock_guard<std::mutex> lock(users_mutex_);

        if (users_.find(req.username()) != users_.end()) {
            response.set_message("User already exists");
            return response;
        }

        users_[req.username()] = req.password();
        response.set_message("User registered successfully");
        successful_requests_++;
        return response;
    }

    bool CheckUser(const hotelreservation::CheckUserRequest& req) {
        total_requests_++;
        std::lock_guard<std::mutex> lock(users_mutex_);
        auto it = users_.find(req.username());
        bool result = (it != users_.end() && it->second == req.password());
        if (result) {
            successful_requests_++;
        }
        return result;
    }
};

int main() {
    httplib::Server svr;
    UserService service;

    // Set up multi-threading options
    svr.new_task_queue = [] { return new httplib::ThreadPool(1024); }; // Match pool size with client pool

    svr.Post("/user", [&](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();

        auto check_timeout = [&start_time]() -> bool {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            return elapsed > 100; // 100ms timeout
        };

        hotelreservation::UserRequest request;
        if (!microservice::utils::deserialize_message(req.body, request)) {
            res.status = 400;
            res.set_content("{\"error\": \"Failed to deserialize request\"}", "application/json");
            return;
        }

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during deserialization\"}", "application/json");
            return;
        }

        auto response = service.RegisterUser(request);

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during processing\"}", "application/json");
            return;
        }

        std::string serialized_response = microservice::utils::serialize_message(response);

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during serialization\"}", "application/json");
            return;
        }

        res.set_content(serialized_response, "application/x-protobuf");
    });

    svr.Post("/check-user", [&](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();

        auto check_timeout = [&start_time]() -> bool {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            return elapsed > 100; // 100ms timeout
        };

        hotelreservation::CheckUserRequest request;
        if (!microservice::utils::deserialize_message(req.body, request)) {
            res.status = 400;
            res.set_content("{\"error\": \"Failed to deserialize request\"}", "application/json");
            return;
        }

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during deserialization\"}", "application/json");
            return;
        }

        hotelreservation::CheckUserResponse response;
        response.set_exists(service.CheckUser(request));

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during processing\"}", "application/json");
            return;
        }

        std::string serialized_response = microservice::utils::serialize_message(response);

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during serialization\"}", "application/json");
            return;
        }

        res.set_content(serialized_response, "application/x-protobuf");
    });

    std::cout << "User service listening on 0.0.0.0:50054 with 1024 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50054);

    return 0;
}