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
    }

    hotelreservation::RecommendResponse process_request(const hotelreservation::RecommendRequest& req) {
        
        // Get hotel profiles first
        hotelreservation::GetProfilesRequest profile_req;
        for (int i = 1; i <= 10; i++) {
            profile_req.add_hotel_ids(std::to_string(i));
        }
        profile_req.set_locale(req.locale());
        auto pads_profile = microservice::utils::generate_padding_fields();
        profile_req.set_padding1(pads_profile[0]);
        profile_req.set_padding2(pads_profile[1]);
        profile_req.set_padding3(pads_profile[2]);
        profile_req.set_padding4(pads_profile[3]);
        profile_req.set_padding5(pads_profile[4]);
        profile_req.set_padding6(pads_profile[5]);
        profile_req.set_padding7(pads_profile[6]);
        profile_req.set_padding8(pads_profile[7]);
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
        auto pads_rate = microservice::utils::generate_padding_fields();
        rate_req.set_padding1(pads_rate[0]);
        rate_req.set_padding2(pads_rate[1]);
        rate_req.set_padding3(pads_rate[2]);
        rate_req.set_padding4(pads_rate[3]);
        rate_req.set_padding5(pads_rate[4]);
        rate_req.set_padding6(pads_rate[5]);
        rate_req.set_padding7(pads_rate[6]);
        rate_req.set_padding8(pads_rate[7]);
        std::string rate_resp_str = sendProtobufOverUDS("/tmp/rate_service.sock", microservice::utils::serialize_message(ser1de, rate_req));
        hotelreservation::GetRatesResponse rate_resp;
        if (!microservice::utils::deserialize_message(ser1de, rate_resp_str, rate_resp)) {
            return hotelreservation::RecommendResponse();
        }

        // Combine results
        hotelreservation::RecommendResponse response;
        for (const auto& profile : profile_resp.profiles()) {
            *response.add_hotels() = profile;
        }
        
        auto pads = microservice::utils::generate_padding_fields();
        response.set_padding1(pads[0]);
        response.set_padding2(pads[1]);
        response.set_padding3(pads[2]);
        response.set_padding4(pads[3]);
        response.set_padding5(pads[4]);
        response.set_padding6(pads[5]);
        response.set_padding7(pads[6]);
        response.set_padding8(pads[7]);
        
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/recommendation_service.sock";
    const int NUM_WORKERS = 8;  // Number of worker processes
    
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