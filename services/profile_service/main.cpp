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
        pre_generated_random_data_.resize(2000);
        for (int i = 0; i < 2000; i++) {
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
        profile1.set_description("A storied icon just off Union Square, this Philippe Starck–designed hotel pairs whimsical furniture vignettes with moody, gallery-like corridors that invite lingering. Evenings drift toward the legendary Redwood Room for cocktails beneath towering redwood panels, while mornings begin with cable cars cresting the nearby hill. Powell Street BART is a short walk for easy links to SFO and the East Bay. Rotating art installations and pet-friendly accommodations make it a refined base for shoppers, theatergoers, and design lovers alike.");
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
        profile2.set_description("Steps from Yerba Buena Gardens and the Moscone Center, this vibrant address blends bold color, kinetic light installations, and a soundtrack that moves from lobby lounge to skyline views. Creative travelers split time between SFMOMA, boutique coffee on 3rd Street, and late-night ramen around the corner. Powell and Montgomery BART stations are both walkable, putting the airport and East Bay within easy reach. Rooms are sleek and functional, with pet-amenities available on request.");
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
        profile3.set_description("In SoMa’s creative corridor, playful public spaces and tech-art flourishes set a lively tone: a game room for friendly battles, co-working nooks for quick sprints, and espresso always within arm’s reach. SFMOMA and the Metreon are minutes away, with Powell Street BART and the Market Street lines connecting the rest of the city. Rooms balance industrial textures with soft lighting for a calm reset. Dog beds and bowls are available; morning walks to Yerba Buena Gardens are a ritual.");
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
        profile4.set_description("Facing the bay along the Embarcadero, this boutique retreat is known for its tranquil rooftop spa—sun-warmed decks, soaking tubs, and glimpses of ferries slipping past the piers. Guests stroll to the Ferry Building for oysters and weekend markets, or jog the palm-lined promenade at sunrise. Embarcadero Muni and BART sit nearby for seamless transit. Rooms lean into coastal calm with airy palettes; a limited number of pet-friendly rooms open toward the waterfront.");
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
        profile5.set_description("Perched on Nob Hill, this classic retreat pairs discreet service with hushed corridors and sweeping skyline views. Cable cars glide by the front steps while Grace Cathedral and Huntington Park anchor the neighborhood’s elegant pace. Inside, club-level extras, a serene courtyard, and white-linen dining set the tone. Smaller dogs are warmly welcomed; concierges can map leafy morning strolls and arrange sedan service for evenings downtown.");
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
        profile6.set_description("A modern SoMa landmark where contemporary art, polished service, and an urban spa converge, steps from major museums and gallery openings. Rooms layer rich textiles over clean architecture, with deep soaking tubs for unwinding after conference days at Moscone. Muni and BART are close, and the concierge can point you to chef-driven tasting menus within a few blocks. Pet kits and quiet floors make city stays comfortable for four-legged companions.");
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
        profile7.set_description("Crowning Nob Hill with postcard views in every direction, this historic grande dame blends gilded ballrooms and marble staircases with lively lounges. The cable car stops at the corner, whisking you to Union Square or Fisherman’s Wharf. By day, visit bookstores in nearby Chinatown; by night, toast beneath twinkling chandeliers as fog curls over the bay. Pet-friendly rooms are available, and the concierge offers tailored routes to Washington Square Park.");
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
        profile8.set_description("A Beaux-Arts landmark in the Financial District, famed for its glass-domed Garden Court where brunch unfolds beneath a canopy of light. Montgomery BART sits a few blocks away, connecting quickly to SFO, Oakland, and Berkeley. Rooms are gracious and quiet for downtown, while the on-site bar channels old San Francisco with polished wood and hushed conversation. Pets are welcome; leafy pocket parks and the Embarcadero are an easy walk for morning outings.");
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

        // Hotel 9
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("9");
            profile.set_name("Union Square Boutique Hotel");
            profile.set_phone_number("(415) 555-19");
            profile.set_description(
                "Tucked half a block from Union Square, this intimate boutique stay blends classic Beaux-Arts character with "
                "contemporary art installations and a quietly attentive staff. Guests step out to flagship shopping, cable cars, "
                "and pre-theater dining, while a short stroll connects to Powell Street BART for cross-bay and airport access. "
                "Rooms feature warm wood tones and bay window seating overlooking the square’s palm-lined plaza. "
                "Pet-friendly floors include in-room beds and bowls, and a small outdoor relief area is accessible via a side alley. "
                "Within 10 minutes on foot: the Contemporary Jewish Museum, the Westfield Dome, and the Dragon Gate to Chinatown.");
            auto* address = profile.mutable_address();
            address->set_street_number("350");
            address->set_street_name("Powell St");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94102");
            address->set_lat(37.7881);
            address->set_lon(-122.4085);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }

        // Hotel 10
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("10");
            profile.set_name("SoMa Loft Hotel");
            profile.set_phone_number("(415) 555-110");
            profile.set_description(
                "Set in a converted warehouse near Yerba Buena Gardens, this loft-style hotel offers high ceilings, "
                "polished concrete floors, and floor-to-ceiling windows that frame SoMa’s skyline. The lobby espresso bar "
                "pours third-wave roasts for early museum runs—SFMOMA and the Museum of the African Diaspora are minutes away. "
                "Howard Street places you two blocks from the Moscone Center and a 10-minute walk from Powell or Montgomery BART. "
                "Well-behaved pets are welcome; the front desk provides biodegradable bags and a dog-walking map to nearby parks.");
            auto* address = profile.mutable_address();
            address->set_street_number("888");
            address->set_street_name("Howard St");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94103");
            address->set_lat(37.7836);
            address->set_lon(-122.4039);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }

        // Hotel 11
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("11");
            profile.set_name("Embarcadero Bayview Hotel");
            profile.set_phone_number("(415) 555-111");
            profile.set_description(
                "Overlooking the Ferry Building and the Bay, this calm retreat pairs coastal palettes with light woods and "
                "floor-length sheers that glow at sunrise. Guests can stroll the Embarcadero promenade to weekend farmers’ "
                "markets, sip oysters on crushed ice, or hop Muni to Oracle Park. Montgomery Street BART and Embarcadero "
                "Station are both a short walk for easy links to SFO and the East Bay. Pet-friendly rooms are limited but "
                "well-appointed, and a seaside jogging route begins right outside the lobby.");
            auto* address = profile.mutable_address();
            address->set_street_number("4");
            address->set_street_name("Embarcadero Center");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94111");
            address->set_lat(37.7957);
            address->set_lon(-122.3960);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }

        // Hotel 12
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("12");
            profile.set_name("Nob Hill Heritage Hotel");
            profile.set_phone_number("(415) 555-112");
            profile.set_description(
                "Crowned atop Nob Hill, this landmark-inspired property channels old San Francisco with marble-clad corridors, "
                "gilded mirrors, and evening piano in the parlor. Cable cars crest the corner; Grace Cathedral and Huntington Park "
                "are just steps away. Descend California Street to the Financial District or wander north to North Beach cafes. "
                "Montgomery BART is reachable via a cable-car and 10-minute walk. Pets up to 40 lbs are permitted; the concierge "
                "stocks blankets for chilly foggy nights.");
            auto* address = profile.mutable_address();
            address->set_street_number("1001");
            address->set_street_name("California St");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94108");
            address->set_lat(37.7929);
            address->set_lon(-122.4109);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }

        // Hotel 13
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("13");
            profile.set_name("Ferry Building Quarters");
            profile.set_phone_number("(415) 555-113");
            profile.set_description(
                "Anchored at the edge of the bay, this airy stay brings the Ferry Building’s buzz to your doorstep: "
                "artisan cheesemongers, fresh bread still warm from the oven, and stalls of citrus and local honey on weekends. "
                "The Embarcadero waterfront trail unfurls in both directions, while Embarcadero Station provides fast transit links. "
                "Rooms emphasize natural textures and quiet insulation from the city hum. Pet-friendly rooms include a welcome "
                "treat and easy access to waterside green space.");
            auto* address = profile.mutable_address();
            address->set_street_number("1");
            address->set_street_name("Ferry Building");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94111");
            address->set_lat(37.7955);
            address->set_lon(-122.3937);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }

        // Hotel 14
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("14");
            profile.set_name("Yerba Buena Garden Suites");
            profile.set_phone_number("(415) 555-114");
            profile.set_description(
                "Facing the lawns and sculpture gardens of Yerba Buena, this all-suites hotel offers separate living areas for "
                "work and rest, plus a rooftop terrace for golden-hour skyline views. SFMOMA lies around the corner, and Moscone "
                "Center is a five-minute walk for conferences. Montgomery BART and multiple Muni lines simplify crosstown trips. "
                "Pets are welcomed with a small nightly fee; the concierge can recommend groomers and nearby pet boutiques.");
            auto* address = profile.mutable_address();
            address->set_street_number("750");
            address->set_street_name("Folsom St");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94107");
            address->set_lat(37.7844);
            address->set_lon(-122.4013);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }

        // Hotel 15
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("15");
            profile.set_name("Civic Center Grand");
            profile.set_phone_number("(415) 555-115");
            profile.set_description(
                "Steps from the Asian Art Museum and the gilded Beaux-Arts dome of City Hall, this grand property mixes "
                "period detail with modern comforts. Light rail lines and bus routes converge at Civic Center for quick hops "
                "to Hayes Valley dining, the Castro, and the Mission. Rooms facing east catch the morning light; suites "
                "overlook the plaza’s sycamores. Pet policy is relaxed; larger breeds are considered with advance notice.");
            auto* address = profile.mutable_address();
            address->set_street_number("200");
            address->set_street_name("Larkin St");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94102");
            address->set_lat(37.7803);
            address->set_lon(-122.4165);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }

        // Hotel 16
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("16");
            profile.set_name("North Beach Art House");
            profile.set_phone_number("(415) 555-116");
            profile.set_description(
                "In literary North Beach, this art-forward hideout sits near Beat-era landmarks, espresso bars, and red-sauce "
                "trattorias. By day, wander to Coit Tower’s murals; by night, slip into jazz clubs tucked along Columbus Avenue. "
                "The California Street cable car is a short walk away; from there it’s an easy transfer to BART. A limited number "
                "of pet-friendly rooms are available; Washington Square Park offers early-morning romps.");
            auto* address = profile.mutable_address();
            address->set_street_number("530");
            address->set_street_name("Columbus Ave");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94133");
            address->set_lat(37.8004);
            address->set_lon(-122.4099);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }

        // Hotel 17
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("17");
            profile.set_name("Mission Market Inn");
            profile.set_phone_number("(415) 555-117");
            profile.set_description(
                "Rooted in the Mission District, this colorful inn neighbors taquerias, vibrant murals, and the indie boutiques "
                "of Valencia Street. Weekend mornings begin with cold brew and a stroll through the Mission Community Market. "
                "The 24th Street BART station is nearby for trips downtown or to the East Bay. Pet-friendly rooms face an internal "
                "courtyard to reduce street noise; dog-friendly cafes abound within a few blocks.");
            auto* address = profile.mutable_address();
            address->set_street_number("2400");
            address->set_street_name("Mission St");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94110");
            address->set_lat(37.7595);
            address->set_lon(-122.4184);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }

        // Hotel 18
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("18");
            profile.set_name("Financial District Executive Hotel");
            profile.set_phone_number("(415) 555-118");
            profile.set_description(
                "Geared to business travelers, this polished address places you amid the skyscrapers of the Financial District, "
                "with quiet, well-lit workspaces in every room. Morning walks to the Ferry Building set the tone; evenings wind down "
                "in speakeasy-style lounges hidden in plain sight. Montgomery Street BART is five minutes away. Pets are permitted "
                "on designated floors; room-service includes a simple pet menu.");
            auto* address = profile.mutable_address();
            address->set_street_number("555");
            address->set_street_name("California St");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94104");
            address->set_lat(37.7910);
            address->set_lon(-122.4021);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }

        // Hotel 19
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("19");
            profile.set_name("Transbay Terminal Residences");
            profile.set_phone_number("(415) 555-119");
            profile.set_description(
                "Rising near the Salesforce Transit Center’s rooftop park, these sleek residences suit extended stays with "
                "kitchenettes, washer/dryers, and access to a leafy, elevated garden. The Transbay hub connects bus lines across "
                "the Bay Area, while Embarcadero BART is a short walk. Oracle Park lies just over the viaduct for ballgames. "
                "Pets are embraced—longer-stay packages include waived pet fees and monthly grooming partnerships.");
            auto* address = profile.mutable_address();
            address->set_street_number("201");
            address->set_street_name("Folsom St");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94105");
            address->set_lat(37.7892);
            address->set_lon(-122.3906);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }

        // Hotel 20
        {
            hotelreservation::HotelProfile profile;
            profile.set_id("20");
            profile.set_name("South Park Courtyard Hotel");
            profile.set_phone_number("(415) 555-120");
            profile.set_description(
                "Encircling the leafy oval of South Park, this understated hotel balances startup energy with neighborhood calm. "
                "Mornings begin with pastries on outdoor benches beneath towering trees; afternoons end with a stroll to the ballpark "
                "or a gallery opening. The Caltrain terminal at 4th & King is within walking distance, with Muni links back to BART. "
                "Pet-friendly rooms open onto the courtyard for easy early walks; water bowls are set by the entrance on warm days.");
            auto* address = profile.mutable_address();
            address->set_street_number("68");
            address->set_street_name("South Park");
            address->set_city("San Francisco");
            address->set_state("CA");
            address->set_country("United States");
            address->set_postal_code("94107");
            address->set_lat(37.7816);
            address->set_lon(-122.3932);
            *address->mutable_padding() = microservice::utils::generate_person_padding();
            *profile.mutable_padding() = microservice::utils::generate_person_padding();
            profiles_[profile.id()] = profile;
        }
    }

    hotelreservation::GetProfilesResponse process_request(const hotelreservation::GetProfilesRequest& req) {
        std::lock_guard<std::mutex> lock(profiles_mutex_);

		// Optional dummy compression
#if ENABLE_DUMMY_SERVICE_COMPRESSION
		std::string compressed_random = microservice::compression::compress_data(pre_generated_random_data_);
		std::string decompressed_random = microservice::compression::decompress_data(compressed_random);
#endif

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
    const int NUM_WORKERS = 64;  // Number of worker processes
    
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