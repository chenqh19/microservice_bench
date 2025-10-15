#include <iostream>
#include <atomic>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include "social_network.pb.h"
#include "serialization_utils.h"
#include "socialnet_padding_utils.h"
#include "../prefork_utils.h"

class UniqueIdService {
private:
    std::atomic<long long> next_id_;
public:
    UniqueIdService() : next_id_(1) {}

    socialnetwork::UniqueIdResponse process_request(const socialnetwork::UniqueIdRequest& req) {
        (void)req;
        socialnetwork::UniqueIdResponse resp;
        long long id = next_id_.fetch_add(1);
        resp.set_id(id);
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }
};

int main() {
    const char* socket_path = "/tmp/unique_id_service.sock";
    const int NUM_WORKERS = 16;

    PreforkServer server(NUM_WORKERS);
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }

    if (server.fork_workers()) {
        UniqueIdService service;
        Ser1de_re ser1de;
        worker_loop<UniqueIdService, socialnetwork::UniqueIdRequest, socialnetwork::UniqueIdResponse>(
            server.get_server_fd(), service, ser1de);
        return 0;
    } else {
        server.master_loop();
        return 0;
    }
}
