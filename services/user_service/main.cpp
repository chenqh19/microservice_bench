#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include "hotel_reservation.pb.h"
#include "../utils/serialization_utils.h"
#include "../utils/padding_utils.h"
#include "../utils/compression_utils.h"
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
#include "../utils/prefork_utils.h"

class UserService {
private:
    std::unordered_map<std::string, std::string> users_; // username -> password
    std::mutex users_mutex_;
    std::string pre_generated_random_data_;

public:
    UserService() {
        // Pre-generate random data once during initialization
        pre_generated_random_data_.resize(20000);
        for (int i = 0; i < 20000; i++) {
            pre_generated_random_data_[i] = 'A' + (i % 26);
        }
        InitializeSampleData();
        // Initialize compression manager
        microservice::compression::init_compression();
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

    hotelreservation::UserResponse process_request(const hotelreservation::UserRequest& req) {
        std::lock_guard<std::mutex> lock(users_mutex_);

        // Useless compression/decompression of random 5000B string
        std::string compressed_random = microservice::compression::compress_data(pre_generated_random_data_);
        std::string decompressed_random = microservice::compression::decompress_data(compressed_random);

        if (users_.find(req.username()) != users_.end()) {
            hotelreservation::UserResponse response;
            response.set_message("User already exists");
            *response.mutable_padding() = microservice::utils::generate_person_padding();
            
            return response;
        }

        users_[req.username()] = req.password();
        hotelreservation::UserResponse response;
        response.set_message("User registered successfully");
        *response.mutable_padding() = microservice::utils::generate_person_padding();
        
        return response;
    }

    hotelreservation::CheckUserResponse process_check_request(const hotelreservation::CheckUserRequest& req) {
        std::lock_guard<std::mutex> lock(users_mutex_);

        // Useless compression/decompression of random 5000B string
        std::string compressed_random = microservice::compression::compress_data(pre_generated_random_data_);
        std::string decompressed_random = microservice::compression::decompress_data(compressed_random);

        auto it = users_.find(req.username());
        hotelreservation::CheckUserResponse response;
        
        if (it != users_.end() && it->second == req.password()) {
            response.set_exists("True");
        } else {
            response.set_exists("False");
        }
        
        *response.mutable_padding() = microservice::utils::generate_person_padding();
        
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/user_service.sock";
    const int NUM_WORKERS = 32;  // Number of worker processes
    
    PreforkServer server(NUM_WORKERS);
    
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }
    
    std::cout << "User service socket setup complete" << std::endl;
    
    // Fork worker processes
    if (server.fork_workers()) {
        // This is a worker process
        UserService service;
        Ser1de_re ser1de;
        
        // Worker process main loop - handle both UserRequest and CheckUserRequest
        std::cout << "Worker " << getpid() << " ready to accept connections" << std::endl;
        
        while (true) {
            int client_fd = accept(server.get_server_fd(), nullptr, nullptr);
            if (client_fd < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("accept");
                break;
            }

            // Handle the client
            char len_buf[4];
            ssize_t n = read(client_fd, len_buf, 4);
            if (n != 4) { 
                close(client_fd); 
                continue; 
            }
            
            uint32_t msg_len = 0;
            memcpy(&msg_len, len_buf, 4);
            std::vector<char> buf(msg_len);
            n = read(client_fd, buf.data(), msg_len);
            if (n != (ssize_t)msg_len) { 
                close(client_fd); 
                continue; 
            }
            
            // Try to deserialize as UserRequest first
            hotelreservation::UserRequest user_req;
            bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), user_req);
            if (ok) {
                auto response = service.process_request(user_req);
                std::string resp_str = microservice::utils::serialize_message(ser1de, response);
                uint32_t resp_len = resp_str.size();
                write(client_fd, &resp_len, 4);
                write(client_fd, resp_str.data(), resp_len);
            } else {
                // Try as CheckUserRequest
                hotelreservation::CheckUserRequest check_req;
                ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), check_req);
                if (ok) {
                    auto response = service.process_check_request(check_req);
                    std::string resp_str = microservice::utils::serialize_message(ser1de, response);
                    uint32_t resp_len = resp_str.size();
                    write(client_fd, &resp_len, 4);
                    write(client_fd, resp_str.data(), resp_len);
                }
            }
            close(client_fd);
        }
        
        return 0;
    } else {
        // This is the master process
        std::cout << "User service master process started with " << NUM_WORKERS << " workers" << std::endl;
        server.master_loop();
        return 0;
    }
}