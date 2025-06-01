#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <condition_variable>

class ReservationService {
private:
    static const int POOL_SIZE = 1024;
    static const int MAX_CONCURRENT_CONNECTIONS = 512;
    std::atomic<size_t> active_connections_{0};
    
    struct ClientInfo {
        std::unique_ptr<httplib::Client> client;
        std::atomic<bool> in_use;
        std::chrono::steady_clock::time_point last_used;

        ClientInfo() : client(nullptr), in_use(false), last_used(std::chrono::steady_clock::now()) {}
        
        ClientInfo(std::unique_ptr<httplib::Client>&& c) 
            : client(std::move(c)), in_use(false), last_used(std::chrono::steady_clock::now()) {}
        
        ClientInfo(const ClientInfo&) = delete;
        ClientInfo& operator=(const ClientInfo&) = delete;
        
        ClientInfo(ClientInfo&& other) noexcept
            : client(std::move(other.client)), in_use(false), last_used(std::chrono::steady_clock::now()) {}
        
        ClientInfo& operator=(ClientInfo&& other) noexcept {
            client = std::move(other.client);
            bool expected = other.in_use.load();
            in_use.store(expected);
            last_used = std::chrono::steady_clock::now();
            return *this;
        }
    };

    struct HotelReservations {
        std::vector<hotelreservation::Reservation> reservations;
        int total_rooms;
    };

    std::unordered_map<std::string, HotelReservations> hotel_reservations_;
    std::mutex reservations_mutex_;
    std::vector<ClientInfo> user_clients_;
    std::atomic<size_t> current_user_idx_{0};
    std::mutex connection_mutex_;
    std::condition_variable connection_cv_;
    std::atomic<size_t> successful_reservations_{0};
    std::atomic<size_t> total_reservations_{0};

    httplib::Client* getNextAvailableClient(std::vector<ClientInfo>& clients, std::atomic<size_t>& current_idx) {
        size_t start_idx = current_idx.fetch_add(1) % POOL_SIZE;
        size_t current = start_idx;
        
        // First try to find an available client without waiting
        do {
            bool expected = false;
            if (clients[current].in_use.compare_exchange_strong(expected, true)) {
                clients[current].last_used = std::chrono::steady_clock::now();
                return clients[current].client.get();
            }
            current = (current + 1) % POOL_SIZE;
        } while (current != start_idx);
        
        // If no client is available, wait for one with a timeout
        std::unique_lock<std::mutex> lock(connection_mutex_);
        if (connection_cv_.wait_for(lock, std::chrono::milliseconds(50), 
            [this] { return active_connections_ < MAX_CONCURRENT_CONNECTIONS; })) {
            // Try one more round after waiting
            current = start_idx;
            do {
                bool expected = false;
                if (clients[current].in_use.compare_exchange_strong(expected, true)) {
                    clients[current].last_used = std::chrono::steady_clock::now();
                    active_connections_++;
                    return clients[current].client.get();
                }
                current = (current + 1) % POOL_SIZE;
            } while (current != start_idx);
        }
        
        return nullptr;
    }

    void releaseClient(std::vector<ClientInfo>& clients, httplib::Client* client) {
        for (auto& info : clients) {
            if (info.client.get() == client) {
                info.in_use.store(false);
                active_connections_--;
                connection_cv_.notify_one();
                break;
            }
        }
    }

    void monitorResources() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::cout << "Resource Usage - Active Connections: " << active_connections_ 
                      << ", Total Reservations: " << total_reservations_ 
                      << ", Successful Reservations: " << successful_reservations_ << std::endl;
        }
    }

