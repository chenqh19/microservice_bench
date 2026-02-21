// TCP server: accepts connections, each sends one line "M,K,N", server runs matmul and replies "OK <latency_us>".
// Threading: one acceptor thread + N worker threads. Each worker handles one client at a time (AMX is per-thread).
// Run: ./matmul_server [port] [num_workers]. Default: port 50061, 4 workers.
// Send request: echo "768,768,768" | nc localhost 50061
#include "matmul_utils.h"
#include "metrics.h"
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

#if defined(__linux__)
static void pin_self_to_core(int core_id) {
  if (core_id < 0) return;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) == 0)
    std::cout << "[worker] pinned to core " << core_id << std::endl;
}
#else
static void pin_self_to_core(int core_id) { (void)core_id; }
#endif

static constexpr int64_t kMaxDim = 4096;
static constexpr int kDefaultPort = 50061;
static constexpr int kDefaultWorkers = 4;
static constexpr int kQueueMax = 256;

struct Queue {
  std::mutex m;
  std::condition_variable cv;
  std::queue<int> q;
  std::atomic<bool> done{false};

  bool pop(int& fd) {
    std::unique_lock<std::mutex> lock(m);
    while (q.empty() && !done) cv.wait(lock);
    if (done && q.empty()) return false;
    fd = q.front();
    q.pop();
    return true;
  }

  void push(int fd) {
    std::lock_guard<std::mutex> lock(m);
    if (q.size() < static_cast<size_t>(kQueueMax)) {
      q.push(fd);
      cv.notify_one();
    } else {
      close(fd);
    }
  }

  void shutdown() {
    done = true;
    cv.notify_all();
  }
};

static void worker(Queue& queue, int worker_index) {
  int num_cpus = static_cast<int>(std::thread::hardware_concurrency());
  if (num_cpus > 0)
    pin_self_to_core(worker_index % num_cpus);

  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(-128, 127);
  while (true) {
    int client_fd = -1;
    if (!queue.pop(client_fd)) break;
    char buf[64];
    ssize_t n = 0;
    for (size_t i = 0; i < sizeof(buf) - 1; ) {
      ssize_t r = read(client_fd, buf + i, 1);
      if (r <= 0) break;
      if (buf[i] == '\n') { buf[i] = '\0'; n = static_cast<ssize_t>(i); break; }
      i++;
    }
    if (n <= 0) {
      const char* err = "ERR read\n";
      (void)write(client_fd, err, strlen(err));
      close(client_fd);
      continue;
    }
    int64_t M = 0, K = 0, N = 0;
    if (!microservice::matmul::parse_matmul_dims(std::string(buf), M, K, N, kMaxDim)) {
      const char* err = "ERR invalid_dims\n";
      (void)write(client_fd, err, strlen(err));
      close(client_fd);
      continue;
    }
    decision::record_hw_submission();
    auto key = decision::record_hw_start_get_inflight_after();
    size_t szA = static_cast<size_t>(M * K);
    size_t szB = static_cast<size_t>(K * N);
    std::vector<int8_t> A(szA), B(szB);
    for (size_t i = 0; i < szA; ++i) A[i] = static_cast<int8_t>(dist(rng));
    for (size_t i = 0; i < szB; ++i) B[i] = static_cast<int8_t>(dist(rng));
    uint64_t latency_us = 0;
    std::string result = microservice::matmul::amx_matmul_impl(M, K, N, A.data(), B.data(), &latency_us);
    decision::record_hw_finish(key, latency_us);
    std::string reply = (result == "OK") ? ("OK " + std::to_string(latency_us) + "\n") : "ERR " + result + "\n";
    (void)write(client_fd, reply.c_str(), reply.size());
    close(client_fd);
  }
}

int main(int argc, char** argv) {
  int port = kDefaultPort;
  int num_workers = kDefaultWorkers;
  if (argc >= 2) port = std::atoi(argv[1]);
  if (argc >= 3) num_workers = std::atoi(argv[2]);
  if (num_workers <= 0) num_workers = 1;

  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    perror("socket");
    return 1;
  }
  int opt = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (bind(listen_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
    perror("bind");
    close(listen_fd);
    return 1;
  }
  if (listen(listen_fd, 64) < 0) {
    perror("listen");
    close(listen_fd);
    return 1;
  }
  std::cout << "matmul_server listening on 0.0.0.0:" << port << " workers=" << num_workers << std::endl;

  decision::init_hw_counters_master();
  decision::start_global_hw_logger_to_file("matmul_ring.log");

  Queue client_queue;
  std::vector<std::thread> worker_threads;
  for (int i = 0; i < num_workers; ++i)
    worker_threads.emplace_back(worker, std::ref(client_queue), i);

  while (true) {
    int client_fd = accept(listen_fd, nullptr, nullptr);
    if (client_fd < 0) {
      perror("accept");
      break;
    }
    client_queue.push(client_fd);
  }
  client_queue.shutdown();
  for (auto& t : worker_threads) t.join();
  close(listen_fd);
  return 0;
}
