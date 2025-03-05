#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>

class ReservationService {
private:
    struct HotelReservations {
        std::vector<hotelreservation::Reservation> reservations;
        int total_rooms;  // Changed from fixed 100 to variable
    };

    std::unordered_map<std::string, HotelReservations> hotel_reservations_;
    std::mutex reservations_mutex_;
    httplib::Client user_client_;

public:
    ReservationService() : user_client_("user", 50054) {
        InitializeSampleData();
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
        
        hotelreservation::ReservationResponse response;

        // First, verify user credentials
        hotelreservation::CheckUserRequest user_req;
        user_req.set_username(req.username());
        user_req.set_password(req.password());

        auto user_result = user_client_.Post("/check-user", 
            microservice::utils::serialize_message(user_req), 
            "application/x-protobuf");

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
        return response;
    }
};

int main() {
    httplib::Server svr;
    ReservationService service;

    // Set up multi-threading options
    svr.new_task_queue = [] { return new httplib::ThreadPool(100); }; // Create thread pool with 8 threads

    svr.Post("/reservation", [&](const httplib::Request& req, httplib::Response& res) {
        hotelreservation::ReservationRequest request;
        if (microservice::utils::deserialize_message(req.body, request)) {
            auto response = service.MakeReservation(request);
            std::string serialized_response = microservice::utils::serialize_message(response);
            res.set_content(serialized_response, "application/x-protobuf");
        } else {
            res.status = 400;
        }
    });

    std::cout << "Reservation service listening on 0.0.0.0:50055 with 100 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50055);

    return 0;
} 