public:
    ReservationService() {
        // Initialize user client pool
        for (int i = 0; i < POOL_SIZE; i++) {
            user_clients_.push_back(ClientInfo(std::make_unique<httplib::Client>("user", 50054)));
        }
        InitializeSampleData();

        // Start resource monitoring thread
        std::thread monitor_thread(&ReservationService::monitorResources, this);
        monitor_thread.detach();
    }

    void InitializeSampleData() {
        // Initialize first 6 hotels with 200 rooms each
        for (int i = 1; i <= 6; i++) {
            std::string hotel_id = std::to_string(i);
            hotel_reservations_[hotel_id] = HotelReservations{std::vector<hotelreservation::Reservation>(), 200};
        }

        // Initialize hotels 7-80 with varying room numbers
        for (int i = 7; i <= 80; i++) {
            std::string hotel_id = std::to_string(i);
            int room_num = 200;  // default
            if (i % 3 == 1) {
                room_num = 300;
            } else if (i % 3 == 2) {
                room_num = 250;
            }
            hotel_reservations_[hotel_id] = HotelReservations{std::vector<hotelreservation::Reservation>(), room_num};
        }

        // Add initial reservation for Alice in hotel 4
        hotelreservation::Reservation initial_reservation;
        initial_reservation.set_hotel_id("4");
        initial_reservation.set_customer_name("Alice");
        initial_reservation.set_in_date("2015-04-09");
        initial_reservation.set_out_date("2015-04-10");
        initial_reservation.set_number(1);
        hotel_reservations_["4"].reservations.push_back(initial_reservation);
    }

    bool checkAvailability(const std::string& hotel_id, const std::string& in_date,
                          const std::string& out_date, int64_t room_number) {
        auto it = hotel_reservations_.find(hotel_id);
        if (it == hotel_reservations_.end()) return false;

        int booked_rooms = 0;
        for (const auto& reservation : it->second.reservations) {
            if ((reservation.in_date() <= out_date) && 
                (reservation.out_date() >= in_date)) {
                booked_rooms += reservation.number();
            }
        }

        return (booked_rooms + room_number) <= it->second.total_rooms;
    }

    hotelreservation::ReservationResponse MakeReservation(
        const hotelreservation::ReservationRequest& req) {
        
        total_reservations_++;
        hotelreservation::ReservationResponse response;

        // First, verify user credentials using client pool
        hotelreservation::CheckUserRequest user_req;
        user_req.set_username(req.username());
        user_req.set_password(req.password());

        auto* user_client = getNextAvailableClient(user_clients_, current_user_idx_);
        if (!user_client) {
            response.set_message("No available clients to verify user credentials");
            return response;
        }

        auto user_result = user_client->Post("/check-user", 
            microservice::utils::serialize_message(user_req), 
            "application/x-protobuf");
        releaseClient(user_clients_, user_client);

        if (!user_result || user_result->status != 200) {
            response.set_message("Failed to verify user credentials");
            return response;
        }

        hotelreservation::CheckUserResponse user_resp;
        if (!microservice::utils::deserialize_message(user_result->body, user_resp) ||
            !user_resp.exists()) {
            response.set_message("Invalid user credentials");
            return response;
        }

        std::lock_guard<std::mutex> lock(reservations_mutex_);

        // Check if hotel exists and has availability
        if (!checkAvailability(req.hotel_id(), req.in_date(), req.out_date(), req.room_number())) {
            response.set_message("No availability for the selected dates");
            return response;
        }

        // Create reservation
        hotelreservation::Reservation reservation;
        reservation.set_hotel_id(req.hotel_id());
        reservation.set_customer_name(req.customer_name());
        reservation.set_in_date(req.in_date());
        reservation.set_out_date(req.out_date());
        reservation.set_number(req.room_number());

        hotel_reservations_[req.hotel_id()].reservations.push_back(reservation);
        
        response.set_message("Reservation confirmed");
        successful_reservations_++;
        return response;
    }
};

int main() {
    httplib::Server svr;
    ReservationService service;

    // Set up multi-threading options
    svr.new_task_queue = [] { return new httplib::ThreadPool(256); }; // Create thread pool with 8 threads

    svr.Post("/reservation", [&](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();

        auto check_timeout = [&start_time]() -> bool {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            return elapsed > 100; // 100ms timeout
        };

        hotelreservation::ReservationRequest request;
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

        auto response = service.MakeReservation(request);

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

    std::cout << "Reservation service listening on 0.0.0.0:50055 with 256 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50055);

    return 0;
} 