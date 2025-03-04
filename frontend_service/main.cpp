#include <iostream>
#include <string>
#include <json/json.h>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>
#include <thread>
#include <mutex>

class FrontEndService {
private:
    httplib::Client search_client_;
    httplib::Client profile_client_;
    httplib::Client recommend_client_;
    httplib::Client user_client_;
    httplib::Client reservation_client_;
    std::mutex clients_mutex_;  // Protect client access

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

public:
    FrontEndService() :
        search_client_("search", 50051),
        profile_client_("profile", 50052),
        recommend_client_("recommendation", 50053),
        user_client_("user", 50054),
        reservation_client_("reservation", 50055) {}

    std::string HandleSearch(const std::string& json_str) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            return "{\"error\": \"Invalid JSON format\"}";
        }

        auto search_req = parseSearchRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(search_req);
        auto result = search_client_.Post("/search", serialized_request, "application/x-protobuf");
        
        if (!result || result->status != 200) {
            return "{\"error\": \"Search service unavailable\"}";
        }

        hotelreservation::SearchResponse response;
        if (!microservice::utils::deserialize_message(result->body, response)) {
            return "{\"error\": \"Failed to process search results\"}";
        }

        Json::Value response_json = searchResponseToJson(response);
        Json::FastWriter writer;
        return writer.write(response_json);
    }

    std::string HandleRecommend(const std::string& json_str) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            return "{\"error\": \"Invalid JSON format\"}";
        }

        auto req = parseRecommendRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(req);
        auto result = recommend_client_.Post("/recommend", serialized_request, "application/x-protobuf");
        
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
        std::lock_guard<std::mutex> lock(clients_mutex_);
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            return "{\"error\": \"Invalid JSON format\"}";
        }

        auto req = parseUserRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(req);
        auto result = user_client_.Post("/user", serialized_request, "application/x-protobuf");
        
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
        std::lock_guard<std::mutex> lock(clients_mutex_);
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(json_str, json)) {
            return "{\"error\": \"Invalid JSON format\"}";
        }

        auto req = parseReservationRequest(json);
        std::string serialized_request = microservice::utils::serialize_message(req);
        auto result = reservation_client_.Post("/reservation", serialized_request, "application/x-protobuf");
        
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

    // Configure thread pool for the server
    auto num_threads = std::thread::hardware_concurrency();
    svr.new_task_queue = [num_threads]() { 
        return new httplib::ThreadPool(num_threads); 
    };

    // Configure server settings
    svr.set_keep_alive_max_count(20000);  // Maximum number of keep-alive requests
    svr.set_read_timeout(5);              // Read timeout in seconds
    svr.set_write_timeout(5);             // Write timeout in seconds
    svr.set_idle_interval(0, 100000);     // Idle interval in microseconds

    svr.Post("/search", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(service.HandleSearch(req.body), "application/json");
    });

    svr.Post("/recommend", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(service.HandleRecommend(req.body), "application/json");
    });

    svr.Post("/user", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(service.HandleUser(req.body), "application/json");
    });

    svr.Post("/reservation", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_content(service.HandleReservation(req.body), "application/json");
    });

    std::cout << "Frontend service listening on 0.0.0.0:50050 with " << num_threads << " threads" << std::endl;
    svr.listen("0.0.0.0", 50050);

    return 0;
}