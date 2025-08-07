#include "../utils/compression_utils.h"
#include <iostream>
#include <string>
#include <json/json.h>
#include "hotel_reservation.pb.h"
#include "../utils/serialization_utils.h"
#include "../utils/padding_utils.h"
#include <httplib.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <vector>
#include <algorithm>

class FrontEndService {
public:
    // Define constants as static constexpr to ensure they're available at compile time
    static constexpr int POOL_SIZE = 128;  // Number of worker processes

    // Helper function to convert JSON to protobuf messages
    hotelreservation::SearchRequest parseSearchRequest(const Json::Value& json) {
        hotelreservation::SearchRequest req;
        req.set_customer_name(json["customerName"].asString());
        req.set_in_date(json["inDate"].asString());
        req.set_out_date(json["outDate"].asString());
        req.set_lat(json["latitude"].asDouble());
        req.set_lon(json["longitude"].asDouble());
        req.set_locale(json["locale"].asString());
        *req.mutable_padding() = microservice::utils::generate_person_padding();
        return req;
    }

    hotelreservation::RecommendRequest parseRecommendRequest(const Json::Value& json) {
        hotelreservation::RecommendRequest req;
        req.set_lat(json["latitude"].asDouble());
        req.set_lon(json["longitude"].asDouble());
        req.set_require(json["require"].asString());
        req.set_locale(json["locale"].asString());
        *req.mutable_padding() = microservice::utils::generate_person_padding();
        return req;
    }

    hotelreservation::UserRequest parseUserRequest(const Json::Value& json) {
        hotelreservation::UserRequest req;
        req.set_username(json["username"].asString());
        req.set_password(json["password"].asString());
        *req.mutable_padding() = microservice::utils::generate_person_padding();
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
        *req.mutable_padding() = microservice::utils::generate_person_padding();
        return req;
    }

    // Helper function to convert protobuf messages to JSON with compression
    Json::Value searchResponseToJson(const hotelreservation::SearchResponse& resp) {
        Json::Value json(Json::arrayValue);
        for (const auto& hotel : resp.hotels()) {
            Json::Value hotel_json;
            
            // Decompress hotel data received from search service
            std::string decompressed_id = microservice::compression::decompress_data(hotel.id());
            std::string decompressed_name = microservice::compression::decompress_data(hotel.name());
            std::string decompressed_phone = microservice::compression::decompress_data(hotel.phone_number());
            std::string decompressed_description = microservice::compression::decompress_data(hotel.description());
            
            // Use decompressed data for JSON response
            hotel_json["id"] = decompressed_id;
            hotel_json["name"] = decompressed_name;
            hotel_json["phoneNumber"] = decompressed_phone;
            hotel_json["description"] = decompressed_description;
            
            Json::Value address;
            std::string decompressed_street_number = microservice::compression::decompress_data(hotel.address().street_number());
            std::string decompressed_street_name = microservice::compression::decompress_data(hotel.address().street_name());
            std::string decompressed_city = microservice::compression::decompress_data(hotel.address().city());
            std::string decompressed_state = microservice::compression::decompress_data(hotel.address().state());
            std::string decompressed_country = microservice::compression::decompress_data(hotel.address().country());
            std::string decompressed_postal = microservice::compression::decompress_data(hotel.address().postal_code());
            
            address["streetNumber"] = decompressed_street_number;
            address["streetName"] = decompressed_street_name;
            address["city"] = decompressed_city;
            address["state"] = decompressed_state;
            address["country"] = decompressed_country;
            address["postalCode"] = decompressed_postal;
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
            
            // Decompress hotel data received from recommendation service
            std::string decompressed_id = microservice::compression::decompress_data(hotel.id());
            std::string decompressed_name = microservice::compression::decompress_data(hotel.name());
            std::string decompressed_phone = microservice::compression::decompress_data(hotel.phone_number());
            std::string decompressed_description = microservice::compression::decompress_data(hotel.description());
            
            // Use decompressed data for JSON response
            hotel_json["id"] = decompressed_id;
            hotel_json["name"] = decompressed_name;
            hotel_json["phoneNumber"] = decompressed_phone;
            hotel_json["description"] = decompressed_description;
            
            Json::Value address;
            std::string decompressed_street_number = microservice::compression::decompress_data(hotel.address().street_number());
            std::string decompressed_street_name = microservice::compression::decompress_data(hotel.address().street_name());
            std::string decompressed_city = microservice::compression::decompress_data(hotel.address().city());
            std::string decompressed_state = microservice::compression::decompress_data(hotel.address().state());
            std::string decompressed_country = microservice::compression::decompress_data(hotel.address().country());
            std::string decompressed_postal = microservice::compression::decompress_data(hotel.address().postal_code());
            
            address["streetNumber"] = decompressed_street_number;
            address["streetName"] = decompressed_street_name;
            address["city"] = decompressed_city;
            address["state"] = decompressed_state;
            address["country"] = decompressed_country;
            address["postalCode"] = decompressed_postal;
            address["latitude"] = hotel.address().lat();
            address["longitude"] = hotel.address().lon();
            
            hotel_json["address"] = address;
            json.append(hotel_json);
        }
        return json;
    }

