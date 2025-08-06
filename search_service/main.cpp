#include "../compression_utils.h"
#include <iostream>
#include <string>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include "padding_utils.h"
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
#include "../prefork_utils.h"

class SearchService {
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
    
    SearchService() {
        // Initialize client pools
        // This class is now purely UDS+Protobuf, so no client pools are needed.
        // Initialize compression manager
        microservice::compression::init_compression();
    }

    hotelreservation::SearchResponse process_request(const hotelreservation::SearchRequest& req) {
        // First, get nearby hotels from geo service
        hotelreservation::NearbyRequest geo_req;
        geo_req.set_lat(req.lat());
        geo_req.set_lon(req.lon());
        *geo_req.mutable_padding() = microservice::utils::generate_person_padding();
        std::string geo_resp_str = sendProtobufOverUDS("/tmp/geo_service.sock", microservice::utils::serialize_message(ser1de, geo_req));
        hotelreservation::NearbyResponse geo_resp;
        if (!microservice::utils::deserialize_message(ser1de, geo_resp_str, geo_resp)) {
            return hotelreservation::SearchResponse();
        }
        // Get rates for these hotels
        hotelreservation::GetRatesRequest rate_req;
        for (const auto& hotel_id : geo_resp.hotel_ids()) {
            rate_req.add_hotel_ids(hotel_id);
        }
        rate_req.set_in_date(req.in_date());
        rate_req.set_out_date(req.out_date());
        *rate_req.mutable_padding() = microservice::utils::generate_person_padding();
        std::string rate_resp_str = sendProtobufOverUDS("/tmp/rate_service.sock", microservice::utils::serialize_message(ser1de, rate_req));
        hotelreservation::GetRatesResponse rate_resp;
        if (!microservice::utils::deserialize_message(ser1de, rate_resp_str, rate_resp)) {
            return hotelreservation::SearchResponse();
        }
        // Get hotel profiles
        hotelreservation::GetProfilesRequest profile_req;
        for (const auto& hotel_id : geo_resp.hotel_ids()) {
            profile_req.add_hotel_ids(hotel_id);
        }
        profile_req.set_locale(req.locale());
        *profile_req.mutable_padding() = microservice::utils::generate_person_padding();
        std::string profile_resp_str = sendProtobufOverUDS("/tmp/profile_service.sock", microservice::utils::serialize_message(ser1de, profile_req));
        hotelreservation::GetProfilesResponse profile_resp;
        if (!microservice::utils::deserialize_message(ser1de, profile_resp_str, profile_resp)) {
            return hotelreservation::SearchResponse();
        }
        // Combine results
        hotelreservation::SearchResponse response;
        for (const auto& profile : profile_resp.profiles()) {
            auto hotel = profile;
            
            // Apply compression to hotel data
            std::string original_name = hotel.name();
            std::string compressed_name = microservice::compression::compress_data(original_name);
            hotel.set_name(compressed_name);
            
            std::string original_description = hotel.description();
            std::string compressed_description = microservice::compression::compress_data(original_description);
            hotel.set_description(compressed_description);
            
            std::string original_phone = hotel.phone_number();
            std::string compressed_phone = microservice::compression::compress_data(original_phone);
            hotel.set_phone_number(compressed_phone);
            
            // Compress address fields if present
            if (hotel.has_address()) {
                auto* address = hotel.mutable_address();
                std::string original_street = address->street_name();
                std::string compressed_street = microservice::compression::compress_data(original_street);
                address->set_street_name(compressed_street);
                
                std::string original_city = address->city();
                std::string compressed_city = microservice::compression::compress_data(original_city);
                address->set_city(compressed_city);
                
                std::string original_state = address->state();
                std::string compressed_state = microservice::compression::compress_data(original_state);
                address->set_state(compressed_state);
                
                std::string original_country = address->country();
                std::string compressed_country = microservice::compression::compress_data(original_country);
                address->set_country(compressed_country);
                
                std::string original_postal = address->postal_code();
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
    const char* socket_path = "/tmp/search_service.sock";
    const int NUM_WORKERS = 16;  // Number of worker processes
    
    PreforkServer server(NUM_WORKERS);
    
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }
    
    std::cout << "Search service socket setup complete" << std::endl;
    
    // Fork worker processes
    if (server.fork_workers()) {
        // This is a worker process
        SearchService service;
        Ser1de_re ser1de;
        
        // Worker process main loop
        worker_loop<SearchService, hotelreservation::SearchRequest, hotelreservation::SearchResponse>(
            server.get_server_fd(), service, ser1de);
        
        return 0;
    } else {
        // This is the master process
        std::cout << "Search service master process started with " << NUM_WORKERS << " workers" << std::endl;
        server.master_loop();
        return 0;
    }
} 