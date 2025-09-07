#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include "hotel_reservation.pb.h"
#include "../utils/serialization_utils.h"
#include "../utils/padding_utils.h"
#include "../utils/compression_utils.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <vector>
#include <thread>
#include <cstring>
#include "../utils/prefork_utils.h"

class ProfileService {
private:
    std::unordered_map<std::string, hotelreservation::HotelProfile> profiles_;
    std::mutex profiles_mutex_;
    std::string pre_generated_random_data_;

public:
    ProfileService() {
        // Pre-generate random data once during initialization
        pre_generated_random_data_.resize(10000);
        for (int i = 0; i < 10000; i++) {
            pre_generated_random_data_[i] = 'A' + (i % 26);
        }
        // Initialize with some sample data
        InitializeSampleData();
        // Initialize compression manager
        microservice::compression::init_compression();
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
        *address1->mutable_padding() = microservice::utils::generate_person_padding();
        *profile1.mutable_padding() = microservice::utils::generate_person_padding();
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
        *address2->mutable_padding() = microservice::utils::generate_person_padding();
        *profile2.mutable_padding() = microservice::utils::generate_person_padding();
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
        *address3->mutable_padding() = microservice::utils::generate_person_padding();
        *profile3.mutable_padding() = microservice::utils::generate_person_padding();
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
        *address4->mutable_padding() = microservice::utils::generate_person_padding();
        *profile4.mutable_padding() = microservice::utils::generate_person_padding();
        profiles_[profile4.id()] = profile4;

        // Hotel 5
        hotelreservation::HotelProfile profile5;
        profile5.set_id("5");
        profile5.set_name("The Ritz-Carlton San Francisco");
        profile5.set_phone_number("(415) 296-7465");
        profile5.set_description("Located in Nob Hill, this luxury hotel offers stunning views of the city and bay, with elegant rooms and world-class service.");
        auto* address5 = profile5.mutable_address();
        address5->set_street_number("600");
        address5->set_street_name("Stockton St");
        address5->set_city("San Francisco");
        address5->set_state("CA");
        address5->set_country("United States");
        address5->set_postal_code("94108");
        address5->set_lat(37.7925);
        address5->set_lon(-122.4070);
        *address5->mutable_padding() = microservice::utils::generate_person_padding();
        *profile5.mutable_padding() = microservice::utils::generate_person_padding();
        profiles_[profile5.id()] = profile5;

        // Hotel 6
        hotelreservation::HotelProfile profile6;
        profile6.set_id("6");
        profile6.set_name("The St. Regis San Francisco");
        profile6.set_phone_number("(415) 284-4000");
        profile6.set_description("This 5-star hotel in SoMa features a spa, fine dining, and luxurious rooms with modern amenities.");
        auto* address6 = profile6.mutable_address();
        address6->set_street_number("125");
        address6->set_street_name("3rd St");
        address6->set_city("San Francisco");
        address6->set_state("CA");
        address6->set_country("United States");
        address6->set_postal_code("94103");
        address6->set_lat(37.7867);
        address6->set_lon(-122.4005);
        *address6->mutable_padding() = microservice::utils::generate_person_padding();
        *profile6.mutable_padding() = microservice::utils::generate_person_padding();
        profiles_[profile6.id()] = profile6;

        // Hotel 7
        hotelreservation::HotelProfile profile7;
        profile7.set_id("7");
        profile7.set_name("The Fairmont San Francisco");
        profile7.set_phone_number("(415) 772-5000");
        profile7.set_description("A historic luxury hotel atop Nob Hill, featuring classic architecture, elegant rooms, and panoramic city views.");
        auto* address7 = profile7.mutable_address();
        address7->set_street_number("950");
        address7->set_street_name("Mason St");
        address7->set_city("San Francisco");
        address7->set_state("CA");
        address7->set_country("United States");
        address7->set_postal_code("94108");
        address7->set_lat(37.7925);
        address7->set_lon(-122.4098);
        *address7->mutable_padding() = microservice::utils::generate_person_padding();
        *profile7.mutable_padding() = microservice::utils::generate_person_padding();
        profiles_[profile7.id()] = profile7;

        // Hotel 8
        hotelreservation::HotelProfile profile8;
        profile8.set_id("8");
        profile8.set_name("The Palace Hotel");
        profile8.set_phone_number("(415) 512-1111");
        profile8.set_description("A historic luxury hotel in the Financial District, featuring the famous Garden Court restaurant and elegant accommodations.");
        auto* address8 = profile8.mutable_address();
        address8->set_street_number("2");
        address8->set_street_name("New Montgomery St");
        address8->set_city("San Francisco");
        address8->set_state("CA");
        address8->set_country("United States");
        address8->set_postal_code("94105");
        address8->set_lat(37.7886);
        address8->set_lon(-122.4005);
        *address8->mutable_padding() = microservice::utils::generate_person_padding();
        *profile8.mutable_padding() = microservice::utils::generate_person_padding();
        profiles_[profile8.id()] = profile8;
    }

    hotelreservation::GetProfilesResponse process_request(const hotelreservation::GetProfilesRequest& req) {
        std::lock_guard<std::mutex> lock(profiles_mutex_);

        // Useless compression/decompression of random 5000B string
        std::string compressed_random = microservice::compression::compress_data(pre_generated_random_data_);
        std::string decompressed_random = microservice::compression::decompress_data(compressed_random);

        hotelreservation::GetProfilesResponse response;
        
        for (const auto& hotel_id : req.hotel_ids()) {
            auto it = profiles_.find(hotel_id);
            if (it != profiles_.end()) {
                auto profile = it->second;
                *response.add_profiles() = profile;
            }
        }
        
        *response.mutable_padding() = microservice::utils::generate_person_padding();
        
        return response;
    }
};

int main() {
    const char* socket_path = "/tmp/profile_service.sock";
    const int NUM_WORKERS = 32;  // Number of worker processes
    
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