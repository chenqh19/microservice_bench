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
    std::string pre_generated_random_data_;

public:
    RateService() {
        // Pre-generate random data once during initialization
        pre_generated_random_data_.resize(10000);
        for (int i = 0; i < 10000; i++) {
            pre_generated_random_data_[i] = 'A' + (i % 26);
        }
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
        // Useless compression/decompression of random 5000B string
        std::string compressed_random = microservice::compression::compress_data(pre_generated_random_data_);
        std::string decompressed_random = microservice::compression::decompress_data(compressed_random);

        hotelreservation::GetRatesResponse response;
        
        for (const auto& hotel_id : req.hotel_ids()) {
            auto* rate_plan = response.add_rate_plans();
            rate_plan->set_hotel_id(hotel_id);
            rate_plan->set_code("STANDARD");
            rate_plan->set_in_date("2023-12-01");
            rate_plan->set_out_date("2023-12-02");
            
            auto* room_type = rate_plan->mutable_room_type();
            room_type->set_code("KING");
            room_type->set_room_description("King size bed");
            room_type->set_bookable_rate(150.0);
        }
        
        *response.mutable_padding() = microservice::utils::generate_person_padding();
        
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/rate_service.sock";
    const int NUM_WORKERS = 32;  // Number of worker processes
    
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