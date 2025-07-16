#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include "padding_utils.h"
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../thread_pool.h"

class ReservationService {
private:
    struct HotelReservations {
        std::vector<hotelreservation::Reservation> reservations;
        int total_rooms;
    };

    std::unordered_map<std::string, HotelReservations> hotel_reservations_;
    std::mutex reservations_mutex_;

    std::string sendProtobufOverUDS(const std::string& path, const std::string& data) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return "";
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(fd);
            return "";
        }
        uint32_t len = data.size();
        if (write(fd, &len, 4) != 4) { close(fd); return ""; }
        if (write(fd, data.data(), len) != (ssize_t)len) { close(fd); return ""; }
        char len_buf[4];
        if (read(fd, len_buf, 4) != 4) { close(fd); return ""; }
        uint32_t resp_len = 0;
        memcpy(&resp_len, len_buf, 4);
        std::vector<char> resp_buf(resp_len);
        if (read(fd, resp_buf.data(), resp_len) != (ssize_t)resp_len) { close(fd); return ""; }
        close(fd);
        return std::string(resp_buf.begin(), resp_buf.end());
    }

public:
    ReservationService() {
        InitializeSampleData();
    }

    void InitializeSampleData() {
        // Initialize hotels with 100 rooms each
        for (int i = 1; i <= 80; i++) {
            std::string hotel_id = std::to_string(i);
            hotel_reservations_[hotel_id] = {{}, 100};
        }
    }

    bool checkAvailability(const std::string& hotel_id, const std::string& in_date, 
                          const std::string& out_date, int room_number) {
        auto it = hotel_reservations_.find(hotel_id);
        if (it == hotel_reservations_.end()) {
            return false;
        }

        // Count existing reservations for the date range
        int reserved_rooms = 0;
        for (const auto& reservation : it->second.reservations) {
            if (reservation.in_date() <= out_date && reservation.out_date() >= in_date) {
                reserved_rooms++;
            }
        }

        return reserved_rooms < it->second.total_rooms;
    }

    hotelreservation::ReservationResponse MakeReservation(
        const hotelreservation::ReservationRequest& req) {
        
        hotelreservation::ReservationResponse response;

        // First, verify user credentials using UDS
        hotelreservation::CheckUserRequest user_req;
        user_req.set_username(req.username());
        user_req.set_password(req.password());
        user_req.set_padding(microservice::utils::generate_padding());
        std::string user_resp_str = sendProtobufOverUDS("/tmp/user_service.sock", microservice::utils::serialize_message(user_req));
        hotelreservation::CheckUserResponse user_resp;
        if (!microservice::utils::deserialize_message(user_resp_str, user_resp) || !user_resp.exists()) {
            response.set_message("Invalid user credentials");
            response.set_padding(microservice::utils::generate_padding());
            return response;
        }
        std::lock_guard<std::mutex> lock(reservations_mutex_);
        // Check if hotel exists and has availability
        if (!checkAvailability(req.hotel_id(), req.in_date(), req.out_date(), req.room_number())) {
            response.set_message("No availability for the selected dates");
            response.set_padding(microservice::utils::generate_padding());
            return response;
        }
        // Create reservation
        hotelreservation::Reservation reservation;
        reservation.set_hotel_id(req.hotel_id());
        reservation.set_customer_name(req.customer_name());
        reservation.set_in_date(req.in_date());
        reservation.set_out_date(req.out_date());
        reservation.set_number(req.room_number());
        reservation.set_padding(microservice::utils::generate_padding());
        hotel_reservations_[req.hotel_id()].reservations.push_back(reservation);
        response.set_message("Reservation confirmed");
        response.set_padding(microservice::utils::generate_padding());
        return response;
    }
};

void handle_client(int client_fd, ReservationService& service) {
    char len_buf[4];
    ssize_t n = read(client_fd, len_buf, 4);
    if (n != 4) { close(client_fd); return; }
    uint32_t msg_len = 0;
    memcpy(&msg_len, len_buf, 4);
    std::vector<char> buf(msg_len);
    n = read(client_fd, buf.data(), msg_len);
    if (n != (ssize_t)msg_len) { close(client_fd); return; }
    hotelreservation::ReservationRequest request;
    bool ok = microservice::utils::deserialize_message(std::string(buf.begin(), buf.end()), request);
    if (ok) {
        auto response = service.MakeReservation(request);
        std::string resp_str = microservice::utils::serialize_message(response);
        uint32_t resp_len = resp_str.size();
        write(client_fd, &resp_len, 4);
        write(client_fd, resp_str.data(), resp_len);
    }
    close(client_fd);
}

int main() {
    const char* socket_path = "/tmp/reservation_service.sock";
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
    std::cout << "Reservation service listening on unix://" << socket_path << std::endl;
    
    ReservationService service;
    ThreadPool pool(64); // Use 64 threads for the pool
    
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) continue;
        pool.enqueue_task([client_fd, &service]() {
            handle_client(client_fd, service);
        });
    }
    close(server_fd);
    return 0;
} 