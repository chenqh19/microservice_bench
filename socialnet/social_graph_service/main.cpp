#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include "social_network.pb.h"
#include "serialization_utils.h"
#include "socialnet_padding_utils.h"
#include "../prefork_utils.h"

class SocialGraphService {
private:
    std::unordered_map<long long, std::unordered_set<long long>> followers_of_user_;
    std::mutex mu_;
public:
    SocialGraphService() {}

    socialnetwork::FollowResponse process_request(const socialnetwork::FollowRequest& req) {
        std::lock_guard<std::mutex> lg(mu_);
        socialnetwork::FollowResponse resp;
        if (req.action() == "follow") {
            followers_of_user_[req.target_user_id()].insert(req.user_id());
            resp.set_message("followed");
        } else if (req.action() == "unfollow") {
            auto it = followers_of_user_.find(req.target_user_id());
            if (it != followers_of_user_.end()) {
                it->second.erase(req.user_id());
            }
            resp.set_message("unfollowed");
        } else {
            resp.set_message("noop");
        }
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }

    socialnetwork::GetFollowersResponse process_get(const socialnetwork::GetFollowersRequest& req) {
        std::lock_guard<std::mutex> lg(mu_);
        socialnetwork::GetFollowersResponse resp;
        auto it = followers_of_user_.find(req.user_id());
        if (it != followers_of_user_.end()) {
            for (auto fid : it->second) resp.add_follower_ids(fid);
        }
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }
};

int main() {
    const char* socket_path = "/tmp/social_graph_service.sock";
    const int NUM_WORKERS = 16;

    PreforkServer server(NUM_WORKERS);
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }

    if (server.fork_workers()) {
        SocialGraphService service;
        Ser1de_re ser1de;

        while (true) {
            int client_fd = accept(server.get_server_fd(), nullptr, nullptr);
            if (client_fd < 0) { if (errno == EINTR) continue; perror("accept"); break; }

            char len_buf[4];
            ssize_t n = read(client_fd, len_buf, 4);
            if (n != 4) { close(client_fd); continue; }
            uint32_t msg_len = 0; memcpy(&msg_len, len_buf, 4);
            std::vector<char> buf(msg_len);
            n = read(client_fd, buf.data(), msg_len);
            if (n != (ssize_t)msg_len) { close(client_fd); continue; }

            socialnetwork::FollowRequest follow_req;
            bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), follow_req);
            if (ok) {
                auto resp = service.process_request(follow_req);
                std::string resp_str = microservice::utils::serialize_message(ser1de, resp);
                uint32_t resp_len = resp_str.size();
                write(client_fd, &resp_len, 4);
                write(client_fd, resp_str.data(), resp_len);
                close(client_fd);
                continue;
            }

            socialnetwork::GetFollowersRequest get_req;
            ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), get_req);
            if (ok) {
                auto resp = service.process_get(get_req);
                std::string resp_str = microservice::utils::serialize_message(ser1de, resp);
                uint32_t resp_len = resp_str.size();
                write(client_fd, &resp_len, 4);
                write(client_fd, resp_str.data(), resp_len);
            }
            close(client_fd);
        }
        return 0;
    } else {
        server.master_loop();
        return 0;
    }
}
