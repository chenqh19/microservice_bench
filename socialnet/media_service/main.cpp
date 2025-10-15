#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include "social_network.pb.h"
#include "serialization_utils.h"
#include "socialnet_padding_utils.h"
#include "../prefork_utils.h"

class MediaService {
public:
    socialnetwork::MediaComposeResponse process_request(const socialnetwork::MediaComposeRequest& req) {
        socialnetwork::MediaComposeResponse resp;
        size_t sz = static_cast<size_t>(req.size_bytes());
        if (sz == 0) sz = 3 * 1024 * 1024; // default 3MB
        std::string blob;
        blob.resize(sz);
        // Fill with a simple repeating pattern
        for (size_t i = 0; i < sz; ++i) blob[i] = static_cast<char>('A' + (i % 26));
        resp.set_media_blob(blob);
        resp.set_mime_type(req.mime_type().empty() ? std::string("application/octet-stream") : req.mime_type());
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }
};

int main() {
    const char* socket_path = "/tmp/media_service.sock";
    const int NUM_WORKERS = 16;
    PreforkServer server(NUM_WORKERS);
    if (!server.setup_socket(socket_path)) { std::cerr << "Failed to setup socket" << std::endl; return 1; }

    if (server.fork_workers()) {
        MediaService service; Ser1de_re ser1de;
        worker_loop<MediaService, socialnetwork::MediaComposeRequest, socialnetwork::MediaComposeResponse>(
            server.get_server_fd(), service, ser1de);
        return 0;
    } else {
        server.master_loop(); return 0;
    }
}


