#include <iostream>
#include <string>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>
#include <vector>
#include <atomic>
#include <thread>

class SearchService {
private:
    static const int POOL_SIZE = 3000;
    std::atomic<size_t> successful_searches_{0};
    std::atomic<size_t> total_searches_{0};
    std::atomic<size_t> total_received_{0};
    
    struct ClientInfo {
        std::unique_ptr<httplib::Client> client;
        std::atomic<bool> in_use;

        ClientInfo() : client(nullptr), in_use(false) {}
        
        ClientInfo(std::unique_ptr<httplib::Client>&& c) 
            : client(std::move(c)), in_use(false) {}
        
        ClientInfo(const ClientInfo&) = delete;
        ClientInfo& operator=(const ClientInfo&) = delete;
        
        ClientInfo(ClientInfo&& other) noexcept
            : client(std::move(other.client)), in_use(false) {}
        
        ClientInfo& operator=(ClientInfo&& other) noexcept {
            client = std::move(other.client);
            bool expected = other.in_use.load();
            in_use.store(expected);
            return *this;
        }
    };

    std::vector<ClientInfo> geo_clients_;
    std::vector<ClientInfo> rate_clients_;
    std::vector<ClientInfo> profile_clients_;
    std::atomic<size_t> current_geo_idx_{0};
    std::atomic<size_t> current_rate_idx_{0};
    std::atomic<size_t> current_profile_idx_{0};

    httplib::Client* getNextAvailableClient(std::vector<ClientInfo>& clients, 
                                          std::atomic<size_t>& current_idx) {
        size_t start_idx = current_idx.fetch_add(1) % POOL_SIZE;
        size_t current = start_idx;
        
        do {
            bool expected = false;
            if (clients[current].in_use.compare_exchange_strong(expected, true)) {
                return clients[current].client.get();
            }
            current = (current + 1) % POOL_SIZE;
        } while (current != start_idx);
        
        // If no client is available, try one more round with a small delay
        current = start_idx;
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            bool expected = false;
            if (clients[current].in_use.compare_exchange_strong(expected, true)) {
                return clients[current].client.get();
            }
            current = (current + 1) % POOL_SIZE;
        } while (current != start_idx);
        
        return nullptr;
    }

    void releaseClient(std::vector<ClientInfo>& clients, httplib::Client* client) {
        for (auto& info : clients) {
            if (info.client.get() == client) {
                info.in_use.store(false);
                break;
            }
        }
    }

public:
    SearchService() {
        // Initialize client pools
        for (int i = 0; i < POOL_SIZE; i++) {
            geo_clients_.push_back(ClientInfo(std::make_unique<httplib::Client>("geo", 50056)));
            rate_clients_.push_back(ClientInfo(std::make_unique<httplib::Client>("rate", 50057)));
            profile_clients_.push_back(ClientInfo(std::make_unique<httplib::Client>("profile", 50052)));
        }
    }

    hotelreservation::SearchResponse Search(const hotelreservation::SearchRequest& req) {
        size_t current_total = total_searches_.fetch_add(1);
        if (current_total % 1000 == 0) {
            std::cout << "Received " << current_total + 1 << " total search requests" << std::endl;
        }

        // First, get nearby hotels from geo service
        hotelreservation::NearbyRequest geo_req;
        geo_req.set_lat(req.lat());
        geo_req.set_lon(req.lon());
        
        auto* geo_client = getNextAvailableClient(geo_clients_, current_geo_idx_);
        if (!geo_client) {
            return hotelreservation::SearchResponse();
        }

        auto geo_result = geo_client->Post("/nearby", 
            microservice::utils::serialize_message(geo_req), 
            "application/x-protobuf");
        releaseClient(geo_clients_, geo_client);

        hotelreservation::NearbyResponse geo_resp;
        if (!geo_result || geo_result->status != 200 || 
            !microservice::utils::deserialize_message(geo_result->body, geo_resp)) {
            return hotelreservation::SearchResponse();
        }

        // Get rates for these hotels
        hotelreservation::GetRatesRequest rate_req;
        for (const auto& hotel_id : geo_resp.hotel_ids()) {
            rate_req.add_hotel_ids(hotel_id);
        }
        rate_req.set_in_date(req.in_date());
        rate_req.set_out_date(req.out_date());

        auto* rate_client = getNextAvailableClient(rate_clients_, current_rate_idx_);
        if (!rate_client) {
            return hotelreservation::SearchResponse();
        }

        auto rate_result = rate_client->Post("/get_rates", 
            microservice::utils::serialize_message(rate_req), 
            "application/x-protobuf");
        releaseClient(rate_clients_, rate_client);

        hotelreservation::GetRatesResponse rate_resp;
        if (!rate_result || rate_result->status != 200 || 
            !microservice::utils::deserialize_message(rate_result->body, rate_resp)) {
            return hotelreservation::SearchResponse();
        }

        // Get hotel profiles
        hotelreservation::GetProfilesRequest profile_req;
        for (const auto& hotel_id : geo_resp.hotel_ids()) {
            profile_req.add_hotel_ids(hotel_id);
        }
        profile_req.set_locale(req.locale());

        auto* profile_client = getNextAvailableClient(profile_clients_, current_profile_idx_);
        if (!profile_client) {
            return hotelreservation::SearchResponse();
        }

        auto profile_result = profile_client->Post("/get_profiles", 
            microservice::utils::serialize_message(profile_req), 
            "application/x-protobuf");
        releaseClient(profile_clients_, profile_client);

        hotelreservation::GetProfilesResponse profile_resp;
        if (!profile_result || profile_result->status != 200 || 
            !microservice::utils::deserialize_message(profile_result->body, profile_resp)) {
            return hotelreservation::SearchResponse();
        }

        // Combine results
        hotelreservation::SearchResponse response;
        for (const auto& profile : profile_resp.profiles()) {
            *response.add_hotels() = profile;
        }
        
        size_t current_success = successful_searches_.fetch_add(1);
        if (current_success % 1000 == 0) {
            std::cout << "Successfully completed " << current_success + 1 << " searches" << std::endl;
        }
        
        return response;
    }
};

int main() {
    httplib::Server svr;
    SearchService service;

    // Match the thread pool size with client pool size
    svr.new_task_queue = [] { return new httplib::ThreadPool(3000); };

    // Add server configurations
    svr.set_keep_alive_max_count(20000);  // Allow more keep-alive connections
    svr.set_read_timeout(5);              // 5 second read timeout
    svr.set_write_timeout(5);             // 5 second write timeout
    svr.set_payload_max_length(1024*1024); // Set maximum request size if needed

    svr.Post("/search", [&](const httplib::Request& req, httplib::Response& res) {
        static std::atomic<size_t> received{0};
        size_t current = received.fetch_add(1);
        if (current % 1000 == 0) {
            std::cout << "HTTP server received " << current + 1 << " total requests" << std::endl;
        }

        hotelreservation::SearchRequest request;
        if (microservice::utils::deserialize_message(req.body, request)) {
            auto response = service.Search(request);
            std::string serialized_response = microservice::utils::serialize_message(response);
            res.set_content(serialized_response, "application/x-protobuf");
        } else {
            std::cout << "Failed to deserialize request" << std::endl;
            res.status = 400;
        }
    });

    std::cout << "Search service listening on 0.0.0.0:50051 with 3000 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50051);

    return 0;
} 