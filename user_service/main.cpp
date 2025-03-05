#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>

class UserService {
private:
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
        hotelreservation::UserResponse response;
        std::lock_guard<std::mutex> lock(users_mutex_);

        if (users_.find(req.username()) != users_.end()) {
            response.set_message("User already exists");
            return response;
        }

        users_[req.username()] = req.password();
        response.set_message("User registered successfully");
        return response;
    }

    bool CheckUser(const hotelreservation::CheckUserRequest& req) {
        std::lock_guard<std::mutex> lock(users_mutex_);
        auto it = users_.find(req.username());
        if (it == users_.end()) return false;
        return it->second == req.password();
    }
};

int main() {
    httplib::Server svr;
    UserService service;

    // Set up multi-threading options
    svr.new_task_queue = [] { return new httplib::ThreadPool(100); }; // Create thread pool with 8 threads

    svr.Post("/user", [&](const httplib::Request& req, httplib::Response& res) {
        hotelreservation::UserRequest request;
        if (microservice::utils::deserialize_message(req.body, request)) {
            auto response = service.RegisterUser(request);
            std::string serialized_response = microservice::utils::serialize_message(response);
            res.set_content(serialized_response, "application/x-protobuf");
        } else {
            res.status = 400;
        }
    });

    svr.Post("/check-user", [&](const httplib::Request& req, httplib::Response& res) {
        hotelreservation::CheckUserRequest request;
        if (microservice::utils::deserialize_message(req.body, request)) {
            hotelreservation::CheckUserResponse response;
            response.set_exists(service.CheckUser(request));
            std::string serialized_response = microservice::utils::serialize_message(response);
            res.set_content(serialized_response, "application/x-protobuf");
        } else {
            res.status = 400;
        }
    });

    std::cout << "User service listening on 0.0.0.0:50054 with 100 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50054);

    return 0;
}