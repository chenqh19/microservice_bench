#include <iostream>
#include <string>
#include <json/json.h>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
#include <sched.h>
#include <unordered_map>
#include <queue>

class FrontEndService {
private:
    static const int POOL_SIZE = 256;
    static const int MAX_RETRIES = 3;
    static const int RETRY_DELAY_MS = 10;
    
    std::atomic<uint64_t> search_requests_received_{0};
    std::atomic<uint64_t> search_requests_completed_{0};
    std::atomic<uint64_t> search_requests_failed_{0};
    std::atomic<uint64_t> search_requests_retried_{0};
    std::atomic<bool> monitoring_active_{true};
    std::thread monitoring_thread_;
    
    struct ClientInfo {
        std::unique_ptr<httplib::Client> client;
        std::atomic<bool> in_use;
        std::atomic<uint64_t> last_used;
        std::atomic<uint64_t> error_count;

        ClientInfo() : client(nullptr), in_use(false), last_used(0), error_count(0) {}
        
        ClientInfo(std::unique_ptr<httplib::Client>&& c) 
            : client(std::move(c)), in_use(false), last_used(0), error_count(0) {}
        
        ClientInfo(const ClientInfo&) = delete;
        ClientInfo& operator=(const ClientInfo&) = delete;
        
        ClientInfo(ClientInfo&& other) noexcept
            : client(std::move(other.client)), in_use(false), last_used(0), error_count(0) {}
        
        ClientInfo& operator=(ClientInfo&& other) noexcept {
            client = std::move(other.client);
            bool expected = other.in_use.load();
            in_use.store(expected);
            return *this;
        }
    };

    std::vector<ClientInfo> search_clients_;
    std::vector<ClientInfo> recommend_clients_;
    std::vector<ClientInfo> user_clients_;
    std::vector<ClientInfo> reservation_clients_;
    std::atomic<size_t> current_search_idx_{0};
    std::atomic<size_t> current_recommend_idx_{0};
    std::atomic<size_t> current_user_idx_{0};
    std::atomic<size_t> current_reservation_idx_{0};

    // Metrics collection
    struct Metrics {
        std::atomic<uint64_t> total_requests{0};
        std::atomic<uint64_t> successful_requests{0};
        std::atomic<uint64_t> failed_requests{0};
        std::atomic<uint64_t> retried_requests{0};
        std::atomic<uint64_t> total_latency{0};
    };
    Metrics metrics_;

    httplib::Client* getNextAvailableClient(std::vector<ClientInfo>& clients, 
                                          std::atomic<size_t>& current_idx) {
        size_t start_idx = current_idx.fetch_add(1) % POOL_SIZE;
        size_t current = start_idx;
        uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        // First try to find an available client
        do {
            bool expected = false;
            if (clients[current].in_use.compare_exchange_strong(expected, true)) {
                clients[current].last_used.store(current_time);
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
                clients[current].last_used.store(current_time);
                return clients[current].client.get();
            }
            current = (current + 1) % POOL_SIZE;
        } while (current != start_idx);
        
        return nullptr;
    }

    void releaseClient(std::vector<ClientInfo>& clients, httplib::Client* client, bool had_error = false) {
        for (auto& info : clients) {
            if (info.client.get() == client) {
                info.in_use.store(false);
                if (had_error) {
                    info.error_count++;
                } else {
                    info.error_count.store(0);
                }
                break;
            }
        }
    }

    // Helper function to set CPU affinity for a thread
    void setThreadAffinity(int cpu_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }

    // Helper function to convert JSON to protobuf messages
    hotelreservation::SearchRequest parseSearchRequest(const Json::Value& json) {
        hotelreservation::SearchRequest req;
        req.set_customer_name(json["customerName"].asString());
        req.set_in_date(json["inDate"].asString());
        req.set_out_date(json["outDate"].asString());
        req.set_lat(json["latitude"].asDouble());
        req.set_lon(json["longitude"].asDouble());
        req.set_locale(json["locale"].asString());
        return req;
    }

    hotelreservation::RecommendRequest parseRecommendRequest(const Json::Value& json) {
        hotelreservation::RecommendRequest req;
        req.set_lat(json["latitude"].asDouble());
        req.set_lon(json["longitude"].asDouble());
        req.set_require(json["require"].asString());
        req.set_locale(json["locale"].asString());
        return req;
    }

    hotelreservation::UserRequest parseUserRequest(const Json::Value& json) {
        hotelreservation::UserRequest req;
        req.set_username(json["username"].asString());
        req.set_password(json["password"].asString());
        return req;
    }

