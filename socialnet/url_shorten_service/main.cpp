#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include "social_network.pb.h"
#include "serialization_utils.h"
#include "socialnet_padding_utils.h"
#include "../prefork_utils.h"

class UrlShortenService {
private:
    std::unordered_map<std::string, std::string> long_to_short_;
    std::unordered_map<std::string, std::string> short_to_long_;
    std::mutex mu_;
    int counter_ = 1;

    std::string encode(int n) {
        static const char* alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::string s;
        while (n > 0) { s.push_back(alphabet[n % 62]); n /= 62; }
        if (s.empty()) s = "a";
        return std::string(s.rbegin(), s.rend());
    }
public:
    socialnetwork::UrlShortenResponse process_request(const socialnetwork::UrlShortenRequest& req) {
        std::lock_guard<std::mutex> lg(mu_);
        socialnetwork::UrlShortenResponse resp;
        auto it = long_to_short_.find(req.url());
        if (it != long_to_short_.end()) {
            resp.set_short_url(it->second);
        } else {
            std::string code = encode(counter_++);
            std::string short_url = std::string("http://s/") + code;
            long_to_short_[req.url()] = short_url;
            short_to_long_[short_url] = req.url();
            resp.set_short_url(short_url);
        }
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }
};

int main() {
    const char* socket_path = "/tmp/url_shorten_service.sock";
    const int NUM_WORKERS = 8;

    PreforkServer server(NUM_WORKERS);
    if (!server.setup_socket(socket_path)) { std::cerr << "Failed to setup socket" << std::endl; return 1; }

    if (server.fork_workers()) {
        UrlShortenService service; Ser1de_re ser1de;
        worker_loop<UrlShortenService, socialnetwork::UrlShortenRequest, socialnetwork::UrlShortenResponse>(
            server.get_server_fd(), service, ser1de);
        return 0;
    } else {
        server.master_loop(); return 0;
    }
}