    std::string sendProtobufOverUDS(const std::string& path, const std::string& data) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return "{\"error\": \"socket error\"}";
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(fd);
            return "{\"error\": \"connect error\"}";
        }
        uint32_t len = data.size();
        if (write(fd, &len, 4) != 4) { close(fd); return "{\"error\": \"write error\"}"; }
        if (write(fd, data.data(), len) != (ssize_t)len) { close(fd); return "{\"error\": \"write error\"}"; }
        char len_buf[4];
        if (read(fd, len_buf, 4) != 4) { close(fd); return "{\"error\": \"read error\"}"; }
        uint32_t resp_len = 0;
        memcpy(&resp_len, len_buf, 4);
        std::vector<char> resp_buf(resp_len);
        if (read(fd, resp_buf.data(), resp_len) != (ssize_t)resp_len) { close(fd); return "{\"error\": \"read error\"}"; }
        close(fd);
        return std::string(resp_buf.begin(), resp_buf.end());
    }

public:
    FrontEndService() {
        // Remove all initialization of httplib::Client in constructor
        // Initialize compression manager
        microservice::compression::init_compression();
    }

    ~FrontEndService() {
        // No cleanup needed
    }
    
    Ser1de_re ser1de;

    std::string HandleSearch(const std::string& json_str) {
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            return "{\"error\": \"Invalid JSON format\"}";
        }
        auto search_req = parseSearchRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(ser1de, search_req);
        std::string response_str = sendProtobufOverUDS("/tmp/search_service.sock", serialized_request);
        hotelreservation::SearchResponse response;
        if (!microservice::utils::deserialize_message(ser1de, response_str, response)) {
            return "{\"error\": \"Failed to process search results\"}";
        }
        Json::Value response_json = searchResponseToJson(response);
        Json::FastWriter writer;
        return writer.write(response_json);
    }

    std::string HandleRecommend(const std::string& json_str) {
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            return "{\"error\": \"Invalid JSON format\"}";
        }
        auto req = parseRecommendRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(ser1de, req);
        std::string response_str = sendProtobufOverUDS("/tmp/recommendation_service.sock", serialized_request);
        hotelreservation::RecommendResponse response;
        if (!microservice::utils::deserialize_message(ser1de, response_str, response)) {
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
        std::string serialized_request = microservice::utils::serialize_message(ser1de, req);
        std::string response_str = sendProtobufOverUDS("/tmp/user_service.sock", serialized_request);
        hotelreservation::UserResponse response;
        if (!microservice::utils::deserialize_message(ser1de, response_str, response)) {
            return "{\"error\": \"Failed to process user request\"}";
        }
        Json::Value response_json;
        // Decompress the message from user service
        std::string decompressed_message = microservice::compression::decompress_data(response.message());
        response_json["message"] = decompressed_message;
        return response_json.toStyledString();
    }

    std::string HandleReservation(const std::string& json_str) {
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            return "{\"error\": \"Invalid JSON format\"}";
        }
        auto req = parseReservationRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(ser1de, req);
        std::string response_str = sendProtobufOverUDS("/tmp/reservation_service.sock", serialized_request);
        hotelreservation::ReservationResponse response;
        if (!microservice::utils::deserialize_message(ser1de, response_str, response)) {
            return "{\"error\": \"Failed to process reservation\"}";
        }
        Json::Value response_json;
        // Decompress the message from reservation service
        std::string decompressed_message = microservice::compression::decompress_data(response.message());
        response_json["message"] = decompressed_message;
        return response_json.toStyledString();
    }
};

// Define the static constexpr members outside the class
constexpr int FrontEndService::POOL_SIZE;

class PreforkHTTPServer {
private:
    int num_workers_;
    std::vector<pid_t> worker_pids_;
    bool should_stop_;

public:
    PreforkHTTPServer(int num_workers = 32) : num_workers_(num_workers), should_stop_(false) {
        // Set up signal handlers for graceful shutdown
        signal(SIGTERM, [](int) { /* handled in main loop */ });
        signal(SIGINT, [](int) { /* handled in main loop */ });
    }

    // Fork worker processes
    bool fork_workers() {
        std::cout << "Starting " << num_workers_ << " HTTP worker processes..." << std::endl;
        
        for (int i = 0; i < num_workers_; ++i) {
            pid_t pid = fork();
            
            if (pid == 0) {
                // Worker process
                std::cout << "HTTP Worker process " << getpid() << " started" << std::endl;
                return true; // Return true to indicate this is a worker
            } else if (pid > 0) {
                // Master process
                worker_pids_.push_back(pid);
            } else {
                // Fork failed
                perror("fork");
                return false;
            }
        }

        // Master process continues here
        std::cout << "HTTP Master process " << getpid() << " started " << worker_pids_.size() << " workers" << std::endl;
        return false; // Return false to indicate this is the master
    }

