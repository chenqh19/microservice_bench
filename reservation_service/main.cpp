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
#include "../prefork_utils.h"

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
    Ser1de_re ser1de;
    
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

    hotelreservation::ReservationResponse process_request(const hotelreservation::ReservationRequest& req) {
        
        hotelreservation::ReservationResponse response;

        // First, verify user credentials using UDS
        hotelreservation::CheckUserRequest user_req;
        user_req.set_username(req.username());
        user_req.set_password(req.password());
        auto pads_user = microservice::utils::generate_padding_fields();
        user_req.set_padding1(pads_user[0]);
        user_req.set_padding2(pads_user[1]);
        user_req.set_padding3(pads_user[2]);
        user_req.set_padding4(pads_user[3]);
        user_req.set_padding5(pads_user[4]);
        user_req.set_padding6(pads_user[5]);
        user_req.set_padding7(pads_user[6]);
        user_req.set_padding8(pads_user[7]);
        std::string user_resp_str = sendProtobufOverUDS("/tmp/user_service.sock", microservice::utils::serialize_message(ser1de, user_req));
        hotelreservation::CheckUserResponse user_resp;
        if (!microservice::utils::deserialize_message(ser1de, user_resp_str, user_resp) || !user_resp.exists()) {
            response.set_message("Invalid user credentials");
            auto pads_response = microservice::utils::generate_padding_fields();
            response.set_padding1(pads_response[0]);
            response.set_padding2(pads_response[1]);
            response.set_padding3(pads_response[2]);
            response.set_padding4(pads_response[3]);
            response.set_padding5(pads_response[4]);
            response.set_padding6(pads_response[5]);
            response.set_padding7(pads_response[6]);
            response.set_padding8(pads_response[7]);
            return response;
        }
        std::lock_guard<std::mutex> lock(reservations_mutex_);
        // Check if hotel exists and has availability
        auto it = hotel_reservations_.find(req.hotel_id());
        if (it == hotel_reservations_.end()) {
            response.set_message("Hotel not found");
            auto pads_response = microservice::utils::generate_padding_fields();
            response.set_padding1(pads_response[0]);
            response.set_padding2(pads_response[1]);
            response.set_padding3(pads_response[2]);
            response.set_padding4(pads_response[3]);
            response.set_padding5(pads_response[4]);
            response.set_padding6(pads_response[5]);
            response.set_padding7(pads_response[6]);
            response.set_padding8(pads_response[7]);
            return response;
        }

        if (!checkAvailability(req.hotel_id(), req.in_date(), req.out_date(), req.room_number())) {
            response.set_message("No availability for the requested dates");
            auto pads_response = microservice::utils::generate_padding_fields();
            response.set_padding1(pads_response[0]);
            response.set_padding2(pads_response[1]);
            response.set_padding3(pads_response[2]);
            response.set_padding4(pads_response[3]);
            response.set_padding5(pads_response[4]);
            response.set_padding6(pads_response[5]);
            response.set_padding7(pads_response[6]);
            response.set_padding8(pads_response[7]);
            return response;
        }

        // Create reservation
        hotelreservation::Reservation reservation;
        reservation.set_customer_name(req.customer_name());
        reservation.set_hotel_id(req.hotel_id());
        reservation.set_in_date(req.in_date());
        reservation.set_out_date(req.out_date());
        reservation.set_number(req.room_number());
        auto pads = microservice::utils::generate_padding_fields();
        reservation.set_padding1(pads[0]);
        reservation.set_padding2(pads[1]);
        reservation.set_padding3(pads[2]);
        reservation.set_padding4(pads[3]);
        reservation.set_padding5(pads[4]);
        reservation.set_padding6(pads[5]);
        reservation.set_padding7(pads[6]);
        reservation.set_padding8(pads[7]);

        it->second.reservations.push_back(reservation);

        response.set_message("Reservation successful");
        auto pads_response = microservice::utils::generate_padding_fields();
        response.set_padding1(pads_response[0]);
        response.set_padding2(pads_response[1]);
        response.set_padding3(pads_response[2]);
        response.set_padding4(pads_response[3]);
        response.set_padding5(pads_response[4]);
        response.set_padding6(pads_response[5]);
        response.set_padding7(pads_response[6]);
        response.set_padding8(pads_response[7]);
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/reservation_service.sock";
    const int NUM_WORKERS = 8;  // Number of worker processes
    
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