    hotelreservation::ReservationRequest parseReservationRequest(const Json::Value& json) {
        hotelreservation::ReservationRequest req;
        req.set_customer_name(json["customerName"].asString());
        req.set_hotel_id(json["hotelId"].asString());
        req.set_in_date(json["inDate"].asString());
        req.set_out_date(json["outDate"].asString());
        req.set_room_number(json["roomNumber"].asInt64());
        req.set_username(json["username"].asString());
        req.set_password(json["password"].asString());
        return req;
    }

    // Helper function to convert protobuf messages to JSON
    Json::Value searchResponseToJson(const hotelreservation::SearchResponse& resp) {
        Json::Value json(Json::arrayValue);
        for (const auto& hotel : resp.hotels()) {
            Json::Value hotel_json;
            hotel_json["id"] = hotel.id();
            hotel_json["name"] = hotel.name();
            hotel_json["phoneNumber"] = hotel.phone_number();
            hotel_json["description"] = hotel.description();
            
            Json::Value address;
            address["streetNumber"] = hotel.address().street_number();
            address["streetName"] = hotel.address().street_name();
            address["city"] = hotel.address().city();
            address["state"] = hotel.address().state();
            address["country"] = hotel.address().country();
            address["postalCode"] = hotel.address().postal_code();
            address["latitude"] = hotel.address().lat();
            address["longitude"] = hotel.address().lon();
            
            hotel_json["address"] = address;
            json.append(hotel_json);
        }
        return json;
    }

    Json::Value recommendResponseToJson(const hotelreservation::RecommendResponse& resp) {
        Json::Value json(Json::arrayValue);
        for (const auto& hotel : resp.hotels()) {
            Json::Value hotel_json;
            hotel_json["id"] = hotel.id();
            hotel_json["name"] = hotel.name();
            hotel_json["phoneNumber"] = hotel.phone_number();
            hotel_json["description"] = hotel.description();
            
            Json::Value address;
            address["streetNumber"] = hotel.address().street_number();
            address["streetName"] = hotel.address().street_name();
            address["city"] = hotel.address().city();
            address["state"] = hotel.address().state();
            address["country"] = hotel.address().country();
            address["postalCode"] = hotel.address().postal_code();
            address["latitude"] = hotel.address().lat();
            address["longitude"] = hotel.address().lon();
            
            hotel_json["address"] = address;
            json.append(hotel_json);
        }
        return json;
    }

