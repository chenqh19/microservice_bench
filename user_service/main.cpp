#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include "padding_utils.h"
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../thread_pool.h"

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
        std::lock_guard<std::mutex> lock(users_mutex_);

        if (users_.find(req.username()) != users_.end()) {
            hotelreservation::UserResponse response;
            response.set_message("User already exists");
            response.set_padding(microservice::utils::generate_padding());
            return response;
        }

        users_[req.username()] = req.password();
        hotelreservation::UserResponse response;
        response.set_message("User registered successfully");
        response.set_padding(microservice::utils::generate_padding());
        return response;
    }

    bool CheckUser(const hotelreservation::CheckUserRequest& req) {
        std::lock_guard<std::mutex> lock(users_mutex_);
        auto it = users_.find(req.username());
        return (it != users_.end() && it->second == req.password());
    }
};

void handle_client(int client_fd, UserService& service, Ser1de_re& ser1de) {
    char len_buf[4];
    ssize_t n = read(client_fd, len_buf, 4);
    if (n != 4) { close(client_fd); return; }
    uint32_t msg_len = 0;
    memcpy(&msg_len, len_buf, 4);
    std::vector<char> buf(msg_len);
    n = read(client_fd, buf.data(), msg_len);
    if (n != (ssize_t)msg_len) { close(client_fd); return; }
    // Try UserRequest
    hotelreservation::UserRequest user_req;
    bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), user_req);
    if (ok) {
        auto response = service.RegisterUser(user_req);
        std::string resp_str = microservice::utils::serialize_message(ser1de, response);
        uint32_t resp_len = resp_str.size();
        write(client_fd, &resp_len, 4);
        write(client_fd, resp_str.data(), resp_len);
        close(client_fd);
        return;
    }
    // Try CheckUserRequest
    hotelreservation::CheckUserRequest check_req;
    ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), check_req);
    if (ok) {
        hotelreservation::CheckUserResponse response;
        response.set_exists(service.CheckUser(check_req));
        response.set_padding(microservice::utils::generate_padding());
        std::string resp_str = microservice::utils::serialize_message(ser1de, response);
        uint32_t resp_len = resp_str.size();
        write(client_fd, &resp_len, 4);
        write(client_fd, resp_str.data(), resp_len);
        close(client_fd);
        return;
    }
    // Unknown request
    close(client_fd);
}

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
    ThreadPool pool(8); // Use 8 threads for the pool
    Ser1de_re ser1de;
    
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) continue;
        pool.enqueue_task([client_fd, &service, &ser1de]() {
            handle_client(client_fd, service, ser1de);
        });
    }
    close(server_fd);
    return 0;
}