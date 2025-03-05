#include <iostream>
#include <string>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>

class SearchService {
private:
    httplib::Client geo_client_;
    httplib::Client rate_client_;
    httplib::Client profile_client_;

public:
    SearchService() :
        geo_client_("geo", 50056),
        rate_client_("rate", 50057),
        profile_client_("profile", 50052) {}

    hotelreservation::SearchResponse Search(const hotelreservation::SearchRequest& req) {
        // First, get nearby hotels from geo service
        hotelreservation::NearbyRequest geo_req;
        geo_req.set_lat(req.lat());
        geo_req.set_lon(req.lon());
        
        auto geo_result = geo_client_.Post("/nearby", 
            microservice::utils::serialize_message(geo_req), 
            "application/x-protobuf");

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

        auto rate_result = rate_client_.Post("/get_rates", 
            microservice::utils::serialize_message(rate_req), 
            "application/x-protobuf");

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

        auto profile_result = profile_client_.Post("/get_profiles", 
            microservice::utils::serialize_message(profile_req), 
            "application/x-protobuf");

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
        
        return response;
    }
};

int main() {
    httplib::Server svr;
    SearchService service;

    // Set up multi-threading options
    svr.new_task_queue = [] { return new httplib::ThreadPool(100); }; // Create thread pool with 8 threads

    svr.Post("/search", [&](const httplib::Request& req, httplib::Response& res) {
        hotelreservation::SearchRequest request;
        if (microservice::utils::deserialize_message(req.body, request)) {
            auto response = service.Search(request);
            std::string serialized_response = microservice::utils::serialize_message(response);
            res.set_content(serialized_response, "application/x-protobuf");
        } else {
            res.status = 400;
        }
    });

    std::cout << "Search service listening on 0.0.0.0:50051 with 100 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50051);

    return 0;
} 