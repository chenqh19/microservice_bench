#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <condition_variable>

class RecommendationService {
private:
    static const int POOL_SIZE = 1024;
    static const int MAX_CONCURRENT_CONNECTIONS = 512;
    std::atomic<size_t> active_connections_{0};
    
    struct ClientInfo {
        std::unique_ptr<httplib::Client> client;
        std::atomic<bool> in_use;
        std::chrono::steady_clock::time_point last_used;

        ClientInfo() : client(nullptr), in_use(false), last_used(std::chrono::steady_clock::now()) {}
        
        ClientInfo(std::unique_ptr<httplib::Client>&& c) 
            : client(std::move(c)), in_use(false), last_used(std::chrono::steady_clock::now()) {}
        
        ClientInfo(const ClientInfo&) = delete;
        ClientInfo& operator=(const ClientInfo&) = delete;
        
        ClientInfo(ClientInfo&& other) noexcept
            : client(std::move(other.client)), in_use(false), last_used(std::chrono::steady_clock::now()) {}
        
        ClientInfo& operator=(ClientInfo&& other) noexcept {
            client = std::move(other.client);
            bool expected = other.in_use.load();
            in_use.store(expected);
            last_used = std::chrono::steady_clock::now();
            return *this;
        }
    };

    struct Hotel {
        std::string id;
        double lat;
        double lon;
        double rate;
        double price;
    };

    std::vector<ClientInfo> profile_clients_;
    std::vector<ClientInfo> rate_clients_;
    std::atomic<size_t> current_profile_idx_{0};
    std::atomic<size_t> current_rate_idx_{0};
    std::unordered_map<std::string, Hotel> hotels_;
    static constexpr double EARTH_RADIUS = 6371.0;
    std::mutex connection_mutex_;
    std::condition_variable connection_cv_;
    std::atomic<size_t> successful_recommendations_{0};
    std::atomic<size_t> total_recommendations_{0};

    httplib::Client* getNextAvailableClient(std::vector<ClientInfo>& clients, std::atomic<size_t>& current_idx) {
        size_t start_idx = current_idx.fetch_add(1) % POOL_SIZE;
        size_t current = start_idx;
        
        // First try to find an available client without waiting
        do {
            bool expected = false;
            if (clients[current].in_use.compare_exchange_strong(expected, true)) {
                clients[current].last_used = std::chrono::steady_clock::now();
                return clients[current].client.get();
            }
            current = (current + 1) % POOL_SIZE;
        } while (current != start_idx);
        
        // If no client is available, wait for one with a timeout
        std::unique_lock<std::mutex> lock(connection_mutex_);
        if (connection_cv_.wait_for(lock, std::chrono::milliseconds(50), 
            [this] { return active_connections_ < MAX_CONCURRENT_CONNECTIONS; })) {
            // Try one more round after waiting
            current = start_idx;
            do {
                bool expected = false;
                if (clients[current].in_use.compare_exchange_strong(expected, true)) {
                    clients[current].last_used = std::chrono::steady_clock::now();
                    active_connections_++;
                    return clients[current].client.get();
                }
                current = (current + 1) % POOL_SIZE;
            } while (current != start_idx);
        }
        
        return nullptr;
    }

    void releaseClient(std::vector<ClientInfo>& clients, httplib::Client* client) {
        for (auto& info : clients) {
            if (info.client.get() == client) {
                info.in_use.store(false);
                active_connections_--;
                connection_cv_.notify_one();
                break;
            }
        }
    }

    void monitorResources() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::cout << "Resource Usage - Active Connections: " << active_connections_ 
                      << ", Total Recommendations: " << total_recommendations_ 
                      << ", Successful Recommendations: " << successful_recommendations_ << std::endl;
        }
    }

public:
    RecommendationService() {
        // Initialize client pools
        for (int i = 0; i < POOL_SIZE; i++) {
            profile_clients_.push_back(ClientInfo(std::make_unique<httplib::Client>("profile", 50052)));
            rate_clients_.push_back(ClientInfo(std::make_unique<httplib::Client>("rate", 50057)));
        }
        InitializeSampleData();

        // Start resource monitoring thread
        std::thread monitor_thread(&RecommendationService::monitorResources, this);
        monitor_thread.detach();
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

    hotelreservation::RecommendResponse Recommend(const hotelreservation::RecommendRequest& req) {
        total_recommendations_++;
        
        // Get hotel profiles first
        hotelreservation::GetProfilesRequest profile_req;
        for (int i = 1; i <= 10; i++) {
            profile_req.add_hotel_ids(std::to_string(i));
        }
        profile_req.set_locale(req.locale());

        auto* profile_client = getNextAvailableClient(profile_clients_, current_profile_idx_);
        if (!profile_client) {
            return hotelreservation::RecommendResponse();
        }

        auto profile_result = profile_client->Post("/get_profiles", 
            microservice::utils::serialize_message(profile_req), 
            "application/x-protobuf");
        releaseClient(profile_clients_, profile_client);

        hotelreservation::GetProfilesResponse profile_resp;
        if (!profile_result || profile_result->status != 200 || 
            !microservice::utils::deserialize_message(profile_result->body, profile_resp)) {
            return hotelreservation::RecommendResponse();
        }

        // Get rates for these hotels
        hotelreservation::GetRatesRequest rate_req;
        for (const auto& profile : profile_resp.profiles()) {
            rate_req.add_hotel_ids(profile.id());
        }
        rate_req.set_in_date("2023-12-01");
        rate_req.set_out_date("2023-12-02");

        auto* rate_client = getNextAvailableClient(rate_clients_, current_rate_idx_);
        if (!rate_client) {
            return hotelreservation::RecommendResponse();
        }

        auto rate_result = rate_client->Post("/get_rates", 
            microservice::utils::serialize_message(rate_req), 
            "application/x-protobuf");
        releaseClient(rate_clients_, rate_client);

        hotelreservation::GetRatesResponse rate_resp;
        if (!rate_result || rate_result->status != 200 || 
            !microservice::utils::deserialize_message(rate_result->body, rate_resp)) {
            return hotelreservation::RecommendResponse();
        }

        // Combine results
        hotelreservation::RecommendResponse response;
        for (const auto& profile : profile_resp.profiles()) {
            *response.add_hotels() = profile;
        }
        
        successful_recommendations_++;
        return response;
    }
};

int main() {
    httplib::Server svr;
    RecommendationService service;

    // Set up multi-threading options
    svr.new_task_queue = [] { return new httplib::ThreadPool(256); };

    svr.Post("/recommend", [&](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();

        auto check_timeout = [&start_time]() -> bool {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            return elapsed > 100; // 100ms timeout
        };

        hotelreservation::RecommendRequest request;
        if (!microservice::utils::deserialize_message(req.body, request)) {
            res.status = 400;
            res.set_content("{\"error\": \"Failed to deserialize request\"}", "application/json");
            return;
        }

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during deserialization\"}", "application/json");
            return;
        }

        auto response = service.Recommend(request);

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during processing\"}", "application/json");
            return;
        }

        std::string serialized_response = microservice::utils::serialize_message(response);

        if (check_timeout()) {
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout during serialization\"}", "application/json");
            return;
        }

        res.set_content(serialized_response, "application/x-protobuf");
    });

    std::cout << "Recommendation service listening on 0.0.0.0:50053 with 256 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50053);

    return 0;
} 