#include "../utils/compression_utils.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include "hotel_reservation.pb.h"
#include "../utils/serialization_utils.h"
#include "../utils/padding_utils.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <condition_variable>
#include <cstring>
#include "../utils/prefork_utils.h"

class GeoService {
private:
    struct HotelLocation {
        std::string id;
        double lat;
        double lon;
    };
    std::vector<HotelLocation> hotels_;
    std::string pre_generated_random_data_;

    static constexpr double EARTH_RADIUS = 6371.0; // Earth's radius in kilometers
    static constexpr int MAX_SEARCH_RESULTS = 5;
    static constexpr double MAX_SEARCH_RADIUS = 10.0; // kilometers

public:
    GeoService() {
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
        // Initialize first 6 hotels with exact coordinates
        hotels_.push_back({"1", 37.7867, -122.4112});
        hotels_.push_back({"2", 37.7854, -122.4005});
        hotels_.push_back({"3", 37.7854, -122.4071});
        hotels_.push_back({"4", 37.7936, -122.3930});
        hotels_.push_back({"5", 37.7831, -122.4181});
        hotels_.push_back({"6", 37.7863, -122.4015});

        // Add hotels 7-80 with generated coordinates
        for (int i = 7; i <= 80; i++) {
            std::string hotel_id = std::to_string(i);
            double lat = 37.7835 + static_cast<double>(i)/500.0*3;
            double lon = -122.41 + static_cast<double>(i)/500.0*4;
            hotels_.push_back({hotel_id, lat, lon});
        }
    }

    double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
        double dlat = (lat2 - lat1) * M_PI / 180.0;
        double dlon = (lon2 - lon1) * M_PI / 180.0;
        double a = sin(dlat/2) * sin(dlat/2) + cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) * sin(dlon/2) * sin(dlon/2);
        double c = 2 * atan2(sqrt(a), sqrt(1-a));
        return EARTH_RADIUS * c;
    }

    hotelreservation::NearbyResponse process_request(const hotelreservation::NearbyRequest& req) {
        // Useless compression/decompression of random 5000B string
        std::string compressed_random = microservice::compression::compress_data(pre_generated_random_data_);
        std::string decompressed_random = microservice::compression::decompress_data(compressed_random);

        hotelreservation::NearbyResponse response;
        
        // Add some sample hotel IDs
        for (int i = 1; i <= 10; i++) {
            response.add_hotel_ids(std::to_string(i));
        }
        
        *response.mutable_padding() = microservice::utils::generate_person_padding();
        
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/geo_service.sock";
    const int NUM_WORKERS = 64;  // Number of worker processes
    
    PreforkServer server(NUM_WORKERS);
    
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }
    
    std::cout << "Geo service socket setup complete" << std::endl;
    
    // Fork worker processes
    if (server.fork_workers()) {
        // This is a worker process
        GeoService service;
        Ser1de_re ser1de;
        
        // Worker process main loop
        worker_loop<GeoService, hotelreservation::NearbyRequest, hotelreservation::NearbyResponse>(
            server.get_server_fd(), service, ser1de);
        
        return 0;
    } else {
        // This is the master process
        std::cout << "Geo service master process started with " << NUM_WORKERS << " workers" << std::endl;
        server.master_loop();
        return 0;
    }
} 