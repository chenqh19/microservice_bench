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
#include "../thread_pool.h"

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
    }

    hotelreservation::SearchResponse Search(const hotelreservation::SearchRequest& req) {
        // First, get nearby hotels from geo service
        hotelreservation::NearbyRequest geo_req;
        geo_req.set_lat(req.lat());
        geo_req.set_lon(req.lon());
        geo_req.set_padding(microservice::utils::generate_padding());
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
        rate_req.set_padding(microservice::utils::generate_padding());
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
        profile_req.set_padding(microservice::utils::generate_padding());
        std::string profile_resp_str = sendProtobufOverUDS("/tmp/profile_service.sock", microservice::utils::serialize_message(ser1de, profile_req));
        hotelreservation::GetProfilesResponse profile_resp;
        if (!microservice::utils::deserialize_message(ser1de, profile_resp_str, profile_resp)) {
            return hotelreservation::SearchResponse();
        }
        // Combine results
        hotelreservation::SearchResponse response;
        for (const auto& profile : profile_resp.profiles()) {
            *response.add_hotels() = profile;
        }
        response.set_padding(microservice::utils::generate_padding());
        return response;
    }
};

void handle_client(int client_fd, SearchService& service, Ser1de_re& ser1de) {
    char len_buf[4];
    ssize_t n = read(client_fd, len_buf, 4);
    if (n != 4) { close(client_fd); return; }
    uint32_t msg_len = 0;
    memcpy(&msg_len, len_buf, 4);
    std::vector<char> buf(msg_len);
    n = read(client_fd, buf.data(), msg_len);
    if (n != (ssize_t)msg_len) { close(client_fd); return; }
    hotelreservation::SearchRequest request;
    bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), request);
    if (ok) {
        auto response = service.Search(request);
        std::string resp_str = microservice::utils::serialize_message(ser1de, response);
        uint32_t resp_len = resp_str.size();
        write(client_fd, &resp_len, 4);
        write(client_fd, resp_str.data(), resp_len);
    }
    close(client_fd);
}

int main() {
    const char* socket_path = "/tmp/search_service.sock";
    unlink(socket_path); // Remove if exists
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    chmod(socket_path, 0777); // Ensure world-writable for Docker
    if (listen(server_fd, 1024) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }
    std::cout << "Search service listening on unix://" << socket_path << std::endl;
    
    SearchService service;
    Ser1de_re ser1de;
    ThreadPool pool(64); // Use 64 threads for the pool
    
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) continue;
        pool.enqueue_task([client_fd, &service, &ser1de]() {
            handle_client(client_fd, service, ser1de);
        });
    }
    close(server_fd);
    return 0;
} 