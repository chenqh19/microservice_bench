#include "../utils/compression_utils.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include "hotel_reservation.pb.h"
#include "../utils/serialization_utils.h"
#include "../utils/padding_utils.h"
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
#include "../utils/prefork_utils.h"

class RateService {
private:
    std::unordered_map<std::string, std::vector<hotelreservation::RoomType>> hotel_rates_;

public:
    RateService() {
        InitializeSampleRates();
        // Initialize compression manager
        microservice::compression::init_compression();
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
            *standard.mutable_padding() = microservice::utils::generate_person_padding();
            room_types.push_back(standard);

            // Deluxe Room
            hotelreservation::RoomType deluxe;
            deluxe.set_bookable_rate(200.0 + (rand() % 100));
            deluxe.set_code("DLX");
            deluxe.set_room_description("Deluxe Room");
            deluxe.set_total_rate(deluxe.bookable_rate() * 1.1);
            deluxe.set_total_rate_inclusive(deluxe.total_rate() * 1.2);
            *deluxe.mutable_padding() = microservice::utils::generate_person_padding();
            room_types.push_back(deluxe);

            hotel_rates_[hotel_id] = room_types;
        }
    }

    hotelreservation::GetRatesResponse process_request(const hotelreservation::GetRatesRequest& req) {
        hotelreservation::GetRatesResponse response;
        
        for (const auto& hotel_id : req.hotel_ids()) {
            auto it = hotel_rates_.find(hotel_id);
            if (it != hotel_rates_.end()) {
                // Create a rate plan for each room type
                for (const auto& room_type : it->second) {
                    auto* rate_plan = response.add_rate_plans();
                    
                    // Apply compression to rate plan data
                    std::string original_hotel_id = hotel_id;
                    std::string compressed_hotel_id = microservice::compression::compress_data(original_hotel_id);
                    rate_plan->set_hotel_id(compressed_hotel_id);
                    
                    std::string original_code = room_type.code();
                    std::string compressed_code = microservice::compression::compress_data(original_code);
                    rate_plan->set_code(compressed_code);
                    
                    std::string original_in_date = req.in_date();
                    std::string compressed_in_date = microservice::compression::compress_data(original_in_date);
                    rate_plan->set_in_date(compressed_in_date);
                    
                    std::string original_out_date = req.out_date();
                    std::string compressed_out_date = microservice::compression::compress_data(original_out_date);
                    rate_plan->set_out_date(compressed_out_date);
                    
                    // Apply compression to room type data
                    auto compressed_room_type = room_type;
                    std::string original_room_code = compressed_room_type.code();
                    std::string compressed_room_code = microservice::compression::compress_data(original_room_code);
                    compressed_room_type.set_code(compressed_room_code);
                    
                    std::string original_room_description = compressed_room_type.room_description();
                    std::string compressed_room_description = microservice::compression::compress_data(original_room_description);
                    compressed_room_type.set_room_description(compressed_room_description);
                    
                    *rate_plan->mutable_room_type() = compressed_room_type;
                    *rate_plan->mutable_padding() = microservice::utils::generate_person_padding();
                }
            }
        }
        
        *response.mutable_padding() = microservice::utils::generate_person_padding();
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/rate_service.sock";
    const int NUM_WORKERS = 16;  // Number of worker processes
    
    PreforkServer server(NUM_WORKERS);
    
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }
    
    std::cout << "Rate service socket setup complete" << std::endl;
    
    // Fork worker processes
    if (server.fork_workers()) {
        // This is a worker process
        RateService service;
        Ser1de_re ser1de;
        
        // Worker process main loop
        worker_loop<RateService, hotelreservation::GetRatesRequest, hotelreservation::GetRatesResponse>(
            server.get_server_fd(), service, ser1de);
        
        return 0;
    } else {
        // This is the master process
        std::cout << "Rate service master process started with " << NUM_WORKERS << " workers" << std::endl;
        server.master_loop();
        return 0;
    }
} 