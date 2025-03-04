#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>

class RecommendationService {
private:
    struct Hotel {
        std::string id;
        double lat;
        double lon;
        double rate;
        double price;
    };
    
    std::unordered_map<std::string, Hotel> hotels_;
    httplib::Client profile_client_;

    static constexpr double EARTH_RADIUS = 6371.0; // Earth's radius in kilometers

public:
    RecommendationService() : profile_client_("profile", 50052) {
        InitializeSampleData();
    }

    void InitializeSampleData() {
        // Initialize first 6 hotels with exact data
        hotels_["1"] = {"1", 37.7867, -122.4112, 109.00, 150.00};
        hotels_["2"] = {"2", 37.7854, -122.4005, 139.00, 120.00};
        hotels_["3"] = {"3", 37.7834, -122.4071, 109.00, 190.00};
        hotels_["4"] = {"4", 37.7936, -122.3930, 129.00, 160.00};
        hotels_["5"] = {"5", 37.7831, -122.4181, 119.00, 140.00};
        hotels_["6"] = {"6", 37.7863, -122.4015, 149.00, 200.00};

        // Add hotels 7-80 with generated data
        for (int i = 7; i <= 80; i++) {
            std::string hotel_id = std::to_string(i);
            double lat = 37.7835 + static_cast<double>(i)/500.0*3;
            double lon = -122.41 + static_cast<double>(i)/500.0*4;

            double rate = 135.00;
            double rate_inc = 179.00;
            if (i % 3 == 0) {
                if (i % 5 == 0) {
                    rate = 109.00;
                    rate_inc = 123.17;
                } else if (i % 5 == 1) {
                    rate = 120.00;
                    rate_inc = 140.00;
                } else if (i % 5 == 2) {
                    rate = 124.00;
                    rate_inc = 144.00;
                } else if (i % 5 == 3) {
                    rate = 132.00;
                    rate_inc = 158.00;
                } else if (i % 5 == 4) {
                    rate = 232.00;
                    rate_inc = 258.00;
                }
            }
            
            hotels_[hotel_id] = {hotel_id, lat, lon, rate, rate_inc};
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

    hotelreservation::RecommendResponse GetRecommendations(const hotelreservation::RecommendRequest& req) {
        hotelreservation::RecommendResponse response;
        std::vector<std::string> recommended_ids;

        if (req.require() == "dis") {
            // Find hotels with minimum distance
            double min_distance = std::numeric_limits<double>::max();
            std::unordered_map<std::string, double> distances;

            // Calculate all distances first
            for (const auto& [id, hotel] : hotels_) {
                double distance = calculateDistance(req.lat(), req.lon(), hotel.lat, hotel.lon);
                distances[id] = distance;
                min_distance = std::min(min_distance, distance);
            }

            // Find all hotels with minimum distance
            for (const auto& [id, distance] : distances) {
                if (std::abs(distance - min_distance) < 1e-10) {  // Use epsilon comparison for doubles
                    recommended_ids.push_back(id);
                }
            }
        } 
        else if (req.require() == "rate") {
            // Find hotels with maximum rate
            double max_rate = -std::numeric_limits<double>::max();
            for (const auto& [_, hotel] : hotels_) {
                max_rate = std::max(max_rate, hotel.rate);
            }

            for (const auto& [id, hotel] : hotels_) {
                if (std::abs(hotel.rate - max_rate) < 1e-10) {
                    recommended_ids.push_back(id);
                }
            }
        }
        else if (req.require() == "price") {
            // Find hotels with minimum price
            double min_price = std::numeric_limits<double>::max();
            for (const auto& [_, hotel] : hotels_) {
                min_price = std::min(min_price, hotel.price);
            }

            for (const auto& [id, hotel] : hotels_) {
                if (std::abs(hotel.price - min_price) < 1e-10) {
                    recommended_ids.push_back(id);
                }
            }
        }

        // Get hotel profiles for recommended hotels
        if (!recommended_ids.empty()) {
            hotelreservation::GetProfilesRequest profile_req;
            for (const auto& id : recommended_ids) {
                profile_req.add_hotel_ids(id);
            }
            profile_req.set_locale(req.locale());

            auto profile_result = profile_client_.Post("/get_profiles", 
                microservice::utils::serialize_message(profile_req), 
                "application/x-protobuf");

            if (profile_result && profile_result->status == 200) {
                hotelreservation::GetProfilesResponse profile_resp;
                if (microservice::utils::deserialize_message(profile_result->body, profile_resp)) {
                    for (const auto& profile : profile_resp.profiles()) {
                        *response.add_hotels() = profile;
                    }
                }
            }
        }

        return response;
    }
};

int main() {
    httplib::Server svr;
    RecommendationService service;

    svr.Post("/recommend", [&](const httplib::Request& req, httplib::Response& res) {
        hotelreservation::RecommendRequest request;
        if (microservice::utils::deserialize_message(req.body, request)) {
            auto response = service.GetRecommendations(request);
            std::string serialized_response = microservice::utils::serialize_message(response);
            res.set_content(serialized_response, "application/x-protobuf");
        } else {
            res.status = 400;
        }
    });

    std::cout << "Recommendation service listening on 0.0.0.0:50053" << std::endl;
    svr.listen("0.0.0.0", 50053);

    return 0;
} 