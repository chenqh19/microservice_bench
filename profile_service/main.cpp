#include <iostream>
#include <string>
#include <unordered_map>
#include "hotel_reservation.pb.h"
#include "serialization_utils.h"
#include "padding_utils.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <vector>
#include <thread>
#include <cstring>
#include "../prefork_utils.h"

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
        auto pads = microservice::utils::generate_padding_fields();
        address1->set_padding1(pads[0]);
        address1->set_padding2(pads[1]);
        address1->set_padding3(pads[2]);
        address1->set_padding4(pads[3]);
        address1->set_padding5(pads[4]);
        address1->set_padding6(pads[5]);
        address1->set_padding7(pads[6]);
        address1->set_padding8(pads[7]);
        profile1.set_padding1(pads[0]);
        profile1.set_padding2(pads[1]);
        profile1.set_padding3(pads[2]);
        profile1.set_padding4(pads[3]);
        profile1.set_padding5(pads[4]);
        profile1.set_padding6(pads[5]);
        profile1.set_padding7(pads[6]);
        profile1.set_padding8(pads[7]);
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
        auto pads2 = microservice::utils::generate_padding_fields();
        address2->set_padding1(pads2[0]);
        address2->set_padding2(pads2[1]);
        address2->set_padding3(pads2[2]);
        address2->set_padding4(pads2[3]);
        address2->set_padding5(pads2[4]);
        address2->set_padding6(pads2[5]);
        address2->set_padding7(pads2[6]);
        address2->set_padding8(pads2[7]);
        profile2.set_padding1(pads2[0]);
        profile2.set_padding2(pads2[1]);
        profile2.set_padding3(pads2[2]);
        profile2.set_padding4(pads2[3]);
        profile2.set_padding5(pads2[4]);
        profile2.set_padding6(pads2[5]);
        profile2.set_padding7(pads2[6]);
        profile2.set_padding8(pads2[7]);
        profiles_[profile2.id()] = profile2;

        // Hotel 3
        hotelreservation::HotelProfile profile3;
        profile3.set_id("3");
        profile3.set_name("Hotel Zetta San Francisco");
        profile3.set_phone_number("(415) 543-8555");
        profile3.set_description("A 2-minute walk from the San Francisco Museum of Modern Art in SoMa, this hip hotel features a game room and a 24-hour fitness center.");
        auto* address3 = profile3.mutable_address();
        address3->set_street_number("55");
        address3->set_street_name("5th St");
        address3->set_city("San Francisco");
        address3->set_state("CA");
        address3->set_country("United States");
        address3->set_postal_code("94103");
        address3->set_lat(37.7854);
        address3->set_lon(-122.4071);
        auto pads3 = microservice::utils::generate_padding_fields();
        address3->set_padding1(pads3[0]);
        address3->set_padding2(pads3[1]);
        address3->set_padding3(pads3[2]);
        address3->set_padding4(pads3[3]);
        address3->set_padding5(pads3[4]);
        address3->set_padding6(pads3[5]);
        address3->set_padding7(pads3[6]);
        address3->set_padding8(pads3[7]);
        profile3.set_padding1(pads3[0]);
        profile3.set_padding2(pads3[1]);
        profile3.set_padding3(pads3[2]);
        profile3.set_padding4(pads3[3]);
        profile3.set_padding5(pads3[4]);
        profile3.set_padding6(pads3[5]);
        profile3.set_padding7(pads3[6]);
        profile3.set_padding8(pads3[7]);
        profiles_[profile3.id()] = profile3;

        // Hotel 4
        hotelreservation::HotelProfile profile4;
        profile4.set_id("4");
        profile4.set_name("Hotel Vitale");
        profile4.set_phone_number("(415) 278-3700");
        profile4.set_description("This boutique hotel with a rooftop spa is located in the Financial District, a 5-minute walk from the Ferry Building.");
        auto* address4 = profile4.mutable_address();
        address4->set_street_number("8");
        address4->set_street_name("Mission St");
        address4->set_city("San Francisco");
        address4->set_state("CA");
        address4->set_country("United States");
        address4->set_postal_code("94105");
        address4->set_lat(37.7936);
        address4->set_lon(-122.3930);
        auto pads4 = microservice::utils::generate_padding_fields();
        address4->set_padding1(pads4[0]);
        address4->set_padding2(pads4[1]);
        address4->set_padding3(pads4[2]);
        address4->set_padding4(pads4[3]);
        address4->set_padding5(pads4[4]);
        address4->set_padding6(pads4[5]);
        address4->set_padding7(pads4[6]);
        address4->set_padding8(pads4[7]);
        profile4.set_padding1(pads4[0]);
        profile4.set_padding2(pads4[1]);
        profile4.set_padding3(pads4[2]);
        profile4.set_padding4(pads4[3]);
        profile4.set_padding5(pads4[4]);
        profile4.set_padding6(pads4[5]);
        profile4.set_padding7(pads4[6]);
        profile4.set_padding8(pads4[7]);
        profiles_[profile4.id()] = profile4;

        // Hotel 5
        hotelreservation::HotelProfile profile5;
        profile5.set_id("5");
        profile5.set_name("Phoenix Hotel");
        profile5.set_phone_number("(415) 776-1380");
        profile5.set_description("This retro-chic hotel in the Tenderloin neighborhood features a heated outdoor pool and a restaurant serving California cuisine.");
        auto* address5 = profile5.mutable_address();
        address5->set_street_number("601");
        address5->set_street_name("Eddy St");
        address5->set_city("San Francisco");
        address5->set_state("CA");
        address5->set_country("United States");
        address5->set_postal_code("94109");
        address5->set_lat(37.7831);
        address5->set_lon(-122.4181);
        auto pads5 = microservice::utils::generate_padding_fields();
        address5->set_padding1(pads5[0]);
        address5->set_padding2(pads5[1]);
        address5->set_padding3(pads5[2]);
        address5->set_padding4(pads5[3]);
        address5->set_padding5(pads5[4]);
        address5->set_padding6(pads5[5]);
        address5->set_padding7(pads5[6]);
        address5->set_padding8(pads5[7]);
        profile5.set_padding1(pads5[0]);
        profile5.set_padding2(pads5[1]);
        profile5.set_padding3(pads5[2]);
        profile5.set_padding4(pads5[3]);
        profile5.set_padding5(pads5[4]);
        profile5.set_padding6(pads5[5]);
        profile5.set_padding7(pads5[6]);
        profile5.set_padding8(pads5[7]);
        profiles_[profile5.id()] = profile5;

        // Hotel 6
        hotelreservation::HotelProfile profile6;
        profile6.set_id("6");
        profile6.set_name("Hotel Nikko San Francisco");
        profile6.set_phone_number("(415) 394-1111");
        profile6.set_description("This upscale hotel in Nob Hill features a Japanese restaurant, a fitness center, and a heated indoor pool.");
        auto* address6 = profile6.mutable_address();
        address6->set_street_number("222");
        address6->set_street_name("Mason St");
        address6->set_city("San Francisco");
        address6->set_state("CA");
        address6->set_country("United States");
        address6->set_postal_code("94102");
        address6->set_lat(37.7863);
        address6->set_lon(-122.4015);
        auto pads6 = microservice::utils::generate_padding_fields();
        address6->set_padding1(pads6[0]);
        address6->set_padding2(pads6[1]);
        address6->set_padding3(pads6[2]);
        address6->set_padding4(pads6[3]);
        address6->set_padding5(pads6[4]);
        address6->set_padding6(pads6[5]);
        address6->set_padding7(pads6[6]);
        address6->set_padding8(pads6[7]);
        profile6.set_padding1(pads6[0]);
        profile6.set_padding2(pads6[1]);
        profile6.set_padding3(pads6[2]);
        profile6.set_padding4(pads6[3]);
        profile6.set_padding5(pads6[4]);
        profile6.set_padding6(pads6[5]);
        profile6.set_padding7(pads6[6]);
        profile6.set_padding8(pads6[7]);
        profiles_[profile6.id()] = profile6;

        // Add more hotels 7-80 with generated data
        for (int i = 7; i <= 80; i++) {
            hotelreservation::HotelProfile profile;
            profile.set_id(std::to_string(i));
            profile.set_name("Hotel " + std::to_string(i));
            profile.set_phone_number("(415) 555-" + std::to_string(1000 + i));
            profile.set_description("A comfortable hotel in San Francisco with modern amenities and excellent service.");
            
            auto* address = profile.mutable_address();
            address->set_street_number(std::to_string(100 + i));
            address->set_street_name("Main St");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94102");
            address->set_lat(37.7835 + static_cast<double>(i)/500.0*3);
            address->set_lon(-122.41 + static_cast<double>(i)/500.0*4);
            auto pads = microservice::utils::generate_padding_fields();
            address->set_padding1(pads[0]);
            address->set_padding2(pads[1]);
            address->set_padding3(pads[2]);
            address->set_padding4(pads[3]);
            address->set_padding5(pads[4]);
            address->set_padding6(pads[5]);
            address->set_padding7(pads[6]);
            address->set_padding8(pads[7]);
            
            profile.set_padding1(pads[0]);
            profile.set_padding2(pads[1]);
            profile.set_padding3(pads[2]);
            profile.set_padding4(pads[3]);
            profile.set_padding5(pads[4]);
            profile.set_padding6(pads[5]);
            profile.set_padding7(pads[6]);
            profile.set_padding8(pads[7]);
            profiles_[profile.id()] = profile;
        }
    }

    hotelreservation::GetProfilesResponse process_request(const hotelreservation::GetProfilesRequest& req) {
        hotelreservation::GetProfilesResponse response;
        
        for (const auto& hotel_id : req.hotel_ids()) {
            auto it = profiles_.find(hotel_id);
            if (it != profiles_.end()) {
                *response.add_profiles() = it->second;
            }
        }
        
        auto pads_response = microservice::utils::generate_padding_fields();
        response.set_padding1(pads_response[0]);
        response.set_padding2(pads_response[1]);
        response.set_padding3(pads_response[2]);
        response.set_padding4(pads_response[3]);
        response.set_padding5(pads_response[4]);
        response.set_padding6(pads_response[5]);
        response.set_padding7(pads_response[6]);
        response.set_padding8(pads_response[7]);
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/profile_service.sock";
    const int NUM_WORKERS = 8;  // Number of worker processes
    
    PreforkServer server(NUM_WORKERS);
    
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }
    
    std::cout << "Profile service socket setup complete" << std::endl;
    
    // Fork worker processes
    if (server.fork_workers()) {
        // This is a worker process
        ProfileService service;
        Ser1de_re ser1de;
        
        // Worker process main loop
        worker_loop<ProfileService, hotelreservation::GetProfilesRequest, hotelreservation::GetProfilesResponse>(
            server.get_server_fd(), service, ser1de);
        
        return 0;
    } else {
        // This is the master process
        std::cout << "Profile service master process started with " << NUM_WORKERS << " workers" << std::endl;
        server.master_loop();
        return 0;
    }
} 