#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include "social_network.pb.h"
#include "serialization_utils.h"
#include "socialnet_padding_utils.h"
#include "../prefork_utils.h"

static std::string uds_call(const std::string& path, const std::string& data) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return std::string();
    sockaddr_un addr{}; addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) { close(fd); return std::string(); }
    uint32_t len = data.size();
    if (write(fd, &len, 4) != 4) { close(fd); return std::string(); }
    if (write(fd, data.data(), len) != (ssize_t)len) { close(fd); return std::string(); }
    char len_buf[4];
    if (read(fd, len_buf, 4) != 4) { close(fd); return std::string(); }
    uint32_t resp_len = 0; memcpy(&resp_len, len_buf, 4);
    std::string out; out.resize(resp_len);
    if (resp_len > 0 && read(fd, &out[0], resp_len) != (ssize_t)resp_len) { close(fd); return std::string(); }
    close(fd); return out;
}

class ComposePostService {
public:
    Ser1de_re ser1de;

    socialnetwork::ComposePostResponse process_request(const socialnetwork::ComposePostRequest& req) {
        socialnetwork::ComposePostResponse out;

        // 0) text processing
        socialnetwork::TextProcessRequest tpreq; tpreq.set_text(req.text()); *tpreq.mutable_padding() = microservice::utils::generate_person_padding();
        socialnetwork::TextProcessResponse tpresp; microservice::utils::deserialize_message(ser1de, uds_call("/tmp/text_service.sock", microservice::utils::serialize_message(ser1de, tpreq)), tpresp);

        // shorten any URLs found
        for (int i = 0; i < tpresp.urls_size(); ++i) {
            socialnetwork::UrlShortenRequest ureq; ureq.set_url(tpresp.urls(i)); *ureq.mutable_padding() = microservice::utils::generate_person_padding();
            socialnetwork::UrlShortenResponse uresp; microservice::utils::deserialize_message(ser1de, uds_call("/tmp/url_shorten_service.sock", microservice::utils::serialize_message(ser1de, ureq)), uresp);
            // naive replace: replace first occurrence
            size_t pos = tpresp.mutable_text()->find(tpresp.urls(i));
            if (pos != std::string::npos) tpresp.mutable_text()->replace(pos, tpresp.urls(i).size(), uresp.short_url());
        }

        // 1) get or register user id
        socialnetwork::GetUserRequest get_user; get_user.set_username(req.username()); *get_user.mutable_padding() = microservice::utils::generate_person_padding();
        std::string get_user_req = microservice::utils::serialize_message(ser1de, get_user);
        std::string get_user_resp_str = uds_call("/tmp/user_service.sock", get_user_req);
        socialnetwork::GetUserResponse get_user_resp;
        bool ok = microservice::utils::deserialize_message(ser1de, get_user_resp_str, get_user_resp);
        long long user_id = 0;
        if (ok && get_user_resp.found() == "True") {
            user_id = get_user_resp.user_id();
        } else {
            socialnetwork::RegisterUserRequest reg; reg.set_username(req.username()); *reg.mutable_padding() = microservice::utils::generate_person_padding();
            std::string reg_req = microservice::utils::serialize_message(ser1de, reg);
            std::string reg_resp_str = uds_call("/tmp/user_service.sock", reg_req);
            socialnetwork::RegisterUserResponse reg_resp;
            microservice::utils::deserialize_message(ser1de, reg_resp_str, reg_resp);
            user_id = reg_resp.user_id();
        }

        // 2) allocate unique id
        socialnetwork::UniqueIdRequest uid_req; *uid_req.mutable_padding() = microservice::utils::generate_person_padding();
        std::string uid_req_str = microservice::utils::serialize_message(ser1de, uid_req);
        std::string uid_resp_str = uds_call("/tmp/unique_id_service.sock", uid_req_str);
        socialnetwork::UniqueIdResponse uid_resp; microservice::utils::deserialize_message(ser1de, uid_resp_str, uid_resp);

        // 3) store post
        socialnetwork::Post post; post.set_post_id(uid_resp.id()); post.set_user_id(user_id); post.set_username(req.username()); post.set_text(tpresp.text()); post.set_timestamp((long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()); *post.mutable_padding() = microservice::utils::generate_person_padding();
        socialnetwork::StorePostRequest store_req; *store_req.mutable_padding() = microservice::utils::generate_person_padding(); *store_req.mutable_post() = post;
        std::string store_req_str = microservice::utils::serialize_message(ser1de, store_req);
        uds_call("/tmp/post_storage_service.sock", store_req_str);

        // 4) write user timeline
        socialnetwork::WriteUserTimelineRequest wut; wut.set_user_id(user_id); wut.set_post_id(post.post_id()); *wut.mutable_padding() = microservice::utils::generate_person_padding();
        uds_call("/tmp/user_timeline_service.sock", microservice::utils::serialize_message(ser1de, wut));

        // 5) get followers and fanout to home_timeline
        socialnetwork::GetFollowersRequest gf; gf.set_user_id(user_id); *gf.mutable_padding() = microservice::utils::generate_person_padding();
        std::string gf_resp_str = uds_call("/tmp/social_graph_service.sock", microservice::utils::serialize_message(ser1de, gf));
        socialnetwork::GetFollowersResponse gf_resp; microservice::utils::deserialize_message(ser1de, gf_resp_str, gf_resp);

        socialnetwork::WriteHomeTimelineRequest wht; wht.set_user_id(user_id); wht.set_post_id(post.post_id()); for (auto fid : gf_resp.follower_ids()) wht.add_follower_ids(fid); *wht.mutable_padding() = microservice::utils::generate_person_padding();
        uds_call("/tmp/home_timeline_service.sock", microservice::utils::serialize_message(ser1de, wht));

        // 6) write mentions to mention timelines
        for (int i = 0; i < tpresp.mentions_size(); ++i) {
            socialnetwork::GetUserRequest gu; gu.set_username(tpresp.mentions(i)); *gu.mutable_padding() = microservice::utils::generate_person_padding();
            socialnetwork::GetUserResponse gur; microservice::utils::deserialize_message(ser1de, uds_call("/tmp/user_service.sock", microservice::utils::serialize_message(ser1de, gu)), gur);
            if (gur.found() == "True") {
                socialnetwork::WriteUserMentionRequest wm; wm.set_user_id(gur.user_id()); wm.set_post_id(post.post_id()); *wm.mutable_padding() = microservice::utils::generate_person_padding();
                uds_call("/tmp/user_mention_service.sock", microservice::utils::serialize_message(ser1de, wm));
            }
        }

        out.set_post_id(post.post_id());
        out.set_message("ok");
        *out.mutable_padding() = microservice::utils::generate_person_padding();
        return out;
    }
};

int main() {
    const char* socket_path = "/tmp/compose_post_service.sock";
    const int NUM_WORKERS = 16;

    PreforkServer server(NUM_WORKERS);
    if (!server.setup_socket(socket_path)) { std::cerr << "Failed to setup socket" << std::endl; return 1; }

    if (server.fork_workers()) {
        ComposePostService service; Ser1de_re ser1de;
        worker_loop<ComposePostService, socialnetwork::ComposePostRequest, socialnetwork::ComposePostResponse>(
            server.get_server_fd(), service, ser1de);
        return 0;
    } else {
        server.master_loop(); return 0;
    }
}
