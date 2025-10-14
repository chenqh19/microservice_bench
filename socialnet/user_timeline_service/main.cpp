#include <iostream>
#include <unordered_map>
#include <vector>
#include <deque>
#include <mutex>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include "social_network.pb.h"
#include "serialization_utils.h"
#include "socialnet_padding_utils.h"
#include "../prefork_utils.h"

class UserTimelineService {
private:
    std::unordered_map<long long, std::deque<long long>> timeline_by_user_;
    std::mutex mu_;
    static constexpr size_t MAX_TIMELINE = 1000;
public:
    UserTimelineService() {}

    socialnetwork::WriteUserTimelineResponse process_request(const socialnetwork::WriteUserTimelineRequest& req) {
        std::lock_guard<std::mutex> lg(mu_);
        auto& dq = timeline_by_user_[req.user_id()];
        dq.push_front(req.post_id());
        if (dq.size() > MAX_TIMELINE) dq.pop_back();
        socialnetwork::WriteUserTimelineResponse resp;
        resp.set_message("ok");
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }

    socialnetwork::ReadUserTimelineResponse process_read(const socialnetwork::ReadUserTimelineRequest& req) {
        std::lock_guard<std::mutex> lg(mu_);
        socialnetwork::ReadUserTimelineResponse resp;
        auto it = timeline_by_user_.find(req.user_id());
        if (it != timeline_by_user_.end()) {
            long long start = req.start();
            long long limit = req.limit();
            for (long long i = start; i < (long long)it->second.size() && (i - start) < limit; ++i) {
                resp.add_post_ids(it->second[i]);
            }
        }
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }
};

int main() {
    const char* socket_path = "/tmp/user_timeline_service.sock";
    const int NUM_WORKERS = 16;

    PreforkServer server(NUM_WORKERS);
    if (!server.setup_socket(socket_path)) {
        std::cerr << "Failed to setup socket" << std::endl;
        return 1;
    }

    if (server.fork_workers()) {
        UserTimelineService service;
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

            socialnetwork::WriteUserTimelineRequest write_req;
            bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), write_req);
            if (ok) {
                auto resp = service.process_request(write_req);
                std::string resp_str = microservice::utils::serialize_message(ser1de, resp);
                uint32_t resp_len = resp_str.size();
                write(client_fd, &resp_len, 4);
                write(client_fd, resp_str.data(), resp_len);
                close(client_fd);
                continue;
            }

            socialnetwork::ReadUserTimelineRequest read_req;
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
