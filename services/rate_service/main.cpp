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
        pre_generated_random_data_.resize(2000);
        for (int i = 0; i < 2000; i++) {
            pre_generated_random_data_[i] = 'A' + (i % 26);
        }
        InitializeSampleRates();
        // Initialize compression manager
        microservice::compression::init_compression();
    }

    void InitializeSampleRates() {
        // Add multiple room types with richer, longer descriptions for each hotel
        for (int i = 1; i <= 20; i++) {
            std::string hotel_id = std::to_string(i);
            std::vector<hotelreservation::RoomType> room_types;

            auto make_rate = [](const std::string& code,
                                const std::string& desc,
                                double base) {
                hotelreservation::RoomType rt;
                rt.set_code(code);
                rt.set_room_description(desc);
                rt.set_bookable_rate(base);
                rt.set_total_rate(rt.bookable_rate() * 1.1);
                rt.set_total_rate_inclusive(rt.total_rate() * 1.2);
                *rt.mutable_padding() = microservice::utils::generate_person_padding();
                return rt;
            };

            room_types.push_back(make_rate(
                "QUEEN",
                "A quiet interior QUEEN designed for deep rest with layered linens, blackout drapery, "
                "and a compact writing desk. Ideal for explorers who value a calm, cocooned finish to the day.",
                149.0 + (i % 7) * 3.0
            ));

            room_types.push_back(make_rate(
                "KING",
                "A generous KING layout with a lounge chair by the window for slow mornings over coffee. "
                "Rainfall shower, warm ambient lighting, and subtle acoustic treatment for multi-night comfort.",
                189.0 + (i % 5) * 4.0
            ));

            room_types.push_back(make_rate(
                "DOUBLE",
                "Flexible DOUBLE for friends or families, with a larger work surface, ample charging, "
                "and a quiet corner perfect for sketching out the day’s city itinerary.",
                209.0 + (i % 6) * 2.5
            ));

            room_types.push_back(make_rate(
                "SUITE",
                "One-bedroom SUITE offering a separate living room for entertaining or decompressing. "
                "Curated art, a well-stocked minibar, and a deep soaking tub elevate the stay.",
                289.0 + (i % 4) * 6.0
            ));

            room_types.push_back(make_rate(
                "PENTHOUSE",
                "Top-floor PENTHOUSE with sweeping city views, an intimate dining nook for in-room tastings, "
                "and a terrace where sunsets spill across the skyline.",
                449.0 + (i % 3) * 10.0
            ));

            hotel_rates_[hotel_id] = room_types;
        }
    }

    hotelreservation::GetRatesResponse process_request(const hotelreservation::GetRatesRequest& req) {
		// Optional dummy compression
#if ENABLE_DUMMY_SERVICE_COMPRESSION
		std::string compressed_random = microservice::compression::compress_data(pre_generated_random_data_);
		std::string decompressed_random = microservice::compression::decompress_data(compressed_random);
#endif

        hotelreservation::GetRatesResponse response;
        
        for (const auto& hotel_id : req.hotel_ids()) {
            auto it = hotel_rates_.find(hotel_id);
            if (it == hotel_rates_.end()) {
                continue;
            }
            for (const auto& rt : it->second) {
                auto* rate_plan = response.add_rate_plans();
                rate_plan->set_hotel_id(hotel_id);
                rate_plan->set_code(rt.code());
                rate_plan->set_in_date("2023-12-01");
                rate_plan->set_out_date("2023-12-02");
                *rate_plan->mutable_room_type() = rt;
            }
        }
        
        *response.mutable_padding() = microservice::utils::generate_person_padding();
        
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/rate_service.sock";
    const int NUM_WORKERS = 64;  // Number of worker processes
    
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