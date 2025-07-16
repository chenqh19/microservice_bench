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
#include "../thread_pool.h"

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
        address1->set_padding(microservice::utils::generate_padding());
        profile1.set_padding(microservice::utils::generate_padding());
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
        address2->set_padding(microservice::utils::generate_padding());
        profile2.set_padding(microservice::utils::generate_padding());
        profiles_[profile2.id()] = profile2;

        // Hotel 3
        hotelreservation::HotelProfile profile3;
        profile3.set_id("3");
        profile3.set_name("Hotel Zetta San Francisco");
        profile3.set_phone_number("(415) 543-8555");
        profile3.set_description("A 2-minute walk from the Powell Street cable car turnaround and BART station, this hip hotel in a 1913 building features high-tech amenities.");
        auto* address3 = profile3.mutable_address();
        address3->set_street_number("55");
        address3->set_street_name("5th St");
        address3->set_city("San Francisco");
        address3->set_state("CA");
        address3->set_country("United States");
        address3->set_postal_code("94103");
        address3->set_lat(37.7854);
        address3->set_lon(-122.4071);
        address3->set_padding(microservice::utils::generate_padding());
        profile3.set_padding(microservice::utils::generate_padding());
        profiles_[profile3.id()] = profile3;

        // Hotel 4
        hotelreservation::HotelProfile profile4;
        profile4.set_id("4");
        profile4.set_name("Hotel Vitale");
        profile4.set_phone_number("(415) 278-3700");
        profile4.set_description("A 5-minute walk from the Ferry Building and Embarcadero BART station, this boutique hotel offers bay views and a rooftop spa.");
        auto* address4 = profile4.mutable_address();
        address4->set_street_number("8");
        address4->set_street_name("Mission St");
        address4->set_city("San Francisco");
        address4->set_state("CA");
        address4->set_country("United States");
        address4->set_postal_code("94105");
        address4->set_lat(37.7936);
        address4->set_lon(-122.3930);
        address4->set_padding(microservice::utils::generate_padding());
        profile4.set_padding(microservice::utils::generate_padding());
        profiles_[profile4.id()] = profile4;

        // Hotel 5
        hotelreservation::HotelProfile profile5;
        profile5.set_id("5");
        profile5.set_name("Phoenix Hotel");
        profile5.set_phone_number("(415) 776-1380");
        profile5.set_description("A 10-minute walk from the Mission District, this retro-chic hotel features a heated outdoor pool and free bike rentals.");
        auto* address5 = profile5.mutable_address();
        address5->set_street_number("601");
        address5->set_street_name("Eddy St");
        address5->set_city("San Francisco");
        address5->set_state("CA");
        address5->set_country("United States");
        address5->set_postal_code("94109");
        address5->set_lat(37.7831);
        address5->set_lon(-122.4181);
        address5->set_padding(microservice::utils::generate_padding());
        profile5.set_padding(microservice::utils::generate_padding());
        profiles_[profile5.id()] = profile5;

        // Hotel 6
        hotelreservation::HotelProfile profile6;
        profile6.set_id("6");
        profile6.set_name("The St. Regis San Francisco");
        profile6.set_phone_number("(415) 284-4000");
        profile6.set_description("A 4-minute walk from the San Francisco Museum of Modern Art, this luxury hotel features a spa and 2 restaurants.");
        auto* address6 = profile6.mutable_address();
        address6->set_street_number("125");
        address6->set_street_name("3rd St");
        address6->set_city("San Francisco");
        address6->set_state("CA");
        address6->set_country("United States");
        address6->set_postal_code("94103");
        address6->set_lat(37.7863);
        address6->set_lon(-122.4015);
        address6->set_padding(microservice::utils::generate_padding());
        profile6.set_padding(microservice::utils::generate_padding());
        profiles_[profile6.id()] = profile6;

        // Add more hotels 7-80 with generated data
        for (int i = 7; i <= 80; i++) {
            hotelreservation::HotelProfile profile;
            profile.set_id(std::to_string(i));
            profile.set_name("Hotel " + std::to_string(i));
            profile.set_phone_number("(415) 555-" + std::to_string(1000 + i));
            profile.set_description("A modern hotel in San Francisco with excellent amenities and service.");
            auto* address = profile.mutable_address();
            address->set_street_number(std::to_string(100 + i));
            address->set_street_name("Main St");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("9410" + std::to_string(i % 10));
            address->set_lat(37.7835 + static_cast<double>(i)/500.0*3);
            address->set_lon(-122.41 + static_cast<double>(i)/500.0*4);
            address->set_padding(microservice::utils::generate_padding());
            profile.set_padding(microservice::utils::generate_padding());
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
        
        response.set_padding(microservice::utils::generate_padding());
        return response;
    }
};

void handle_client(int client_fd, ProfileService& service, Ser1de_re& ser1de) {
    char len_buf[4];
    ssize_t n = read(client_fd, len_buf, 4);
    if (n != 4) { close(client_fd); return; }
    uint32_t msg_len = 0;
    memcpy(&msg_len, len_buf, 4);
    std::vector<char> buf(msg_len);
    n = read(client_fd, buf.data(), msg_len);
    if (n != (ssize_t)msg_len) { close(client_fd); return; }
    hotelreservation::GetProfilesRequest req;
    bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), req);
    if (!ok) { close(client_fd); return; }
    auto response = service.GetProfiles(req);
    std::string resp_str = microservice::utils::serialize_message(ser1de, response);
    uint32_t resp_len = resp_str.size();
    write(client_fd, &resp_len, 4);
    write(client_fd, resp_str.data(), resp_len);
    close(client_fd);
}

int main() {
    const char* socket_path = "/tmp/profile_service.sock";
    unlink(socket_path); // Remove if exists
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    chmod(socket_path, 0777); // Ensure world-writable for Docker
    if (listen(server_fd, 1024) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }
    std::cout << "Profile service listening on unix://" << socket_path << std::endl;
    
    ProfileService service;
    Ser1de_re ser1de;
    ThreadPool pool(64); // Use 64 threads for the pool
    
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) continue;
        pool.enqueue_task([client_fd, &service]() {
            handle_client(client_fd, service, ser1de);
        });
    }
    close(server_fd);
    return 0;
} 