    void monitorRequests() {
        uint64_t last_logged_count = 0;
        while (monitoring_active_.load()) {
            uint64_t current_count = search_requests_received_.load();
            if (current_count >= last_logged_count + 1000) {
                std::cout << "Search requests - Received: " << current_count 
                         << ", Completed: " << search_requests_completed_.load()
                         << ", Failed: " << search_requests_failed_.load()
                         << ", Retried: " << search_requests_retried_.load() << std::endl;
                last_logged_count = current_count;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // Retry logic for failed requests
    template<typename T>
    std::string retryRequest(std::function<std::string()> request_func, 
                           std::vector<ClientInfo>& clients,
                           httplib::Client* client) {
        for (int retry = 0; retry < MAX_RETRIES; retry++) {
            try {
                std::string result = request_func();
                if (result.find("\"error\"") == std::string::npos) {
                    return result;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            } catch (const std::exception& e) {
                std::cerr << "Request failed on retry " << retry + 1 << ": " << e.what() << std::endl;
                if (retry == MAX_RETRIES - 1) {
                    releaseClient(clients, client, true);
                    throw;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            }
        }
        releaseClient(clients, client, true);
        return "{\"error\": \"Max retries exceeded\"}";
    }

public:
    FrontEndService() {
        // Initialize client pools with CPU affinity
        for (int i = 0; i < POOL_SIZE; i++) {
            search_clients_.push_back(ClientInfo(std::make_unique<httplib::Client>("search", 50051)));
            recommend_clients_.push_back(ClientInfo(std::make_unique<httplib::Client>("recommendation", 50053)));
            user_clients_.push_back(ClientInfo(std::make_unique<httplib::Client>("user", 50054)));
            reservation_clients_.push_back(ClientInfo(std::make_unique<httplib::Client>("reservation", 50055)));
        }
        
        // Start monitoring thread
        monitoring_thread_ = std::thread(&FrontEndService::monitorRequests, this);
    }

    ~FrontEndService() {
        monitoring_active_.store(false);
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }

    std::string HandleSearch(const std::string& json_str) {
        search_requests_received_++;
        metrics_.total_requests++;
        auto start_time = std::chrono::steady_clock::now();
        
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            metrics_.failed_requests++;
            return "{\"error\": \"Invalid JSON format\"}";
        }

        auto search_req = parseSearchRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(search_req);
        
        auto* client = getNextAvailableClient(search_clients_, current_search_idx_);
        if (!client) {
            metrics_.failed_requests++;
            return "{\"error\": \"No available clients\"}";
        }

        try {
            auto result = client->Post("/search", serialized_request, "application/x-protobuf");
            releaseClient(search_clients_, client);
            
            if (!result || result->status != 200) {
                metrics_.failed_requests++;
                return "{\"error\": \"Search service unavailable\"}";
            }

            hotelreservation::SearchResponse response;
            if (!microservice::utils::deserialize_message(result->body, response)) {
                metrics_.failed_requests++;
                return "{\"error\": \"Failed to process search results\"}";
            }

            Json::Value response_json = searchResponseToJson(response);
            search_requests_completed_++;
            metrics_.successful_requests++;
            metrics_.total_latency += std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            
            Json::FastWriter writer;
            return writer.write(response_json);
        } catch (const std::exception& e) {
            metrics_.failed_requests++;
            releaseClient(search_clients_, client, true);
            return "{\"error\": \"Search request failed: " + std::string(e.what()) + "\"}";
        }
    }

    std::string HandleRecommend(const std::string& json_str) {
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            return "{\"error\": \"Invalid JSON format\"}";
        }

        auto req = parseRecommendRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(req);
        
        auto* client = getNextAvailableClient(recommend_clients_, current_recommend_idx_);
        if (!client) {
            return "{\"error\": \"No available clients\"}";
        }

        auto result = client->Post("/recommend", serialized_request, "application/x-protobuf");
        releaseClient(recommend_clients_, client);
        
        if (!result || result->status != 200) {
            return "{\"error\": \"Recommendation service unavailable\"}";
        }

        hotelreservation::RecommendResponse response;
        if (!microservice::utils::deserialize_message(result->body, response)) {
            return "{\"error\": \"Failed to process recommendations\"}";
        }

        return recommendResponseToJson(response).toStyledString();
    }

    std::string HandleUser(const std::string& json_str) {
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            return "{\"error\": \"Invalid JSON format\"}";
        }

        auto req = parseUserRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(req);
        
        auto* client = getNextAvailableClient(user_clients_, current_user_idx_);
        if (!client) {
            return "{\"error\": \"No available clients\"}";
        }

        auto result = client->Post("/user", serialized_request, "application/x-protobuf");
        releaseClient(user_clients_, client);
        
        if (!result || result->status != 200) {
            return "{\"error\": \"User service unavailable\"}";
        }

        hotelreservation::UserResponse response;
        if (!microservice::utils::deserialize_message(result->body, response)) {
            return "{\"error\": \"Failed to process user request\"}";
        }

        Json::Value response_json;
        response_json["message"] = response.message();
        return response_json.toStyledString();
    }

    std::string HandleReservation(const std::string& json_str) {
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            return "{\"error\": \"Invalid JSON format\"}";
        }

        auto req = parseReservationRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(req);
        
        auto* client = getNextAvailableClient(reservation_clients_, current_reservation_idx_);
        if (!client) {
            return "{\"error\": \"No available clients\"}";
        }

        auto result = client->Post("/reservation", serialized_request, "application/x-protobuf");
        releaseClient(reservation_clients_, client);
        
        if (!result || result->status != 200) {
            return "{\"error\": \"Reservation service unavailable\"}";
        }

        hotelreservation::ReservationResponse response;
        if (!microservice::utils::deserialize_message(result->body, response)) {
            return "{\"error\": \"Failed to process reservation\"}";
        }

        Json::Value response_json;
        response_json["message"] = response.message();
        return response_json.toStyledString();
    }
};

int main() {
    httplib::Server svr;
    FrontEndService service;

    // Set up multi-threading options with CPU affinity
    svr.new_task_queue = [] { 
        auto pool = new httplib::ThreadPool(256);
        // Set CPU affinity for each thread in the pool
        for (int i = 0; i < 256; i++) {
            pool->enqueue([i]() {
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET(i % 256, &cpuset);  // Distribute threads across all cores
                pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
            });
        }
        return pool;
    };

    // Configure server settings
    svr.set_keep_alive_max_count(20000);
    svr.set_read_timeout(5);
    svr.set_write_timeout(5);
    svr.set_idle_interval(0, 100000);

    svr.Get("/search", [&](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();
        
        auto check_timeout = [&start_time]() -> bool {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            return elapsed > 100; // 100ms timeout
        };
        std::cout << "Search request received" << std::endl;

        if (check_timeout()) {
            std::cerr << "Search request timeout - Request took too long to process" << std::endl;
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout\"}", "application/json");
            return;
        }

        try {
            Json::Value json;
            json["customerName"] = req.get_param_value("customerName");
            json["inDate"] = req.get_param_value("inDate");
            json["outDate"] = req.get_param_value("outDate");
            json["latitude"] = std::stod(req.get_param_value("latitude"));
            json["longitude"] = std::stod(req.get_param_value("longitude"));
            json["locale"] = req.get_param_value("locale");    

            if (check_timeout()) {
                std::cerr << "Search request timeout during parameter processing" << std::endl;
                res.status = 408;
                res.set_content("{\"error\": \"Request timeout during processing\"}", "application/json");
                return;
            }

            Json::FastWriter writer;
            res.set_content(service.HandleSearch(writer.write(json)), "application/json");
        } catch (const std::exception& e) {
            std::cerr << "Search request error: " << e.what() << std::endl;
            res.status = 400;
            res.set_content("{\"error\": \"Invalid request parameters\"}", "application/json");
        }
    });

