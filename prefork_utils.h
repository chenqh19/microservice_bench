#pragma once

#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <errno.h>

static inline bool read_exact_fd(int fd, void* buf, size_t nbytes) {
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

static inline bool write_exact_fd(int fd, const void* buf, size_t nbytes) {
    const char* p = static_cast<const char*>(buf);
    size_t total = 0;
    while (total < nbytes) {
        ssize_t w = write(fd, p + total, nbytes - total);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        total += static_cast<size_t>(w);
    }
    return true;
}

class PreforkServer {
private:
    int server_fd_;
    int num_workers_;
    std::vector<pid_t> worker_pids_;
    bool should_stop_;

public:
    PreforkServer(int NUM_WORKERS = 16) : num_workers_(NUM_WORKERS), should_stop_(false) {
        // Set up signal handlers for graceful shutdown
        signal(SIGTERM, [](int) { /* handled in main loop */ });
        signal(SIGINT, [](int) { /* handled in main loop */ });
    }

    ~PreforkServer() {
        if (server_fd_ >= 0) {
            close(server_fd_);
        }
    }

    // Set up the server socket
    bool setup_socket(const char* socket_path) {
        unlink(socket_path); // Remove if exists
        
        server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            perror("socket");
            return false;
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
        
        if (bind(server_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            close(server_fd_);
            return false;
        }

        chmod(socket_path, 0777); // Ensure world-writable for Docker
        
        if (listen(server_fd_, 8192) < 0) {
            perror("listen");
            close(server_fd_);
            return false;
        }

        return true;
    }

    // Fork worker processes
    bool fork_workers() {
        std::cout << "Starting " << num_workers_ << " worker processes..." << std::endl;
        
        for (int i = 0; i < num_workers_; ++i) {
            pid_t pid = fork();
            
            if (pid == 0) {
                // Worker process
                std::cout << "Worker process " << getpid() << " started" << std::endl;
                return true; // Return true to indicate this is a worker
            } else if (pid > 0) {
                // Master process
                worker_pids_.push_back(pid);
            } else {
                // Fork failed
                perror("fork");
                return false;
            }
        }

        // Master process continues here
        std::cout << "Master process " << getpid() << " started " << worker_pids_.size() << " workers" << std::endl;
        return false; // Return false to indicate this is the master
    }

    // Master process main loop
    void master_loop() {
        std::cout << "Master process waiting for workers..." << std::endl;
        
        while (!should_stop_) {
            // Check if any workers have died
            int status;
            pid_t dead_pid = waitpid(-1, &status, WNOHANG);
            
            if (dead_pid > 0) {
                std::cout << "Worker " << dead_pid << " died, restarting..." << std::endl;
                
                // Remove from our list
                worker_pids_.erase(
                    std::remove(worker_pids_.begin(), worker_pids_.end(), dead_pid),
                    worker_pids_.end()
                );
                
                // Fork a new worker
                pid_t new_pid = fork();
                if (new_pid == 0) {
                    // New worker process
                    std::cout << "Restarted worker process " << getpid() << std::endl;
                    return; // Return to indicate this is a worker
                } else if (new_pid > 0) {
                    // Master process
                    worker_pids_.push_back(new_pid);
                }
            }
            
            // Sleep a bit to avoid busy waiting
            usleep(100000); // 100ms
        }
        
        // Graceful shutdown
        std::cout << "Master process shutting down workers..." << std::endl;
        for (pid_t pid : worker_pids_) {
            kill(pid, SIGTERM);
        }
        
        // Wait for all workers to exit
        for (pid_t pid : worker_pids_) {
            waitpid(pid, nullptr, 0);
        }
    }

    // Get server file descriptor (for workers)
    int get_server_fd() const { return server_fd_; }

    // Set stop flag for graceful shutdown
    void stop() { should_stop_ = true; }
};

// Worker process main loop template
template<typename ServiceType, typename RequestType, typename ResponseType>
void worker_loop(int server_fd, ServiceType& service, Ser1de_re& ser1de) {
    std::cout << "Worker " << getpid() << " ready to accept connections" << std::endl;
    
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, continue
                continue;
            }
            perror("accept");
            break;
        }

        // Handle the client
        uint32_t msg_len = 0;
        if (!read_exact_fd(client_fd, &msg_len, 4)) { close(client_fd); continue; }
        std::vector<char> buf(msg_len);
        if (msg_len > 0 && !read_exact_fd(client_fd, buf.data(), msg_len)) { close(client_fd); continue; }
        
        RequestType request;
        bool ok = microservice::utils::deserialize_message(ser1de, std::string(buf.begin(), buf.end()), request);
        if (ok) {
            auto response = service.process_request(request);
            std::string resp_str = microservice::utils::serialize_message(ser1de, response);
            uint32_t resp_len = resp_str.size();
            if (!write_exact_fd(client_fd, &resp_len, 4)) { close(client_fd); continue; }
            (void)write_exact_fd(client_fd, resp_str.data(), resp_len);
        }
        close(client_fd);
    }
} 