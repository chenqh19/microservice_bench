#include <iostream>
#include <string>
#include <json/json.h>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include "padding_utils.h"
#include <httplib.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
#include <sched.h>
#include <unordered_map>
#include <queue>
#include <condition_variable>
#include <iomanip>
#include <sstream>

// Helper function to get current timestamp as string
std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// Helper function to calculate duration in microseconds
template<typename T>
uint64_t getDurationUs(const T& start, const T& end) {
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

class FrontEndService {
public:
    // Define constants as static constexpr to ensure they're available at compile time
    static constexpr int POOL_SIZE = 4096;  // Increased pool size for higher concurrency
    static constexpr int MAX_CONCURRENT_CONNECTIONS = 2048;  // Limit concurrent connections
    static constexpr int MAX_RETRIES = 3;
    static constexpr int RETRY_DELAY_MS = 10;

private:
    std::atomic<uint64_t> search_requests_received_{0};
    std::atomic<uint64_t> search_requests_completed_{0};
    std::atomic<uint64_t> search_requests_failed_{0};
    std::atomic<uint64_t> search_requests_retried_{0};
    
    struct ClientInfo {
        std::unique_ptr<httplib::Client> client;
        std::atomic<bool> in_use;
        std::atomic<uint64_t> last_used;
        std::atomic<uint64_t> error_count;
        std::atomic<uint64_t> total_requests;
        std::atomic<uint64_t> successful_requests;

        ClientInfo() : client(nullptr), in_use(false), last_used(0), error_count(0), 
                      total_requests(0), successful_requests(0) {}
        
        ClientInfo(std::unique_ptr<httplib::Client>&& c) 
            : client(std::move(c)), in_use(false), last_used(0), error_count(0),
              total_requests(0), successful_requests(0) {}
        
        ClientInfo(const ClientInfo&) = delete;
        ClientInfo& operator=(const ClientInfo&) = delete;
        
        ClientInfo(ClientInfo&& other) noexcept
            : client(std::move(other.client)), in_use(false), last_used(0), error_count(0),
              total_requests(0), successful_requests(0) {}
        
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
    std::mutex connection_mutex_;
    std::condition_variable connection_cv_;

    // Metrics collection
    struct Metrics {
        std::atomic<uint64_t> total_requests{0};
        std::atomic<uint64_t> successful_requests{0};
        std::atomic<uint64_t> failed_requests{0};
        std::atomic<uint64_t> retried_requests{0};
        std::atomic<uint64_t> total_latency{0};
        std::atomic<uint64_t> active_connections{0};
        std::atomic<uint64_t> connection_timeouts{0};
        std::atomic<uint64_t> connection_errors{0};
    };
    Metrics metrics_;

    httplib::Client* getNextAvailableClient(std::vector<ClientInfo>& clients, 
                                          std::atomic<size_t>& current_idx) {
        size_t start_idx = current_idx.fetch_add(1) % POOL_SIZE;
        size_t current = start_idx;
        uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        // First try to find an available client without waiting
        do {
            bool expected = false;
            if (clients[current].in_use.compare_exchange_strong(expected, true)) {
                clients[current].last_used.store(current_time);
                metrics_.active_connections++;
                return clients[current].client.get();
            }
            current = (current + 1) % POOL_SIZE;
        } while (current != start_idx);
        
        // If no client is available, wait for one with a timeout
        std::unique_lock<std::mutex> lock(connection_mutex_);
        if (connection_cv_.wait_for(lock, std::chrono::milliseconds(50), 
            [this] { return metrics_.active_connections < MAX_CONCURRENT_CONNECTIONS; })) {
            // Try one more round after waiting
            current = start_idx;
            do {
                bool expected = false;
                if (clients[current].in_use.compare_exchange_strong(expected, true)) {
                    clients[current].last_used.store(current_time);
                    metrics_.active_connections++;
                    return clients[current].client.get();
                }
                current = (current + 1) % POOL_SIZE;
            } while (current != start_idx);
        }
        
        metrics_.connection_timeouts++;
        return nullptr;
    }

    void releaseClient(std::vector<ClientInfo>& clients, httplib::Client* client, bool had_error = false) {
        for (auto& info : clients) {
            if (info.client.get() == client) {
                info.in_use.store(false);
                metrics_.active_connections--;
                connection_cv_.notify_one();
                if (had_error) {
                    info.error_count++;
                    metrics_.connection_errors++;
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
        req.set_padding(microservice::utils::generate_padding());
        return req;
    }

    hotelreservation::RecommendRequest parseRecommendRequest(const Json::Value& json) {
        hotelreservation::RecommendRequest req;
        req.set_lat(json["latitude"].asDouble());
        req.set_lon(json["longitude"].asDouble());
        req.set_require(json["require"].asString());
        req.set_locale(json["locale"].asString());
        req.set_padding(microservice::utils::generate_padding());
        return req;
    }

    hotelreservation::UserRequest parseUserRequest(const Json::Value& json) {
        hotelreservation::UserRequest req;
        req.set_username(json["username"].asString());
        req.set_password(json["password"].asString());
        req.set_padding(microservice::utils::generate_padding());
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
        req.set_padding(microservice::utils::generate_padding());
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
    }

    ~FrontEndService() {
        // No cleanup needed
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
        auto method_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Method started" << std::endl;
        
        // JSON parsing timing
        auto json_parse_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Starting JSON parsing" << std::endl;
        
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            auto json_parse_end = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - JSON parsing failed in " 
                      << getDurationUs(json_parse_start, json_parse_end) << "μs" << std::endl;
            return "{\"error\": \"Invalid JSON format\"}";
        }
        
        auto json_parse_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - JSON parsing completed in " 
                  << getDurationUs(json_parse_start, json_parse_end) << "μs" << std::endl;

        // Protobuf request creation and serialization timing
        auto pb_serialize_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Starting protobuf serialization" << std::endl;
        
        auto req = parseUserRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(req);
        
        auto pb_serialize_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Protobuf serialization completed in " 
                  << getDurationUs(pb_serialize_start, pb_serialize_end) << "μs" << std::endl;
        
        // Client acquisition timing
        auto client_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Acquiring client" << std::endl;
        
        auto* client = getNextAvailableClient(user_clients_, current_user_idx_);
        if (!client) {
            auto client_end = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Client acquisition failed in " 
                      << getDurationUs(client_start, client_end) << "μs" << std::endl;
            return "{\"error\": \"No available clients\"}";
        }
        
        auto client_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Client acquired in " 
                  << getDurationUs(client_start, client_end) << "μs" << std::endl;

        // HTTP communication timing
        auto http_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Starting HTTP request to user service" << std::endl;
        
        auto result = client->Post("/user", serialized_request, "application/x-protobuf");
        releaseClient(user_clients_, client);
        
        auto http_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - HTTP request completed in " 
                  << getDurationUs(http_start, http_end) << "μs" << std::endl;
        
        if (!result || result->status != 200) {
            std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - HTTP request failed with status " 
                      << (result ? result->status : -1) << std::endl;
            return "{\"error\": \"User service unavailable\"}";
        }

        // Protobuf deserialization timing
        auto pb_deserialize_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Starting protobuf deserialization" << std::endl;
        
        hotelreservation::UserResponse response;
        if (!microservice::utils::deserialize_message(result->body, response)) {
            auto pb_deserialize_end = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Protobuf deserialization failed in " 
                      << getDurationUs(pb_deserialize_start, pb_deserialize_end) << "μs" << std::endl;
            return "{\"error\": \"Failed to process user request\"}";
        }
        
        auto pb_deserialize_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Protobuf deserialization completed in " 
                  << getDurationUs(pb_deserialize_start, pb_deserialize_end) << "μs" << std::endl;

        // JSON response creation timing
        auto json_response_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Creating JSON response" << std::endl;
        
        Json::Value response_json;
        response_json["message"] = response.message();
        std::string json_response = response_json.toStyledString();
        
        auto json_response_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - JSON response created in " 
                  << getDurationUs(json_response_start, json_response_end) << "μs" << std::endl;
        
        auto method_end = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] FrontEndService::HandleUser - Method completed in " 
                  << getDurationUs(method_start, method_end) << "μs total" << std::endl;
        
        return json_response;
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

// Define the static constexpr members outside the class
constexpr int FrontEndService::POOL_SIZE;
constexpr int FrontEndService::MAX_CONCURRENT_CONNECTIONS;
constexpr int FrontEndService::MAX_RETRIES;
constexpr int FrontEndService::RETRY_DELAY_MS;

int main() {
    httplib::Server svr;
    FrontEndService service;

    // Set up multi-threading options with CPU affinity
    svr.new_task_queue = [] { 
        auto pool = new httplib::ThreadPool(FrontEndService::POOL_SIZE);  // Use the static constexpr value
        // Set CPU affinity for each thread in the pool
        for (int i = 0; i < FrontEndService::POOL_SIZE; i++) {
            pool->enqueue([i]() {
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET(i % 256, &cpuset);  // Distribute threads across all cores
                pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
            });
        }
        return pool;
    };

    // Configure server settings for high performance
    svr.set_keep_alive_max_count(50000);  // Increased keep-alive connections
    svr.set_read_timeout(5);
    svr.set_write_timeout(5);
    svr.set_idle_interval(0, 100000);
    svr.set_payload_max_length(1024 * 1024);  // 1MB max payload

    svr.Get("/search", [&](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();
        
        auto check_timeout = [&start_time]() -> bool {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            return elapsed > 10; // 10ms timeout
        };

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
        auto http_start = std::chrono::steady_clock::now();
        std::cout << "[" << getTimestamp() << "] /user endpoint - HTTP request received" << std::endl;
        
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
            // Parameter extraction timing
            auto param_start = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] /user endpoint - Extracting parameters" << std::endl;
            
            Json::Value json;
            json["username"] = req.get_param_value("username");
            json["password"] = req.get_param_value("password");
            
            auto param_end = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] /user endpoint - Parameters extracted in " 
                      << getDurationUs(param_start, param_end) << "μs" << std::endl;

            if (check_timeout()) {
                std::cerr << "User request timeout during parameter processing" << std::endl;
                res.status = 408;
                res.set_content("{\"error\": \"Request timeout during processing\"}", "application/json");
                return;
            }
            
            // JSON creation and service call timing
            auto json_create_start = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] /user endpoint - Creating JSON and calling service" << std::endl;
            
            Json::FastWriter writer;
            std::string json_str = writer.write(json);
            std::string response = service.HandleUser(json_str);
            
            auto json_create_end = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] /user endpoint - JSON created and service called in " 
                      << getDurationUs(json_create_start, json_create_end) << "μs" << std::endl;
            
            res.set_content(response, "application/json");
            
            auto http_end = std::chrono::steady_clock::now();
            std::cout << "[" << getTimestamp() << "] /user endpoint - HTTP response sent in " 
                      << getDurationUs(http_start, http_end) << "μs total" << std::endl;
            
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

    std::cout << "Frontend service listening on 0.0.0.0:50050 with 4096 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50050);

    return 0;
}