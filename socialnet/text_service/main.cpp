#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include "social_network.pb.h"
#include "serialization_utils.h"
#include "socialnet_padding_utils.h"
#include "../prefork_utils.h"

class TextService {
public:
    socialnetwork::TextProcessResponse process_request(const socialnetwork::TextProcessRequest& req) {
        socialnetwork::TextProcessResponse resp;
        std::string text = req.text();

        // Extract @mentions, #hashtags, and URLs (basic regex)
        std::regex mention_re("@([A-Za-z0-9_]+)");
        std::regex hashtag_re("#([A-Za-z0-9_]+)");
        std::regex url_re("https?://[^\n\r\t ]+");

        for (std::sregex_iterator it(text.begin(), text.end(), mention_re), end; it != end; ++it) {
            resp.add_mentions((*it)[1].str());
        }
        for (std::sregex_iterator it(text.begin(), text.end(), hashtag_re), end; it != end; ++it) {
            resp.add_hashtags((*it)[1].str());
        }
        for (std::sregex_iterator it(text.begin(), text.end(), url_re), end; it != end; ++it) {
            resp.add_urls((*it)[0].str());
        }

        resp.set_text(text);
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }
};

int main() {
    const char* socket_path = "/tmp/text_service.sock";
    const int NUM_WORKERS = 32;

    PreforkServer server(NUM_WORKERS);
    if (!server.setup_socket(socket_path)) { std::cerr << "Failed to setup socket" << std::endl; return 1; }

    if (server.fork_workers()) {
        TextService service; Ser1de_re ser1de;
        worker_loop<TextService, socialnetwork::TextProcessRequest, socialnetwork::TextProcessResponse>(
            server.get_server_fd(), service, ser1de);
        return 0;
    } else {
        server.master_loop(); return 0;
    }
}


