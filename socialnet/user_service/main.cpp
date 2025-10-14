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

class SocialUserService {
private:
    std::unordered_map<std::string, long long> username_to_id_;
    std::mutex mu_;
    long long next_user_id_ = 1;
public:
    SocialUserService() {}

    socialnetwork::RegisterUserResponse process_request(const socialnetwork::RegisterUserRequest& req) {
        std::lock_guard<std::mutex> lg(mu_);
        socialnetwork::RegisterUserResponse resp;
        auto it = username_to_id_.find(req.username());
        if (it != username_to_id_.end()) {
            resp.set_user_id(it->second);
            resp.set_message("exists");
        } else {
            long long new_id = next_user_id_++;
            username_to_id_[req.username()] = new_id;
            resp.set_user_id(new_id);
            resp.set_message("created");
        }
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }

    socialnetwork::GetUserResponse process_get(const socialnetwork::GetUserRequest& req) {
        std::lock_guard<std::mutex> lg(mu_);
        socialnetwork::GetUserResponse resp;
        auto it = username_to_id_.find(req.username());
        if (it == username_to_id_.end()) {
            resp.set_user_id(0);
            resp.set_found("False");
        } else {
            resp.set_user_id(it->second);
            resp.set_found("True");
        }
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }
};

int main() {
    const char* socket_path = "/tmp/user_service.sock";
    const int NUM_WORKERS = 16;

    PreforkServer server(NUM_WORKERS);
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }

    if (server.fork_workers()) {
        SocialUserService service;
        Ser1de_re ser1de;

        while (true) {
            int client_fd = accept(server.get_server_fd(), nullptr, nullptr);
            if (client_fd < 0) {
                if (errno == EINTR) continue;
                perror("accept");
                break;
            }

            char len_buf[4];
            ssize_t n = read(client_fd, len_buf, 4);
            if (n != 4) { close(client_fd); continue; }
            uint32_t msg_len = 0; memcpy(&msg_len, len_buf, 4);
            std::vector<char> buf(msg_len);
            n = read(client_fd, buf.data(), msg_len);
            if (n != (ssize_t)msg_len) { close(client_fd); continue; }

            socialnetwork::RegisterUserRequest reg_req;
            bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), reg_req);
            if (ok) {
                auto resp = service.process_request(reg_req);
                std::string resp_str = microservice::utils::serialize_message(ser1de, resp);
                uint32_t resp_len = resp_str.size();
                write(client_fd, &resp_len, 4);
                write(client_fd, resp_str.data(), resp_len);
                close(client_fd);
                continue;
            }

            socialnetwork::GetUserRequest get_req;
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
