#include "../utils/compression_utils.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "hotel_reservation.pb.h"
#include "../utils/serialization_utils.h"
#include "../utils/padding_utils.h"
#include "../utils/data_models.h"
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../utils/prefork_utils.h"

class ReservationService {
private:
    struct HotelReservations {
        std::vector<microservice::models::ReservationData> reservations;
        int total_rooms;
    };

    std::unordered_map<std::string, HotelReservations> hotel_reservations_;
    std::mutex reservations_mutex_;
    std::string pre_generated_random_data_;

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
    Ser1de_re ser1de;
    
    ReservationService() {
        // Pre-generate random data once during initialization
        pre_generated_random_data_.resize(2000);
        for (int i = 0; i < 2000; i++) {
            pre_generated_random_data_[i] = 'A' + (i % 26);
        }
        InitializeSampleData();
        // Initialize compression manager
        microservice::compression::init_compression();
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
            if (reservation.in_date <= out_date && reservation.out_date >= in_date) {
                reserved_rooms++;
            }
        }

        // Check if we have enough rooms available (including the requested room_number)
        return (reserved_rooms + room_number) <= it->second.total_rooms;
    }

    hotelreservation::ReservationResponse process_request(const hotelreservation::ReservationRequest& req) {
        
		// Optional dummy compression
#if ENABLE_DUMMY_SERVICE_COMPRESSION
        std::string compressed_random = microservice::compression::compress_data(pre_generated_random_data_);
        std::string decompressed_random = microservice::compression::decompress_data(compressed_random);
#endif

        hotelreservation::ReservationResponse response;

        // Convert incoming protobuf to domain model
        microservice::models::ReservationRequestData reqd;
        microservice::models::fromProto(req, reqd);

        // First, verify user credentials using UDS
        hotelreservation::CheckUserRequest user_req;
        user_req.set_username(reqd.username);
        user_req.set_password(reqd.password);
        *user_req.mutable_padding() = microservice::utils::generate_person_padding();
        std::string user_resp_str = sendProtobufOverUDS("/tmp/user_service.sock", microservice::utils::serialize_message(ser1de, user_req));
        hotelreservation::CheckUserResponse user_resp;
        if (!microservice::utils::deserialize_message(ser1de, user_resp_str, user_resp)) {
            response.set_message("Failed to deserialize user response");
            *response.mutable_padding() = microservice::utils::generate_person_padding();
            return response;
        }
        
        // Treat "User already exists" as valid user (True)
        if (user_resp.exists() != "True" && user_resp.exists() != "User already exists") {
            response.set_message("Invalid user credentials");
            *response.mutable_padding() = microservice::utils::generate_person_padding();
            return response;
        }

        // Check availability
        if (!checkAvailability(reqd.hotel_id, reqd.in_date, reqd.out_date, (int)reqd.room_number)) {
            response.set_message("No rooms available for the specified dates");
            *response.mutable_padding() = microservice::utils::generate_person_padding();
            return response;
        }

        // Create reservation
        std::lock_guard<std::mutex> lock(reservations_mutex_);
        
        microservice::models::ReservationData r;
        r.hotel_id = reqd.hotel_id;
        r.customer_name = reqd.customer_name;
        r.in_date = reqd.in_date;
        r.out_date = reqd.out_date;
        r.number = reqd.room_number;
        hotel_reservations_[req.hotel_id()].reservations.push_back(std::move(r));
        
        response.set_message("Reservation created successfully");
        *response.mutable_padding() = microservice::utils::generate_person_padding();
        
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/reservation_service.sock";
    const int NUM_WORKERS = 64;  // Number of worker processes
    
    PreforkServer server(NUM_WORKERS);
    
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }
    
    std::cout << "Reservation service socket setup complete" << std::endl;
    
    // Fork worker processes
    if (server.fork_workers()) {
        // This is a worker process
        ReservationService service;
        Ser1de_re ser1de;
        
        // Worker process main loop
        worker_loop<ReservationService, hotelreservation::ReservationRequest, hotelreservation::ReservationResponse>(
            server.get_server_fd(), service, ser1de);
        
        return 0;
    } else {
        // This is the master process
        std::cout << "Reservation service master process started with " << NUM_WORKERS << " workers" << std::endl;
        server.master_loop();
        return 0;
    }
} 