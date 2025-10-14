#include <iostream>
#include <string>
#include <json/json.h>
#include "social_network.pb.h"
#include "serialization_utils.h"
#include "socialnet_padding_utils.h"
#include <httplib.h>
#include <chrono>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <sys/wait.h>
#include <signal.h>

class FrontEndService {
public:
    static constexpr int POOL_SIZE = 64;

    Ser1de_re ser1de;

    static std::string uds_call(const std::string& path, const std::string& data) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return std::string();
        sockaddr_un addr{}; addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) { close(fd); return std::string(); }
        uint32_t len = data.size();
        if (write(fd, &len, 4) != 4) { close(fd); return std::string(); }
        if (write(fd, data.data(), len) != (ssize_t)len) { close(fd); return std::string(); }
        char len_buf[4]; if (read(fd, len_buf, 4) != 4) { close(fd); return std::string(); }
        uint32_t resp_len = 0; memcpy(&resp_len, len_buf, 4);
        std::string out; out.resize(resp_len);
        if (resp_len > 0 && read(fd, &out[0], resp_len) != (ssize_t)resp_len) { close(fd); return std::string(); }
        close(fd); return out;
    }

    std::string handle_compose(const std::string& username, const std::string& text) {
        socialnetwork::ComposePostRequest req; req.set_username(username); req.set_text(text); *req.mutable_padding() = microservice::utils::generate_person_padding();
        auto resp_str = uds_call("/tmp/compose_post_service.sock", microservice::utils::serialize_message(ser1de, req));
        return resp_str; // protobuf bytes
    }

    std::string handle_follow(long long user_id, long long target_user_id, const std::string& action) {
        socialnetwork::FollowRequest req; req.set_user_id(user_id); req.set_target_user_id(target_user_id); req.set_action(action); *req.mutable_padding() = microservice::utils::generate_person_padding();
        return uds_call("/tmp/social_graph_service.sock", microservice::utils::serialize_message(ser1de, req));
    }

    std::string handle_register(const std::string& username) {
        socialnetwork::RegisterUserRequest req; req.set_username(username); *req.mutable_padding() = microservice::utils::generate_person_padding();
        return uds_call("/tmp/user_service.sock", microservice::utils::serialize_message(ser1de, req));
    }

    std::string handle_user_timeline(long long user_id, long long start, long long limit) {
        socialnetwork::ReadUserTimelineRequest req; req.set_user_id(user_id); req.set_start(start); req.set_limit(limit); *req.mutable_padding() = microservice::utils::generate_person_padding();
        auto resp_str = uds_call("/tmp/user_timeline_service.sock", microservice::utils::serialize_message(ser1de, req));
        Json::Value arr(Json::arrayValue);
        socialnetwork::ReadUserTimelineResponse resp;
        if (microservice::utils::deserialize_message(ser1de, resp_str, resp)) {
            for (const auto& id : resp.post_ids()) { arr.append(Json::Int64(id)); }
        }
        Json::FastWriter w; return w.write(arr);
    }

    std::string handle_home_timeline(long long user_id, long long start, long long limit) {
        socialnetwork::ReadHomeTimelineRequest req; req.set_user_id(user_id); req.set_start(start); req.set_limit(limit); *req.mutable_padding() = microservice::utils::generate_person_padding();
        auto resp_str = uds_call("/tmp/home_timeline_service.sock", microservice::utils::serialize_message(ser1de, req));
        Json::Value arr(Json::arrayValue);
        socialnetwork::ReadHomeTimelineResponse resp;
        if (microservice::utils::deserialize_message(ser1de, resp_str, resp)) {
            for (const auto& id : resp.post_ids()) { arr.append(Json::Int64(id)); }
        }
        Json::FastWriter w; return w.write(arr);
    }
};

class PreforkHTTPServer {
private:
    int num_workers_;
    std::vector<pid_t> worker_pids_;
    bool should_stop_;
public:
    PreforkHTTPServer(int num_workers = 32) : num_workers_(num_workers), should_stop_(false) {}

    bool fork_workers() {
        for (int i = 0; i < num_workers_; ++i) {
            pid_t pid = fork();
            if (pid == 0) { return true; }
            else if (pid > 0) { worker_pids_.push_back(pid); }
            else { perror("fork"); return false; }
        }
        return false;
    }

