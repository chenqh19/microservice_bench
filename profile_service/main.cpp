#include <iostream>
#include <string>
#include <unordered_map>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include <httplib.h>

class ProfileService {
private:
    std::unordered_map<std::string, hotelreservation::HotelProfile> profiles_;

public:
    ProfileService() {
        // Initialize with some sample data
        InitializeSampleData();
    }

    void InitializeSampleData() {
        // Hotel 1
        hotelreservation::HotelProfile profile1;
        profile1.set_id("1");
        profile1.set_name("Clift Hotel");
        profile1.set_phone_number("(415) 775-4700");
        profile1.set_description("A 6-minute walk from Union Square and 4 minutes from a Muni Metro station, this luxury hotel designed by Philippe Starck features an artsy furniture collection in the lobby, including work by Salvador Dali.");
        auto* address1 = profile1.mutable_address();
        address1->set_street_number("495");
        address1->set_street_name("Geary St");
        address1->set_city("San Francisco");
        address1->set_state("CA");
        address1->set_country("United States");
        address1->set_postal_code("94102");
        address1->set_lat(37.7867);
        address1->set_lon(-122.4112);
        profiles_[profile1.id()] = profile1;

        // Hotel 2
        hotelreservation::HotelProfile profile2;
        profile2.set_id("2");
        profile2.set_name("W San Francisco");
        profile2.set_phone_number("(415) 777-5300");
        profile2.set_description("Less than a block from the Yerba Buena Center for the Arts, this trendy hotel is a 12-minute walk from Union Square.");
        auto* address2 = profile2.mutable_address();
        address2->set_street_number("181");
        address2->set_street_name("3rd St");
        address2->set_city("San Francisco");
        address2->set_state("CA");
        address2->set_country("United States");
        address2->set_postal_code("94103");
        address2->set_lat(37.7854);
        address2->set_lon(-122.4005);
        profiles_[profile2.id()] = profile2;

        // Hotel 3
        hotelreservation::HotelProfile profile3;
        profile3.set_id("3");
        profile3.set_name("Hotel Zetta");
        profile3.set_phone_number("(415) 543-8555");
        profile3.set_description("A 3-minute walk from the Powell Street cable-car turnaround and BART rail station, this hip hotel 9 minutes from Union Square combines high-tech lodging with artsy touches.");
        auto* address3 = profile3.mutable_address();
        address3->set_street_number("55");
        address3->set_street_name("5th St");
        address3->set_city("San Francisco");
        address3->set_state("CA");
        address3->set_country("United States");
        address3->set_postal_code("94103");
        address3->set_lat(37.7834);
        address3->set_lon(-122.4071);
        profiles_[profile3.id()] = profile3;

        // Hotel 4
        hotelreservation::HotelProfile profile4;
        profile4.set_id("4");
        profile4.set_name("Hotel Vitale");
        profile4.set_phone_number("(415) 278-3700");
        profile4.set_description("This waterfront hotel with Bay Bridge views is 3 blocks from the Financial District and a 4-minute walk from the Ferry Building.");
        auto* address4 = profile4.mutable_address();
        address4->set_street_number("8");
        address4->set_street_name("Mission St");
        address4->set_city("San Francisco");
        address4->set_state("CA");
        address4->set_country("United States");
        address4->set_postal_code("94105");
        address4->set_lat(37.7936);
        address4->set_lon(-122.3930);
        profiles_[profile4.id()] = profile4;

        // Hotel 5
        hotelreservation::HotelProfile profile5;
        profile5.set_id("5");
        profile5.set_name("Phoenix Hotel");
        profile5.set_phone_number("(415) 776-1380");
        profile5.set_description("Located in the Tenderloin neighborhood, a 10-minute walk from a BART rail station, this retro motor lodge has hosted many rock musicians and other celebrities since the 1950s. It's a 4-minute walk from the historic Great American Music Hall nightclub.");
        auto* address5 = profile5.mutable_address();
        address5->set_street_number("601");
        address5->set_street_name("Eddy St");
        address5->set_city("San Francisco");
        address5->set_state("CA");
        address5->set_country("United States");
        address5->set_postal_code("94109");
        address5->set_lat(37.7831);
        address5->set_lon(-122.4181);
        profiles_[profile5.id()] = profile5;

        // Hotel 6
        hotelreservation::HotelProfile profile6;
        profile6.set_id("6");
        profile6.set_name("St. Regis San Francisco");
        profile6.set_phone_number("(415) 284-4000");
        profile6.set_description("St. Regis Museum Tower is a 42-story, 484 ft skyscraper in the South of Market district of San Francisco, California, adjacent to Yerba Buena Gardens, Moscone Center, PacBell Building and the San Francisco Museum of Modern Art.");
        auto* address6 = profile6.mutable_address();
        address6->set_street_number("125");
        address6->set_street_name("3rd St");
        address6->set_city("San Francisco");
        address6->set_state("CA");
        address6->set_country("United States");
        address6->set_postal_code("94109");
        address6->set_lat(37.7863);
        address6->set_lon(-122.4015);
        profiles_[profile6.id()] = profile6;

        // Add hotels 7-80 with generated data
        for (int i = 7; i <= 80; i++) {
            hotelreservation::HotelProfile profile;
            profile.set_id(std::to_string(i));
            profile.set_name("St. Regis San Francisco");
            profile.set_phone_number("(415) 284-40" + std::to_string(i));
            profile.set_description("St. Regis Museum Tower is a 42-story, 484 ft skyscraper in the South of Market district of San Francisco, California, adjacent to Yerba Buena Gardens, Moscone Center, PacBell Building and the San Francisco Museum of Modern Art.");
            
            auto* address = profile.mutable_address();
            address->set_street_number("125");
            address->set_street_name("3rd St");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94109");
            address->set_lat(37.7835 + static_cast<double>(i)/500.0*3);
            address->set_lon(-122.41 + static_cast<double>(i)/500.0*4);
            
            profiles_[profile.id()] = profile;
        }
    }

    hotelreservation::GetProfilesResponse GetProfiles(const hotelreservation::GetProfilesRequest& req) {
        hotelreservation::GetProfilesResponse response;
        
        for (const auto& hotel_id : req.hotel_ids()) {
            auto it = profiles_.find(hotel_id);
            if (it != profiles_.end()) {
                *response.add_profiles() = it->second;
            }
        }
        
        return response;
    }
};

int main() {
    httplib::Server svr;
    ProfileService service;

    // Set up multi-threading options
    svr.new_task_queue = [] { return new httplib::ThreadPool(8); }; // Create thread pool with 8 threads

    svr.Post("/get_profiles", [&](const httplib::Request& req, httplib::Response& res) {
        hotelreservation::GetProfilesRequest request;
        if (microservice::utils::deserialize_message(req.body, request)) {
            auto response = service.GetProfiles(request);
            std::string serialized_response = microservice::utils::serialize_message(response);
            res.set_content(serialized_response, "application/x-protobuf");
        } else {
            res.status = 400;
        }
    });

    std::cout << "Profile service listening on 0.0.0.0:50052 with 8 worker threads" << std::endl;
    svr.listen("0.0.0.0", 50052);

    return 0;
} 