#include <iostream>
#include <vector>
#include <deque>
#include <mutex>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include "social_network.pb.h"
#include "serialization_utils.h"
#include "socialnet_padding_utils.h"
#include "../prefork_utils.h"

static bool read_exact(int fd, void* buf, size_t nbytes) {
    char* p = static_cast<char*>(buf);
    size_t total = 0;
    while (total < nbytes) {
        ssize_t r = read(fd, p + total, nbytes - total);
        if (r == 0) return false; // EOF
        if (r < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        total += static_cast<size_t>(r);
    }
    return true;
}

class UserTimelineService {
private:
    std::vector<std::deque<long long>> timeline_by_user_; // 1..200
    std::mutex mu_;
public:
    UserTimelineService() : timeline_by_user_(201) {
        // Precompute: 1000 posts, post_id i belongs to user ((i-1)%200)+1
        for (long long post_id = 1000; post_id >= 1; --post_id) {
            int uid = (int)(((post_id - 1) % 200) + 1);
            timeline_by_user_[uid].push_back(post_id);
        }
    }

    socialnetwork::ReadUserTimelineResponse process_read(const socialnetwork::ReadUserTimelineRequest& req) {
        std::lock_guard<std::mutex> lg(mu_);
        socialnetwork::ReadUserTimelineResponse resp;
        long long uid = req.user_id();
        if (uid < 1 || uid >= (long long)timeline_by_user_.size()) {
            *resp.mutable_padding() = microservice::utils::generate_person_padding();
            return resp;
        }
        auto& dq = timeline_by_user_[uid];
        long long start = req.start();
        long long limit = req.limit();
        for (long long i = start; i < (long long)dq.size() && (i - start) < limit; ++i) {
            resp.add_post_ids(dq[i]);
        }
        *resp.mutable_padding() = microservice::utils::generate_person_padding();
        return resp;
    }
};

int main() {
    const char* socket_path = "/tmp/user_timeline_service.sock";
    const int NUM_WORKERS = 16;

    PreforkServer server(NUM_WORKERS);
    if (!server.setup_socket(socket_path)) { std::cerr << "Failed to setup socket" << std::endl; return 1; }

    if (server.fork_workers()) {
        UserTimelineService service; Ser1de_re ser1de;

        while (true) {
            int client_fd = accept(server.get_server_fd(), nullptr, nullptr);
            if (client_fd < 0) { if (errno == EINTR) continue; perror("accept"); break; }

            uint32_t msg_len = 0;
            if (!read_exact(client_fd, &msg_len, 4)) { close(client_fd); continue; }
            std::vector<char> buf(msg_len);
            if (!read_exact(client_fd, buf.data(), msg_len)) { close(client_fd); continue; }

            socialnetwork::ReadUserTimelineRequest read_req;
            bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), read_req);
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
        server.master_loop(); return 0;
    }
}
