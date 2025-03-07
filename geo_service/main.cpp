#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>

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
        // Convert to radians
        lat1 = lat1 * M_PI / 180.0;
        lon1 = lon1 * M_PI / 180.0;
        lat2 = lat2 * M_PI / 180.0;
        lon2 = lon2 * M_PI / 180.0;

        // Haversine formula
        double dlat = lat2 - lat1;
        double dlon = lon2 - lon1;
        double a = std::sin(dlat/2) * std::sin(dlat/2) +
                   std::cos(lat1) * std::cos(lat2) *
                   std::sin(dlon/2) * std::sin(dlon/2);
        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1-a));
        return EARTH_RADIUS * c;
    }

    hotelreservation::NearbyResponse GetNearbyHotels(const hotelreservation::NearbyRequest& req) {
        hotelreservation::NearbyResponse response;
        std::vector<std::pair<std::string, double>> distances;

        // Calculate distances to all hotels
        for (const auto& hotel : hotels_) {
            double distance = calculateDistance(req.lat(), req.lon(), 
                                             hotel.lat, hotel.lon);
            if (distance <= MAX_SEARCH_RADIUS) {  // Only include hotels within radius
                distances.push_back({hotel.id, distance});
            }
        }

        // Sort by distance
        std::sort(distances.begin(), distances.end(),
                 [](const auto& a, const auto& b) { return a.second < b.second; });

        // Return top MAX_SEARCH_RESULTS closest hotels
        for (int i = 0; i < std::min(MAX_SEARCH_RESULTS, (int)distances.size()); i++) {
            response.add_hotel_ids(distances[i].first);
        }

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
        }
        return point;
    }
};

int main() {
    httplib::Server svr;
    GeoService service;

    // Set up multi-threading options
    svr.new_task_queue = [] { return new httplib::ThreadPool(256); }; // Create thread pool with 8 threads

    svr.Post("/nearby", [&](const httplib::Request& req, httplib::Response& res) {
        hotelreservation::NearbyRequest request;
        if (microservice::utils::deserialize_message(req.body, request)) {
            auto response = service.GetNearbyHotels(request);
            std::string serialized_response = microservice::utils::serialize_message(response);
            res.set_content(serialized_response, "application/x-protobuf");
        } else {
            res.status = 400;
        }
    });

    svr.Get("/point/:hotel_id", [&](const httplib::Request& req, httplib::Response& res) {
        auto hotel_id = req.path_params.at("hotel_id");
        auto point = service.GetPoint(hotel_id);
        std::string serialized_response = microservice::utils::serialize_message(point);
        res.set_content(serialized_response, "application/x-protobuf");
    });

    std::cout << "Geo service listening on 0.0.0.0:50056 with 256 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50056);

    return 0;
} 