    // Master process main loop
    void master_loop() {
        std::cout << "HTTP Master process waiting for workers..." << std::endl;
        
        while (!should_stop_) {
            // Check if any workers have died
            int status;
            pid_t dead_pid = waitpid(-1, &status, WNOHANG);
            
            if (dead_pid > 0) {
                std::cout << "HTTP Worker " << dead_pid << " died, restarting..." << std::endl;
                
                // Remove from our list
                worker_pids_.erase(
                    std::remove(worker_pids_.begin(), worker_pids_.end(), dead_pid),
                    worker_pids_.end()
                );
                
                // Fork a new worker
                pid_t new_pid = fork();
                if (new_pid == 0) {
                    // New worker process
                    std::cout << "Restarted HTTP worker process " << getpid() << std::endl;
                    return; // Return to indicate this is a worker
                } else if (new_pid > 0) {
                    // Master process
                    worker_pids_.push_back(new_pid);
                }
            }
            
            // Sleep a bit to avoid busy waiting
            usleep(100000); // 100ms
        }
        
        // Graceful shutdown
        std::cout << "HTTP Master process shutting down workers..." << std::endl;
        for (pid_t pid : worker_pids_) {
            kill(pid, SIGTERM);
        }
        
        // Wait for all workers to exit
        for (pid_t pid : worker_pids_) {
            waitpid(pid, nullptr, 0);
        }
    }

    // Set stop flag for graceful shutdown
    void stop() { should_stop_ = true; }
};

int main() {
    const int NUM_WORKERS = FrontEndService::POOL_SIZE;
    
    PreforkHTTPServer server(NUM_WORKERS);
    
    // Fork worker processes
    if (server.fork_workers()) {
        // This is a worker process
        FrontEndService service;
        
        // Set up HTTP server for this worker
        httplib::Server svr;
        
        // Configure server settings for high performance
        svr.set_keep_alive_max_count(50000);
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
                microservice::utils::log_request_timing("search", start_time, std::chrono::steady_clock::now());
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
                    microservice::utils::log_request_timing("search", start_time, std::chrono::steady_clock::now());
                    return;
                }

                Json::FastWriter writer;
                res.set_content(service.HandleSearch(writer.write(json)), "application/json");
                microservice::utils::log_request_timing("search", start_time, std::chrono::steady_clock::now());
            } catch (const std::exception& e) {
                std::cerr << "Search request error: " << e.what() << std::endl;
                res.status = 400;
                res.set_content("{\"error\": \"Invalid request parameters\"}", "application/json");
                microservice::utils::log_request_timing("search", start_time, std::chrono::steady_clock::now());
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
                microservice::utils::log_request_timing("recommend", start_time, std::chrono::steady_clock::now());
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
                    microservice::utils::log_request_timing("recommend", start_time, std::chrono::steady_clock::now());
                    return;
                }
                
                Json::FastWriter writer;
                res.set_content(service.HandleRecommend(writer.write(json)), "application/json");
                microservice::utils::log_request_timing("recommend", start_time, std::chrono::steady_clock::now());
            } catch (const std::exception& e) {
                std::cerr << "Recommend request error: " << e.what() << std::endl;
                res.status = 400;
                res.set_content("{\"error\": \"Invalid request parameters\"}", "application/json");
                microservice::utils::log_request_timing("recommend", start_time, std::chrono::steady_clock::now());
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
                microservice::utils::log_request_timing("user", start_time, std::chrono::steady_clock::now());
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
                    microservice::utils::log_request_timing("user", start_time, std::chrono::steady_clock::now());
                    return;
                }
                
                Json::FastWriter writer;
                res.set_content(service.HandleUser(writer.write(json)), "application/json");
                microservice::utils::log_request_timing("user", start_time, std::chrono::steady_clock::now());
            } catch (const std::exception& e) {
                std::cerr << "User request error: " << e.what() << std::endl;
                res.status = 400;
                res.set_content("{\"error\": \"Invalid request parameters\"}", "application/json");
                microservice::utils::log_request_timing("user", start_time, std::chrono::steady_clock::now());
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
                microservice::utils::log_request_timing("reservation", start_time, std::chrono::steady_clock::now());
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
                    microservice::utils::log_request_timing("reservation", start_time, std::chrono::steady_clock::now());
                    return;
                }
                
                Json::FastWriter writer;
                res.set_content(service.HandleReservation(writer.write(json)), "application/json");
                microservice::utils::log_request_timing("reservation", start_time, std::chrono::steady_clock::now());
            } catch (const std::exception& e) {
                std::cerr << "Reservation request error: " << e.what() << std::endl;
                res.status = 400;
                res.set_content("{\"error\": \"Invalid request parameters\"}", "application/json");
                microservice::utils::log_request_timing("reservation", start_time, std::chrono::steady_clock::now());
            }
        });

        std::cout << "HTTP Worker " << getpid() << " listening on 0.0.0.0:50050" << std::endl;
        svr.listen("0.0.0.0", 50050);
        
        return 0;
    } else {
        // This is the master process
        std::cout << "Frontend service master process started with " << NUM_WORKERS << " HTTP workers" << std::endl;
        server.master_loop();
        return 0;
    }
}