#include <iostream>
#include <unordered_map>
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

class PostStorageService {
private:
    std::unordered_map<long long, socialnetwork::Post> posts_by_id_;
    std::mutex mu_;
public:
    PostStorageService() {}

    socialnetwork::StorePostResponse process_request(const socialnetwork::StorePostRequest& req) {
        std::lock_guard<std::mutex> lg(mu_);
        posts_by_id_[req.post().post_id()] = req.post();
        socialnetwork::StorePostResponse resp;
        resp.set_message("stored");
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }

    socialnetwork::ReadPostsByIdsResponse process_read(const socialnetwork::ReadPostsByIdsRequest& req) {
        std::lock_guard<std::mutex> lg(mu_);
        socialnetwork::ReadPostsByIdsResponse resp;
        for (auto id : req.post_ids()) {
            auto it = posts_by_id_.find(id);
            if (it != posts_by_id_.end()) {
                *resp.add_posts() = it->second;
            }
        }
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }
};

int main() {
    const char* socket_path = "/tmp/post_storage_service.sock";
    const int NUM_WORKERS = 16;

    PreforkServer server(NUM_WORKERS);
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }

    if (server.fork_workers()) {
        PostStorageService service;
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

            socialnetwork::StorePostRequest store_req;
            bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), store_req);
            if (ok) {
                auto resp = service.process_request(store_req);
                std::string resp_str = microservice::utils::serialize_message(ser1de, resp);
                uint32_t resp_len = resp_str.size();
                write(client_fd, &resp_len, 4);
                write(client_fd, resp_str.data(), resp_len);
                close(client_fd);
                continue;
            }

            socialnetwork::ReadPostsByIdsRequest read_req;
            ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), read_req);
            if (ok) {
                auto resp = service.process_read(read_req);
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
