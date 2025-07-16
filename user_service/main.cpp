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
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

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
    const char* socket_path = "/tmp/user_service.sock";
    unlink(socket_path); // Remove if exists
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    chmod(socket_path, 0777); // Ensure world-writable for Docker
    if (listen(server_fd, 1024) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }
    std::cout << "User service listening on unix://" << socket_path << std::endl;
    UserService service;
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) continue;
        std::thread([client_fd, &service]() {
            auto conn_start = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] Connection accepted" << std::endl;
            char len_buf[4];
            ssize_t n = read(client_fd, len_buf, 4);
            if (n != 4) { close(client_fd); return; }
            uint32_t msg_len = 0;
            memcpy(&msg_len, len_buf, 4);
            std::vector<char> buf(msg_len);
            n = read(client_fd, buf.data(), msg_len);
            if (n != (ssize_t)msg_len) { close(client_fd); return; }
            auto req_recv = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] Request received (" << msg_len << " bytes) in " << getDurationUs(conn_start, req_recv) << "μs" << std::endl;
            // Try UserRequest
            hotelreservation::UserRequest user_req;
            auto deser_start = std::chrono::steady_clock::now();
            bool ok = microservice::utils::deserialize_message(std::string(buf.begin(), buf.end()), user_req);
            auto deser_end = std::chrono::steady_clock::now();
            if (ok) {
                std::cout << "[" << getTimestamp() << "] UserRequest deserialized in " << getDurationUs(deser_start, deser_end) << "μs" << std::endl;
                auto app_start = std::chrono::steady_clock::now();
                auto response = service.RegisterUser(user_req);
                auto app_end = std::chrono::steady_clock::now();
                std::cout << "[" << getTimestamp() << "] Application logic completed in " << getDurationUs(app_start, app_end) << "μs" << std::endl;
                auto ser_start = std::chrono::steady_clock::now();
                std::string resp_str = microservice::utils::serialize_message(response);
                uint32_t resp_len = resp_str.size();
                write(client_fd, &resp_len, 4);
                write(client_fd, resp_str.data(), resp_len);
                auto ser_end = std::chrono::steady_clock::now();
                std::cout << "[" << getTimestamp() << "] Response serialized and sent in " << getDurationUs(ser_start, ser_end) << "μs" << std::endl;
                close(client_fd);
                auto conn_end = std::chrono::steady_clock::now();
                std::cout << "[" << getTimestamp() << "] Connection closed in " << getDurationUs(conn_start, conn_end) << "μs total" << std::endl;
                return;
            }
            // Try CheckUserRequest
            hotelreservation::CheckUserRequest check_req;
            deser_start = std::chrono::steady_clock::now();
            ok = microservice::utils::deserialize_message(std::string(buf.begin(), buf.end()), check_req);
            deser_end = std::chrono::steady_clock::now();
            if (ok) {
                std::cout << "[" << getTimestamp() << "] CheckUserRequest deserialized in " << getDurationUs(deser_start, deser_end) << "μs" << std::endl;
                auto app_start = std::chrono::steady_clock::now();
                hotelreservation::CheckUserResponse response;
                response.set_exists(service.CheckUser(check_req));
                response.set_padding(microservice::utils::generate_padding());
                auto app_end = std::chrono::steady_clock::now();
                std::cout << "[" << getTimestamp() << "] Application logic completed in " << getDurationUs(app_start, app_end) << "μs" << std::endl;
                auto ser_start = std::chrono::steady_clock::now();
                std::string resp_str = microservice::utils::serialize_message(response);
                uint32_t resp_len = resp_str.size();
                write(client_fd, &resp_len, 4);
                write(client_fd, resp_str.data(), resp_len);
                auto ser_end = std::chrono::steady_clock::now();
                std::cout << "[" << getTimestamp() << "] Response serialized and sent in " << getDurationUs(ser_start, ser_end) << "μs" << std::endl;
                close(client_fd);
                auto conn_end = std::chrono::steady_clock::now();
                std::cout << "[" << getTimestamp() << "] Connection closed in " << getDurationUs(conn_start, conn_end) << "μs total" << std::endl;
                return;
            }
            // Unknown request
            close(client_fd);
            auto conn_end = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] Connection closed (unknown request) in " << getDurationUs(conn_start, conn_end) << "μs total" << std::endl;
        }).detach();
    }
    close(server_fd);
    return 0;
}