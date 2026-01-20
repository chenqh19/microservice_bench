#include "utils/compression_utils.h"
#include "config.h"
#include <httplib.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <algorithm>
#
class CompressFrontendService {
private:
    std::string pre_generated_random_data_;
public:
    static constexpr int POOL_SIZE = 64;
    CompressFrontendService() {
			const size_t kPreGenSize = 2 * 1024 * 1024;
			const size_t kBlockSize = 4096; // repeatable base pattern size
			// Build a compressible base pattern
			std::string base(kBlockSize, 'A');
			for (size_t j = 0; j < kBlockSize; ++j) {
				base[j] = static_cast<char>('A' + (j % 26));
			}
			pre_generated_random_data_.resize(kPreGenSize);
			// Fill buffer with repeated base pattern and a few deterministic mutations per block
			for (size_t offset = 0, block_idx = 0; offset < kPreGenSize; offset += kBlockSize, ++block_idx) {
				const size_t chunk = std::min(kBlockSize, kPreGenSize - offset);
				std::copy(base.begin(), base.begin() + chunk, pre_generated_random_data_.begin() + offset);
				// Deterministic LCG for light mutations to keep data compressible
				uint32_t rng = 0x9E3779B9u ^ static_cast<uint32_t>(block_idx);
				const size_t mutations = std::max<size_t>(1, chunk / 64); // ~1.5% bytes mutated
				for (size_t m = 0; m < mutations; ++m) {
					rng = rng * 1664525u + 1013904223u;
					size_t pos = rng % chunk;
					char c = static_cast<char>('A' + ((rng >> 8) % 26));
					pre_generated_random_data_[offset + pos] = c;
				}
			}
        microservice::compression::init_compression();
    }
    const std::string& get_buffer() const {
        return pre_generated_random_data_;
    }
};
#
class PreforkHTTPServer {
private:
    int num_workers_;
    std::vector<pid_t> worker_pids_;
    bool should_stop_;
public:
    PreforkHTTPServer(int num_workers = 48) : num_workers_(num_workers), should_stop_(false) {
        signal(SIGTERM, [](int) { /* handled in master loop */ });
        signal(SIGINT, [](int) { /* handled in master loop */ });
    }
    bool fork_workers() {
        std::cout << "Starting " << num_workers_ << " HTTP worker processes..." << std::endl;
        for (int i = 0; i < num_workers_; ++i) {
            pid_t pid = fork();
            if (pid == 0) {
                std::cout << "HTTP Worker process " << getpid() << " started" << std::endl;
                return true;
            } else if (pid > 0) {
                worker_pids_.push_back(pid);
            } else {
                perror("fork");
                return false;
            }
        }
        std::cout << "HTTP Master process " << getpid() << " started " << worker_pids_.size() << " workers" << std::endl;
        return false;
    }
    void master_loop() {
        std::cout << "HTTP Master process waiting for workers..." << std::endl;
        while (!should_stop_) {
            int status;
            pid_t dead_pid = waitpid(-1, &status, WNOHANG);
            if (dead_pid > 0) {
                std::cout << "HTTP Worker " << dead_pid << " died, restarting..." << std::endl;
                worker_pids_.erase(
                    std::remove(worker_pids_.begin(), worker_pids_.end(), dead_pid),
                    worker_pids_.end()
                );
                pid_t new_pid = fork();
                if (new_pid == 0) {
                    std::cout << "Restarted HTTP worker process " << getpid() << std::endl;
                    return;
                } else if (new_pid > 0) {
                    worker_pids_.push_back(new_pid);
                }
            }
            usleep(100000);
        }
        std::cout << "HTTP Master process shutting down workers..." << std::endl;
        for (pid_t pid : worker_pids_) {
            kill(pid, SIGTERM);
        }
        for (pid_t pid : worker_pids_) {
            waitpid(pid, nullptr, 0);
        }
    }
    void stop() { should_stop_ = true; }
};
#
static inline bool parse_size_param(const httplib::Request& req, size_t& size_out) {
    if (!req.has_param("size")) return false;
    try {
        std::string v = req.get_param_value("size");
        if (v.empty()) return false;
        size_t idx = 0;
        unsigned long long parsed = std::stoull(v, &idx, 10);
        if (idx != v.size()) return false;
        size_out = static_cast<size_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}
#
int main() {
    const int NUM_WORKERS = CompressFrontendService::POOL_SIZE;
    PreforkHTTPServer server(NUM_WORKERS);
    if (server.fork_workers()) {
        CompressFrontendService service;
        httplib::Server svr;
        svr.set_keep_alive_max_count(50000);
        svr.set_read_timeout(5);
        svr.set_write_timeout(5);
        svr.set_idle_interval(0, 100000);
        svr.set_payload_max_length(8 * 1024 * 1024);
        svr.Get("/compress", [&](const httplib::Request& req, httplib::Response& res) {
            size_t requested_size = 0;
            if (!parse_size_param(req, requested_size)) {
                res.status = 400;
                res.set_content("missing_or_invalid_size", "text/plain");
                return;
            }
            const std::string& buffer = service.get_buffer();
            if (requested_size > buffer.size()) {
                requested_size = buffer.size();
            }
            std::string slice(buffer.data(), requested_size);
			auto t0 = std::chrono::steady_clock::now();
			std::string compressed = microservice::compression::compress_data(slice);
			auto t1 = std::chrono::steady_clock::now();
#if ENABLE_TIMING
			std::cout << compressed << std::endl;
#endif
			long long latency_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
			size_t compressed_size_bytes = compressed.size();
			if (compressed.size() >= 11 && compressed.substr(0, 11) == "COMPRESSED:") {
				compressed_size_bytes = (compressed.size() - 11) / 2;
			}
			std::string json = std::string("{\"compressed_size\": ") + std::to_string(compressed_size_bytes) +
				", \"compression_latency_us\": " + std::to_string(latency_us) + "}";
			res.set_content(json, "application/json");
        });
        std::cout << "HTTP Worker " << getpid() << " listening on 0.0.0.0:50060" << std::endl;
        svr.listen("0.0.0.0", 50060);
        return 0;
    } else {
        std::cout << "Compress frontend master process started with " << NUM_WORKERS << " HTTP workers" << std::endl;
        server.master_loop();
        return 0;
    }
}


