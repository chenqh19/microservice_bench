#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include "hotel_reservation.pb.h"
#include "../utils/serialization_utils.h"
#include "../utils/padding_utils.h"
#include "../utils/compression_utils.h"
#include "../utils/data_models.h"
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
    std::unordered_map<std::string, microservice::models::HotelProfileData> profiles_;
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
        using microservice::models::HotelProfileData;
        using microservice::models::AddressData;
        profiles_.clear();

        auto add = [&](const std::string& id,
                       const std::string& name,
                       const std::string& phone,
                       const std::string& desc,
                       const AddressData& addr) {
            HotelProfileData p;
            p.id = id;
            p.name = name;
            p.phone_number = phone;
            p.description = desc;
            p.address = addr;
            profiles_[p.id] = p;
        };

        add("1","Clift Hotel","(415) 775-4700",
            "A storied icon just off Union Square, this Philippe Starck–designed hotel pairs whimsical furniture vignettes with moody, gallery-like corridors that invite lingering. Evenings drift toward the legendary Redwood Room for cocktails beneath towering redwood panels, while mornings begin with cable cars cresting the nearby hill. Powell Street BART is a short walk for easy links to SFO and the East Bay. Rotating art installations and pet-friendly accommodations make it a refined base for shoppers, theatergoers, and design lovers alike.",
            AddressData{"495","Geary St","San Francisco","CA","United States","94102",37.7867,-122.4112});

        add("2","W San Francisco","(415) 777-5300",
            "Steps from Yerba Buena Gardens and the Moscone Center, this vibrant address blends bold color, kinetic light installations, and a soundtrack that moves from lobby lounge to skyline views. Creative travelers split time between SFMOMA, boutique coffee on 3rd Street, and late-night ramen around the corner. Powell and Montgomery BART stations are both walkable, putting the airport and East Bay within easy reach. Rooms are sleek and functional, with pet-amenities available on request.",
            AddressData{"181","3rd St","San Francisco","CA","United States","94103",37.7854,-122.4005});

        add("3","Hotel Zetta San Francisco","(415) 543-8555",
            "In SoMa’s creative corridor, playful public spaces and tech-art flourishes set a lively tone: a game room for friendly battles, co-working nooks for quick sprints, and espresso always within arm’s reach. SFMOMA and the Metreon are minutes away, with Powell Street BART and the Market Street lines connecting the rest of the city. Rooms balance industrial textures with soft lighting for a calm reset. Dog beds and bowls are available; morning walks to Yerba Buena Gardens are a ritual.",
            AddressData{"55","5th St","San Francisco","CA","United States","94103",37.7854,-122.4071});

        add("4","Hotel Vitale","(415) 278-3700",
            "Facing the bay along the Embarcadero, this boutique retreat is known for its tranquil rooftop spa—sun-warmed decks, soaking tubs, and glimpses of ferries slipping past the piers. Guests stroll to the Ferry Building for oysters and weekend markets, or jog the palm-lined promenade at sunrise. Embarcadero Muni and BART sit nearby for seamless transit. Rooms lean into coastal calm with airy palettes; a limited number of pet-friendly rooms open toward the waterfront.",
            AddressData{"8","Mission St","San Francisco","CA","United States","94105",37.7936,-122.3930});

        add("5","The Ritz-Carlton San Francisco","(415) 296-7465",
            "Perched on Nob Hill, this classic retreat pairs discreet service with hushed corridors and sweeping skyline views. Cable cars glide by the front steps while Grace Cathedral and Huntington Park anchor the neighborhood’s elegant pace. Inside, club-level extras, a serene courtyard, and white-linen dining set the tone. Smaller dogs are warmly welcomed; concierges can map leafy morning strolls and arrange sedan service for evenings downtown.",
            AddressData{"600","Stockton St","San Francisco","CA","United States","94108",37.7925,-122.4070});

        add("6","The St. Regis San Francisco","(415) 284-4000",
            "A modern SoMa landmark where contemporary art, polished service, and an urban spa converge, steps from major museums and gallery openings. Rooms layer rich textiles over clean architecture, with deep soaking tubs for unwinding after conference days at Moscone. Muni and BART are close, and the concierge can point you to chef-driven tasting menus within a few blocks. Pet kits and quiet floors make city stays comfortable for four-legged companions.",
            AddressData{"125","3rd St","San Francisco","CA","United States","94103",37.7867,-122.4005});

        add("7","The Fairmont San Francisco","(415) 772-5000",
            "Crowning Nob Hill with postcard views in every direction, this historic grande dame blends gilded ballrooms and marble staircases with lively lounges. The cable car stops at the corner, whisking you to Union Square or Fisherman’s Wharf. By day, visit bookstores in nearby Chinatown; by night, toast beneath twinkling chandeliers as fog curls over the bay. Pet-friendly rooms are available, and the concierge offers tailored routes to Washington Square Park.",
            AddressData{"950","Mason St","San Francisco","CA","United States","94108",37.7925,-122.4098});

        add("8","The Palace Hotel","(415) 512-1111",
            "A Beaux-Arts landmark in the Financial District, famed for its glass-domed Garden Court where brunch unfolds beneath a canopy of light. Montgomery BART sits a few blocks away, connecting quickly to SFO, Oakland, and Berkeley. Rooms are gracious and quiet for downtown, while the on-site bar channels old San Francisco with polished wood and hushed conversation. Pets are welcome; leafy pocket parks and the Embarcadero are an easy walk for morning outings.",
            AddressData{"2","New Montgomery St","San Francisco","CA","United States","94105",37.7886,-122.4005});

        // Hotel 9
        add("9","Union Square Boutique Hotel","(415) 555-19",
            "Tucked half a block from Union Square, this intimate boutique stay blends classic Beaux-Arts character with "
            "contemporary art installations and a quietly attentive staff. Guests step out to flagship shopping, cable cars, "
            "and pre-theater dining, while a short stroll connects to Powell Street BART for cross-bay and airport access. "
            "Rooms feature warm wood tones and bay window seating overlooking the square’s palm-lined plaza. "
            "Pet-friendly floors include in-room beds and bowls, and a small outdoor relief area is accessible via a side alley. "
            "Within 10 minutes on foot: the Contemporary Jewish Museum, the Westfield Dome, and the Dragon Gate to Chinatown.",
            AddressData{"350","Powell St","San Francisco","CA","United States","94102",37.7881,-122.4085});

        // Hotel 10
        add("10","SoMa Loft Hotel","(415) 555-110",
            "Set in a converted warehouse near Yerba Buena Gardens, this loft-style hotel offers high ceilings, "
            "polished concrete floors, and floor-to-ceiling windows that frame SoMa’s skyline. The lobby espresso bar "
            "pours third-wave roasts for early museum runs—SFMOMA and the Museum of the African Diaspora are minutes away. "
            "Howard Street places you two blocks from the Moscone Center and a 10-minute walk from Powell or Montgomery BART. "
            "Well-behaved pets are welcome; the front desk provides biodegradable bags and a dog-walking map to nearby parks.",
            AddressData{"888","Howard St","San Francisco","CA","United States","94103",37.7836,-122.4039});

        // Hotel 11
        add("11","Embarcadero Bayview Hotel","(415) 555-111",
            "Overlooking the Ferry Building and the Bay, this calm retreat pairs coastal palettes with light woods and "
            "floor-length sheers that glow at sunrise. Guests can stroll the Embarcadero promenade to weekend farmers’ "
            "markets, sip oysters on crushed ice, or hop Muni to Oracle Park. Montgomery Street BART and Embarcadero "
            "Station are both a short walk for easy links to SFO and the East Bay. Pet-friendly rooms are limited but "
            "well-appointed, and a seaside jogging route begins right outside the lobby.",
            AddressData{"4","Embarcadero Center","San Francisco","CA","United States","94111",37.7957,-122.3960});

        // Hotel 12
        add("12","Nob Hill Heritage Hotel","(415) 555-112",
            "Crowned atop Nob Hill, this landmark-inspired property channels old San Francisco with marble-clad corridors, "
            "gilded mirrors, and evening piano in the parlor. Cable cars crest the corner; Grace Cathedral and Huntington Park "
            "are just steps away. Descend California Street to the Financial District or wander north to North Beach cafes. "
            "Montgomery BART is reachable via a cable-car and 10-minute walk. Pets up to 40 lbs are permitted; the concierge "
            "stocks blankets for chilly foggy nights.",
            AddressData{"1001","California St","San Francisco","CA","United States","94108",37.7929,-122.4109});

        // Hotel 13
        add("13","Ferry Building Quarters","(415) 555-113",
            "Anchored at the edge of the bay, this airy stay brings the Ferry Building’s buzz to your doorstep: "
            "artisan cheesemongers, fresh bread still warm from the oven, and stalls of citrus and local honey on weekends. "
            "The Embarcadero waterfront trail unfurls in both directions, while Embarcadero Station provides fast transit links. "
            "Rooms emphasize natural textures and quiet insulation from the city hum. Pet-friendly rooms include a welcome "
            "treat and easy access to waterside green space.",
            AddressData{"1","Ferry Building","San Francisco","CA","United States","94111",37.7955,-122.3937});

        // Hotel 14
        add("14","Yerba Buena Garden Suites","(415) 555-114",
            "Facing the lawns and sculpture gardens of Yerba Buena, this all-suites hotel offers separate living areas for "
            "work and rest, plus a rooftop terrace for golden-hour skyline views. SFMOMA lies around the corner, and Moscone "
            "Center is a five-minute walk for conferences. Montgomery BART and multiple Muni lines simplify crosstown trips. "
            "Pets are welcomed with a small nightly fee; the concierge can recommend groomers and nearby pet boutiques.",
            AddressData{"750","Folsom St","San Francisco","CA","United States","94107",37.7844,-122.4013});

        // Hotel 15
        add("15","Civic Center Grand","(415) 555-115",
            "Steps from the Asian Art Museum and the gilded Beaux-Arts dome of City Hall, this grand property mixes "
            "period detail with modern comforts. Light rail lines and bus routes converge at Civic Center for quick hops "
            "to Hayes Valley dining, the Castro, and the Mission. Rooms facing east catch the morning light; suites "
            "overlook the plaza’s sycamores. Pet policy is relaxed; larger breeds are considered with advance notice.",
            AddressData{"200","Larkin St","San Francisco","CA","United States","94102",37.7803,-122.4165});

        // Hotel 16
        add("16","North Beach Art House","(415) 555-116",
            "In literary North Beach, this art-forward hideout sits near Beat-era landmarks, espresso bars, and red-sauce "
            "trattorias. By day, wander to Coit Tower’s murals; by night, slip into jazz clubs tucked along Columbus Avenue. "
            "The California Street cable car is a short walk away; from there it’s an easy transfer to BART. A limited number "
            "of pet-friendly rooms are available; Washington Square Park offers early-morning romps.",
            AddressData{"530","Columbus Ave","San Francisco","CA","United States","94133",37.8004,-122.4099});

        // Hotel 17
        add("17","Mission Market Inn","(415) 555-117",
            "Rooted in the Mission District, this colorful inn neighbors taquerias, vibrant murals, and the indie boutiques "
            "of Valencia Street. Weekend mornings begin with cold brew and a stroll through the Mission Community Market. "
            "The 24th Street BART station is nearby for trips downtown or to the East Bay. Pet-friendly rooms face an internal "
            "courtyard to reduce street noise; dog-friendly cafes abound within a few blocks.",
            AddressData{"2400","Mission St","San Francisco","CA","United States","94110",37.7595,-122.4184});

        // Hotel 18
        add("18","Financial District Executive Hotel","(415) 555-118",
            "Geared to business travelers, this polished address places you amid the skyscrapers of the Financial District, "
            "with quiet, well-lit workspaces in every room. Morning walks to the Ferry Building set the tone; evenings wind down "
            "in speakeasy-style lounges hidden in plain sight. Montgomery Street BART is five minutes away. Pets are permitted "
            "on designated floors; room-service includes a simple pet menu.",
            AddressData{"555","California St","San Francisco","CA","United States","94104",37.7910,-122.4021});

        // Hotel 19
        add("19","Transbay Terminal Residences","(415) 555-119",
            "Rising near the Salesforce Transit Center’s rooftop park, these sleek residences suit extended stays with "
            "kitchenettes, washer/dryers, and access to a leafy, elevated garden. The Transbay hub connects bus lines across "
            "the Bay Area, while Embarcadero BART is a short walk. Oracle Park lies just over the viaduct for ballgames. "
            "Pets are embraced—longer-stay packages include waived pet fees and monthly grooming partnerships.",
            AddressData{"201","Folsom St","San Francisco","CA","United States","94105",37.7892,-122.3906});

        // Hotel 20
        add("20","South Park Courtyard Hotel","(415) 555-120",
            "Encircling the leafy oval of South Park, this understated hotel balances startup energy with neighborhood calm. "
            "Mornings begin with pastries on outdoor benches beneath towering trees; afternoons end with a stroll to the ballpark "
            "or a gallery opening. The Caltrain terminal at 4th & King is within walking distance, with Muni links back to BART. "
            "Pet-friendly rooms open onto the courtyard for easy early walks; water bowls are set by the entrance on warm days.",
            AddressData{"68","South Park","San Francisco","CA","United States","94107",37.7816,-122.3932});
    }

    hotelreservation::GetProfilesResponse process_request(const hotelreservation::GetProfilesRequest& req) {
        std::lock_guard<std::mutex> lock(profiles_mutex_);

		// Optional dummy compression
#if ENABLE_DUMMY_SERVICE_COMPRESSION
        std::string compressed_random = microservice::compression::compress_data(pre_generated_random_data_);
        std::string decompressed_random = microservice::compression::decompress_data(compressed_random);
#endif

        hotelreservation::GetProfilesResponse response;
        
        microservice::models::GetProfilesRequestData reqd;
        microservice::models::fromProto(req, reqd);
        for (const auto& hotel_id : reqd.hotel_ids) {
            auto it = profiles_.find(hotel_id);
            if (it != profiles_.end()) {
                hotelreservation::HotelProfile proto_profile;
                microservice::models::toProto(it->second, proto_profile);
                *response.add_profiles() = proto_profile;
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