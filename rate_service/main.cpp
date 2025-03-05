#include <iostream>
#include <string>
#include <unordered_map>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>

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
            room_types.push_back(standard);

            // Deluxe Room
            hotelreservation::RoomType deluxe;
            deluxe.set_bookable_rate(200.0 + (rand() % 100));
            deluxe.set_code("DLX");
            deluxe.set_room_description("Deluxe Room");
            deluxe.set_total_rate(deluxe.bookable_rate() * 1.1);
            deluxe.set_total_rate_inclusive(deluxe.total_rate() * 1.2);
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
                }
            }
        }

        return response;
    }
};

int main() {
    httplib::Server svr;
    RateService service;

    // Set up multi-threading options
    svr.new_task_queue = [] { return new httplib::ThreadPool(100); }; // Create thread pool with 8 threads

    svr.Post("/get_rates", [&](const httplib::Request& req, httplib::Response& res) {
        hotelreservation::GetRatesRequest request;
        if (microservice::utils::deserialize_message(req.body, request)) {
            auto response = service.GetRates(request);
            std::string serialized_response = microservice::utils::serialize_message(response);
            res.set_content(serialized_response, "application/x-protobuf");
        } else {
            res.status = 400;
        }
    });

    std::cout << "Rate service listening on 0.0.0.0:50057 with 100 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50057);

    return 0;
} 