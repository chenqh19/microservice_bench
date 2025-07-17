#include <iostream>
#include <string>
#include <unordered_map>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include "padding_utils.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <cstring>
#include "../thread_pool.h"

class RateService {
private:
    std::unordered_map<std::string, std::vector<hotelreservation::RoomType>> hotel_rates_;

public:
    RateService() {
        InitializeSampleRates();
    }

    void InitializeSampleRates() {
        // Add sample room types and rates for hotels
        for (int i = 1; i <= 10; i++) {
            std::string hotel_id = std::to_string(i);
            std::vector<hotelreservation::RoomType> room_types;

            // Standard Room
            hotelreservation::RoomType standard;
            standard.set_bookable_rate(100.0 + (rand() % 50));
            standard.set_code("STD");
            standard.set_room_description("Standard Room");
            standard.set_total_rate(standard.bookable_rate() * 1.1);
            standard.set_total_rate_inclusive(standard.total_rate() * 1.2);
            standard.set_padding(microservice::utils::generate_padding());
            room_types.push_back(standard);

            // Deluxe Room
            hotelreservation::RoomType deluxe;
            deluxe.set_bookable_rate(200.0 + (rand() % 100));
            deluxe.set_code("DLX");
            deluxe.set_room_description("Deluxe Room");
            deluxe.set_total_rate(deluxe.bookable_rate() * 1.1);
            deluxe.set_total_rate_inclusive(deluxe.total_rate() * 1.2);
            deluxe.set_padding(microservice::utils::generate_padding());
            room_types.push_back(deluxe);

            hotel_rates_[hotel_id] = room_types;
        }
    }

    hotelreservation::GetRatesResponse GetRates(const hotelreservation::GetRatesRequest& req) {
        hotelreservation::GetRatesResponse response;
        
        for (const auto& hotel_id : req.hotel_ids()) {
            auto it = hotel_rates_.find(hotel_id);
            if (it != hotel_rates_.end()) {
                for (const auto& room_type : it->second) {
                    auto* rate_plan = response.add_rate_plans();
                    rate_plan->set_hotel_id(hotel_id);
                    rate_plan->set_code(room_type.code());
                    rate_plan->set_in_date(req.in_date());
                    rate_plan->set_out_date(req.out_date());
                    *rate_plan->mutable_room_type() = room_type;
                    rate_plan->set_padding(microservice::utils::generate_padding());
                }
            }
        }
        
        response.set_padding(microservice::utils::generate_padding());
        return response;
    }
};

void handle_client(int client_fd, RateService& service, Ser1de_re& ser1de) {
    char len_buf[4];
    ssize_t n = read(client_fd, len_buf, 4);
    if (n != 4) { close(client_fd); return; }
    uint32_t msg_len = 0;
    memcpy(&msg_len, len_buf, 4);
    std::vector<char> buf(msg_len);
    n = read(client_fd, buf.data(), msg_len);
    if (n != (ssize_t)msg_len) { close(client_fd); return; }
    hotelreservation::GetRatesRequest req;
    bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), req);
    if (!ok) { close(client_fd); return; }
    auto response = service.GetRates(req);
    std::string resp_str = microservice::utils::serialize_message(ser1de, response);
    uint32_t resp_len = resp_str.size();
    write(client_fd, &resp_len, 4);
    write(client_fd, resp_str.data(), resp_len);
    close(client_fd);
}

int main() {
    const char* socket_path = "/tmp/rate_service.sock";
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
    std::cout << "Rate service listening on unix://" << socket_path << std::endl;
    
    RateService service;
    Ser1de_re ser1de;
    ThreadPool pool(64); // Use 64 threads for the pool
    
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