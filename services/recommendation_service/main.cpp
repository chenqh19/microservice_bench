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
    std::string pre_generated_random_data_;
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
        // Pre-generate random data once during initialization
        pre_generated_random_data_.resize(20000);
        for (int i = 0; i < 20000; i++) {
            pre_generated_random_data_[i] = 'A' + (i % 26);
        }
        // Initialize client pools
        // This class is now purely UDS+Protobuf, so no client pools are needed.
        // Initialize compression manager
        microservice::compression::init_compression();
    }

    hotelreservation::RecommendResponse process_request(const hotelreservation::RecommendRequest& req) {
        // Useless compression/decompression of random 5000B string
        std::string compressed_random = microservice::compression::compress_data(pre_generated_random_data_);
        std::string decompressed_random = microservice::compression::decompress_data(compressed_random);

        // Generate hotel IDs based on location (similar to geo service)
        std::vector<std::string> hotel_ids;
        for (int i = 1; i <= 10; i++) {
            hotel_ids.push_back(std::to_string(i));
        }

        // Get hotel profiles
        hotelreservation::GetProfilesRequest profile_req;
        for (const auto& hotel_id : hotel_ids) {
            profile_req.add_hotel_ids(hotel_id);
        }
        *profile_req.mutable_padding() = microservice::utils::generate_person_padding();
        
        std::string profile_resp_str = sendProtobufOverUDS("/tmp/profile_service.sock", microservice::utils::serialize_message(ser1de, profile_req));
        hotelreservation::GetProfilesResponse profile_resp;
        if (!microservice::utils::deserialize_message(ser1de, profile_resp_str, profile_resp)) {
            return hotelreservation::RecommendResponse();
        }

        // Combine results
        hotelreservation::RecommendResponse response;
        for (const auto& profile : profile_resp.profiles()) {
            auto hotel = profile;
            *response.add_hotels() = hotel;
        }
        
        *response.mutable_padding() = microservice::utils::generate_person_padding();
        
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/recommendation_service.sock";
    const int NUM_WORKERS = 32;  // Number of worker processes
    
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