#include <iostream>
#include <unordered_map>
#include <unordered_set>
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
        if (r == 0) return false;
        if (r < 0) { if (errno == EINTR) continue; return false; }
        total += static_cast<size_t>(r);
    }
    return true;
}

class HomeTimelineService {
private:
    std::vector<std::deque<long long>> timeline_by_user_; // 1..200
    std::unordered_map<int, std::unordered_set<int>> followers_of_user_;
    std::mutex mu_;
public:
    HomeTimelineService() : timeline_by_user_(201) {
        // Build deterministic followers (same as social_graph_service)
        for (int i = 1; i <= 200; ++i) {
            int follower = (i == 1) ? 200 : (i - 1);
            followers_of_user_[i].insert(follower);
        }
        int added = 200;
        for (int i = 1; i <= 200 && added < 2000; ++i) {
            for (int step = 2; step <= 11 && added < 2000; ++step) {
                int follower = ((i - step - 1) % 200 + 200) % 200 + 1;
                if (follower != i && followers_of_user_[i].insert(follower).second) ++added;
            }
        }
        // Fanout 1000 posts
        for (long long post_id = 1000; post_id >= 1; --post_id) {
            int author = (int)(((post_id - 1) % 200) + 1);
            for (int fid : followers_of_user_[author]) {
                timeline_by_user_[fid].push_back(post_id);
            }
        }
    }

    socialnetwork::ReadHomeTimelineResponse process_read(const socialnetwork::ReadHomeTimelineRequest& req) {
        std::lock_guard<std::mutex> lg(mu_);
        socialnetwork::ReadHomeTimelineResponse resp;
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
    const char* socket_path = "/tmp/home_timeline_service.sock";
    const int NUM_WORKERS = 16;

    PreforkServer server(NUM_WORKERS);
    if (!server.setup_socket(socket_path)) { std::cerr << "Failed to setup socket" << std::endl; return 1; }

    if (server.fork_workers()) {
        HomeTimelineService service; Ser1de_re ser1de;

        while (true) {
            int client_fd = accept(server.get_server_fd(), nullptr, nullptr);
            if (client_fd < 0) { if (errno == EINTR) continue; perror("accept"); break; }

            uint32_t msg_len = 0;
            if (!read_exact(client_fd, &msg_len, 4)) { close(client_fd); continue; }
            std::vector<char> buf(msg_len);
            if (!read_exact(client_fd, buf.data(), msg_len)) { close(client_fd); continue; }

            socialnetwork::ReadHomeTimelineRequest read_req;
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