    void master_loop() {
        while (!should_stop_) { int status; pid_t dead = waitpid(-1, &status, WNOHANG); if (dead > 0) { pid_t np = fork(); if (np == 0) return; else if (np > 0) worker_pids_.push_back(np); } usleep(100000); }
        for (pid_t pid : worker_pids_) kill(pid, SIGTERM);
        for (pid_t pid : worker_pids_) waitpid(pid, nullptr, 0);
    }
};

int main() {
    PreforkHTTPServer server(FrontEndService::POOL_SIZE);
    if (server.fork_workers()) {
        FrontEndService svc;
        httplib::Server http;
        http.set_keep_alive_max_count(50000);
        http.set_read_timeout(5);
        http.set_write_timeout(5);
        http.set_idle_interval(0, 100000);
        http.set_payload_max_length(1024 * 1024);

        http.Post("/compose", [&](const httplib::Request& req, httplib::Response& res) {
            Json::Value json; Json::Reader reader;
            if (!reader.parse(req.body, json)) { res.status = 400; res.set_content("{\"error\":\"bad json\"}", "application/json"); return; }
            std::string username = json.get("username", "").asString();
            std::string text = json.get("text", "").asString();
            auto resp = svc.handle_compose(username, text);
            res.set_content(resp, "application/octet-stream");
        });

        http.Get("/compose", [&](const httplib::Request& req, httplib::Response& res) {
            std::string username = req.get_param_value("username");
            std::string text = req.get_param_value("text");
            auto resp = svc.handle_compose(username, text);
            res.set_content(resp, "application/octet-stream");
        });

        http.Post("/follow", [&](const httplib::Request& req, httplib::Response& res) {
            Json::Value json; Json::Reader reader;
            if (!reader.parse(req.body, json)) { res.status = 400; res.set_content("{\"error\":\"bad json\"}", "application/json"); return; }
            long long user_id = json.get("userId", 0).asInt64();
            long long target_user_id = json.get("targetUserId", 0).asInt64();
            std::string action = json.get("action", "follow").asString();
            auto resp = svc.handle_follow(user_id, target_user_id, action);
            res.set_content(resp, "application/octet-stream");
        });

        http.Get("/follow", [&](const httplib::Request& req, httplib::Response& res) {
            long long user_id = std::stoll(req.get_param_value("userId"));
            long long target_user_id = std::stoll(req.get_param_value("targetUserId"));
            std::string action = req.get_param_value("action");
            auto resp = svc.handle_follow(user_id, target_user_id, action);
            res.set_content(resp, "application/octet-stream");
        });

        http.Get("/user_timeline", [&](const httplib::Request& req, httplib::Response& res) {
            long long user_id = std::stoll(req.get_param_value("userId"));
            long long start = std::stoll(req.get_param_value("start"));
            long long limit = std::stoll(req.get_param_value("limit"));
            auto resp = svc.handle_user_timeline(user_id, start, limit);
            res.set_content(resp, "application/json");
        });

        http.Get("/home_timeline", [&](const httplib::Request& req, httplib::Response& res) {
            long long user_id = std::stoll(req.get_param_value("userId"));
            long long start = std::stoll(req.get_param_value("start"));
            long long limit = std::stoll(req.get_param_value("limit"));
            auto resp = svc.handle_home_timeline(user_id, start, limit);
            res.set_content(resp, "application/json");
        });

        http.Post("/register", [&](const httplib::Request& req, httplib::Response& res) {
            Json::Value json; Json::Reader reader; if (!reader.parse(req.body, json)) { res.status = 400; res.set_content("{\"error\":\"bad json\"}", "application/json"); return; }
            std::string username = json.get("username", "").asString();
            auto resp = svc.handle_register(username);
            res.set_content(resp, "application/octet-stream");
        });

        http.Get("/register", [&](const httplib::Request& req, httplib::Response& res) {
            std::string username = req.get_param_value("username");
            auto resp = svc.handle_register(username);
            res.set_content(resp, "application/octet-stream");
        });

        std::cout << "Listening on 0.0.0.0:50060" << std::endl;
        http.listen("0.0.0.0", 50060);
        return 0;
    } else {
        server.master_loop();
        return 0;
    }
}
