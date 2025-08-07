#include "../utils/compression_utils.h"
#include <iostream>
#include <string>
#include "hotel_reservation.pb.h"
#include "../utils/serialization_utils.h"
#include "../utils/padding_utils.h"
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../utils/prefork_utils.h"

class RecommendationService {
private:
    // No unused members
    
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
    
    RecommendationService() {
        // Initialize client pools
        // This class is now purely UDS+Protobuf, so no client pools are needed.
        // Initialize compression manager
        microservice::compression::init_compression();
    }

    hotelreservation::RecommendResponse process_request(const hotelreservation::RecommendRequest& req) {
        
        // Get hotel profiles first
        hotelreservation::GetProfilesRequest profile_req;
        for (int i = 1; i <= 10; i++) {
            profile_req.add_hotel_ids(std::to_string(i));
        }
        profile_req.set_locale(req.locale());
        *profile_req.mutable_padding() = microservice::utils::generate_person_padding();
        std::string profile_resp_str = sendProtobufOverUDS("/tmp/profile_service.sock", microservice::utils::serialize_message(ser1de, profile_req));
        hotelreservation::GetProfilesResponse profile_resp;
        if (!microservice::utils::deserialize_message(ser1de, profile_resp_str, profile_resp)) {
            return hotelreservation::RecommendResponse();
        }

        // Get rates for these hotels
        hotelreservation::GetRatesRequest rate_req;
        for (const auto& profile : profile_resp.profiles()) {
            rate_req.add_hotel_ids(profile.id());
        }
        rate_req.set_in_date("2023-12-01");
        rate_req.set_out_date("2023-12-02");
        *rate_req.mutable_padding() = microservice::utils::generate_person_padding();
        std::string rate_resp_str = sendProtobufOverUDS("/tmp/rate_service.sock", microservice::utils::serialize_message(ser1de, rate_req));
        hotelreservation::GetRatesResponse rate_resp;
        if (!microservice::utils::deserialize_message(ser1de, rate_resp_str, rate_resp)) {
            return hotelreservation::RecommendResponse();
        }

        // Combine results
        hotelreservation::RecommendResponse response;
        for (const auto& profile : profile_resp.profiles()) {
            auto hotel = profile;
            
            // Decompress hotel data received from profile service
            std::string decompressed_name = microservice::compression::decompress_data(hotel.name());
            std::string decompressed_description = microservice::compression::decompress_data(hotel.description());
            std::string decompressed_phone = microservice::compression::decompress_data(hotel.phone_number());
            
            // Apply compression to hotel data for our response
            std::string original_name = decompressed_name;
            std::string compressed_name = microservice::compression::compress_data(original_name);
            hotel.set_name(compressed_name);
            
            std::string original_description = decompressed_description;
            std::string compressed_description = microservice::compression::compress_data(original_description);
            hotel.set_description(compressed_description);
            
            std::string original_phone = decompressed_phone;
            std::string compressed_phone = microservice::compression::compress_data(original_phone);
            hotel.set_phone_number(compressed_phone);
            
            // Decompress and re-compress address fields if present
            if (hotel.has_address()) {
                auto* address = hotel.mutable_address();
                
                std::string decompressed_street = microservice::compression::decompress_data(address->street_name());
                std::string decompressed_city = microservice::compression::decompress_data(address->city());
                std::string decompressed_state = microservice::compression::decompress_data(address->state());
                std::string decompressed_country = microservice::compression::decompress_data(address->country());
                std::string decompressed_postal = microservice::compression::decompress_data(address->postal_code());
                
                std::string original_street = decompressed_street;
                std::string compressed_street = microservice::compression::compress_data(original_street);
                address->set_street_name(compressed_street);
                
                std::string original_city = decompressed_city;
                std::string compressed_city = microservice::compression::compress_data(original_city);
                address->set_city(compressed_city);
                
                std::string original_state = decompressed_state;
                std::string compressed_state = microservice::compression::compress_data(original_state);
                address->set_state(compressed_state);
                
                std::string original_country = decompressed_country;
                std::string compressed_country = microservice::compression::compress_data(original_country);
                address->set_country(compressed_country);
                
                std::string original_postal = decompressed_postal;
                std::string compressed_postal = microservice::compression::compress_data(original_postal);
                address->set_postal_code(compressed_postal);
            }
            
            *response.add_hotels() = hotel;
        }
        
        *response.mutable_padding() = microservice::utils::generate_person_padding();
        
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/recommendation_service.sock";
    const int NUM_WORKERS = 16;  // Number of worker processes
    
    PreforkServer server(NUM_WORKERS);
    
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }
    
    std::cout << "Recommendation service socket setup complete" << std::endl;
    
    // Fork worker processes
    if (server.fork_workers()) {
        // This is a worker process
        RecommendationService service;
        Ser1de_re ser1de;
        
        // Worker process main loop
        worker_loop<RecommendationService, hotelreservation::RecommendRequest, hotelreservation::RecommendResponse>(
            server.get_server_fd(), service, ser1de);
        
        return 0;
    } else {
        // This is the master process
        std::cout << "Recommendation service master process started with " << NUM_WORKERS << " workers" << std::endl;
        server.master_loop();
        return 0;
    }
} 