    svr.Get("/recommend", [&](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();
        
        auto check_timeout = [&start_time]() -> bool {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            return elapsed > 100;
        };

        if (check_timeout()) {
            std::cerr << "Recommend request timeout - Request took too long to process" << std::endl;
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout\"}", "application/json");
            return;
        }

        try {
            Json::Value json;
            json["latitude"] = std::stod(req.get_param_value("latitude"));
            json["longitude"] = std::stod(req.get_param_value("longitude"));
            json["require"] = req.get_param_value("require");
            json["locale"] = req.get_param_value("locale");

            if (check_timeout()) {
                std::cerr << "Recommend request timeout during parameter processing" << std::endl;
                res.status = 408;
                res.set_content("{\"error\": \"Request timeout during processing\"}", "application/json");
                return;
            }
            
            Json::FastWriter writer;
            res.set_content(service.HandleRecommend(writer.write(json)), "application/json");
        } catch (const std::exception& e) {
            std::cerr << "Recommend request error: " << e.what() << std::endl;
            res.status = 400;
            res.set_content("{\"error\": \"Invalid request parameters\"}", "application/json");
        }
    });

    svr.Get("/user", [&](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();
        
        auto check_timeout = [&start_time]() -> bool {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            return elapsed > 100;
        };

        if (check_timeout()) {
            std::cerr << "User request timeout - Request took too long to process" << std::endl;
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout\"}", "application/json");
            return;
        }

        try {
            Json::Value json;
            json["username"] = req.get_param_value("username");
            json["password"] = req.get_param_value("password");

            if (check_timeout()) {
                std::cerr << "User request timeout during parameter processing" << std::endl;
                res.status = 408;
                res.set_content("{\"error\": \"Request timeout during processing\"}", "application/json");
                return;
            }
            
            Json::FastWriter writer;
            res.set_content(service.HandleUser(writer.write(json)), "application/json");
        } catch (const std::exception& e) {
            std::cerr << "User request error: " << e.what() << std::endl;
            res.status = 400;
            res.set_content("{\"error\": \"Invalid request parameters\"}", "application/json");
        }
    });

    svr.Get("/reservation", [&](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();
        
        auto check_timeout = [&start_time]() -> bool {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            return elapsed > 100;
        };

        if (check_timeout()) {
            std::cerr << "Reservation request timeout - Request took too long to process" << std::endl;
            res.status = 408;
            res.set_content("{\"error\": \"Request timeout\"}", "application/json");
            return;
        }

        try {
            Json::Value json;
            json["customerName"] = req.get_param_value("customerName");
            json["hotelId"] = req.get_param_value("hotelId");
            json["inDate"] = req.get_param_value("inDate");
            json["outDate"] = req.get_param_value("outDate");
            json["roomNumber"] = std::stoi(req.get_param_value("roomNumber"));
            json["username"] = req.get_param_value("username");
            json["password"] = req.get_param_value("password");

            if (check_timeout()) {
                std::cerr << "Reservation request timeout during parameter processing" << std::endl;
                res.status = 408;
                res.set_content("{\"error\": \"Request timeout during processing\"}", "application/json");
                return;
            }
            
            Json::FastWriter writer;
            res.set_content(service.HandleReservation(writer.write(json)), "application/json");
        } catch (const std::exception& e) {
            std::cerr << "Reservation request error: " << e.what() << std::endl;
            res.status = 400;
            res.set_content("{\"error\": \"Invalid request parameters\"}", "application/json");
        }
    });

    std::cout << "Frontend service listening on 0.0.0.0:50050 with 256 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50050);

    return 0;
}