#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include "padding_utils.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <condition_variable>
#include <cstring>
#include "../thread_pool.h"

class GeoService {
private:
    struct HotelLocation {
        std::string id;
        double lat;
        double lon;
    };
    std::vector<HotelLocation> hotels_;

    static constexpr double EARTH_RADIUS = 6371.0; // Earth's radius in kilometers
    static constexpr int MAX_SEARCH_RESULTS = 5;
    static constexpr double MAX_SEARCH_RADIUS = 10.0; // kilometers

public:
    GeoService() {
        InitializeSampleData();
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

    hotelreservation::NearbyResponse GetNearbyHotels(const hotelreservation::NearbyRequest& req) {
        hotelreservation::NearbyResponse response;
        
        std::vector<std::pair<double, std::string>> distances;
        
        for (const auto& hotel : hotels_) {
            double distance = calculateDistance(req.lat(), req.lon(), hotel.lat, hotel.lon);
            if (distance <= MAX_SEARCH_RADIUS) {
                distances.push_back({distance, hotel.id});
            }
        }
        
        // Sort by distance and take top results
        std::sort(distances.begin(), distances.end());
        
        for (size_t i = 0; i < std::min(distances.size(), static_cast<size_t>(MAX_SEARCH_RESULTS)); ++i) {
            response.add_hotel_ids(distances[i].second);
        }
        
        response.set_padding(microservice::utils::generate_padding());
        return response;
    }

    hotelreservation::Point GetPoint(const std::string& hotel_id) {
        hotelreservation::Point point;
        auto it = std::find_if(hotels_.begin(), hotels_.end(),
                              [&](const HotelLocation& h) { return h.id == hotel_id; });
        
        if (it != hotels_.end()) {
            point.set_pid(it->id);
            point.set_plat(it->lat);
            point.set_plon(it->lon);
            point.set_padding(microservice::utils::generate_padding());
        }
        return point;
    }
};

void handle_client(int client_fd, GeoService& service, Ser1de_re& ser1de) {
    char len_buf[4];
    ssize_t n = read(client_fd, len_buf, 4);
    if (n != 4) { close(client_fd); return; }
    uint32_t msg_len = 0;
    memcpy(&msg_len, len_buf, 4);
    std::vector<char> buf(msg_len);
    n = read(client_fd, buf.data(), msg_len);
    if (n != (ssize_t)msg_len) { close(client_fd); return; }
    hotelreservation::NearbyRequest req;
    bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), req);
    if (!ok) { close(client_fd); return; }
    auto response = service.GetNearbyHotels(req);
    std::string resp_str = microservice::utils::serialize_message(ser1de, response);
    uint32_t resp_len = resp_str.size();
    write(client_fd, &resp_len, 4);
    write(client_fd, resp_str.data(), resp_len);
    close(client_fd);
}

int main() {
    const char* socket_path = "/tmp/geo_service.sock";
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
    std::cout << "Geo service listening on unix://" << socket_path << std::endl;
    
    GeoService service;
    Ser1de_re ser1de;
    ThreadPool pool(64); // Use 64 threads for the pool
    
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) continue;
        pool.enqueue_task([client_fd, &service]() {
            handle_client(client_fd, service, ser1de);
        });
    }
    close(server_fd);
    return 0;
} 