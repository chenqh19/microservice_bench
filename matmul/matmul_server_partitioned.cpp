// Partitioned TCP server: 4 AMX cores, 28 service cores, acceptor on separate core.
// Run: ./matmul_server_partitioned [port] [use_amx]. Default: port 50061, use_amx=1.
// Send request: echo "768,768,768" | nc localhost 50061
#include "matmul_partitioned_common.h"
#include "matmul_utils.h"
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

static constexpr int64_t kMaxDim = 4096;
static constexpr int kDefaultPort = 50061;
static constexpr int kQueueMax = 256;

struct ClientQueue {
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

static void service_worker(ClientQueue& client_queue, partitioned::AmxJobQueue& amx_queue,
                          int worker_index, bool use_amx) {
  partitioned::pin_self_to_core(partitioned::SERVICE_CORE_START + worker_index);

  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(-128, 127);

  while (true) {
    int client_fd = -1;
    if (!client_queue.pop(client_fd)) break;

    char buf[64];
    ssize_t n = 0;
    for (size_t i = 0; i < static_cast<ssize_t>(sizeof(buf)) - 1; ) {
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

    size_t szA = static_cast<size_t>(M * K);
    size_t szB = static_cast<size_t>(K * N);
    auto A = std::make_shared<std::vector<int8_t>>(szA);
    auto B = std::make_shared<std::vector<int8_t>>(szB);
    for (size_t i = 0; i < szA; ++i) (*A)[i] = static_cast<int8_t>(dist(rng));
    for (size_t i = 0; i < szB; ++i) (*B)[i] = static_cast<int8_t>(dist(rng));

    uint64_t latency_us = 0;
    partitioned::service_matmul_one(M, K, N, A, B, use_amx, amx_queue, &latency_us);

    std::string reply = "OK " + std::to_string(latency_us) + "\n";
    (void)write(client_fd, reply.c_str(), reply.size());
    close(client_fd);
  }
}

static void acceptor(int listen_fd, ClientQueue& client_queue) {
  partitioned::pin_self_to_core(partitioned::GENERATOR_CORE_START);
  while (true) {
    int client_fd = accept(listen_fd, nullptr, nullptr);
    if (client_fd < 0) {
      perror("accept");
      break;
    }
    client_queue.push(client_fd);
  }
}

int main(int argc, char** argv) {
  int port = kDefaultPort;
  int use_amx = 1;
  if (argc >= 2) port = std::atoi(argv[1]);
  if (argc >= 3) use_amx = std::atoi(argv[2]);
  if (use_amx != 0 && use_amx != 1) use_amx = 1;

#if defined(__x86_64__) && defined(__linux__)
  if (use_amx && !microservice::matmul::amx_request_perm()) {
    std::cerr << "AMX permission denied; run with use_amx=0 or on AMX-capable hardware.\n";
    return 1;
  }
#endif

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

  std::cout << "matmul_server_partitioned listening on 0.0.0.0:" << port
            << " use_amx=" << use_amx
            << " AMX_cores=" << partitioned::NUM_AMX_CORES
            << " service_cores=" << partitioned::NUM_SERVICE_CORES << std::endl;

  decision::init_hw_counters_master();
  decision::start_global_hw_logger_to_file("matmul_ring.log");

  ClientQueue client_queue;
  partitioned::AmxJobQueue amx_queue;

  std::vector<std::thread> amx_threads;
  for (int i = 0; i < partitioned::NUM_AMX_CORES; ++i)
    amx_threads.emplace_back(partitioned::amx_worker, std::ref(amx_queue), i);

  std::vector<std::thread> service_threads;
  for (int i = 0; i < partitioned::NUM_SERVICE_CORES; ++i)
    service_threads.emplace_back(service_worker, std::ref(client_queue), std::ref(amx_queue), i, use_amx != 0);

  std::thread acceptor_thread(acceptor, listen_fd, std::ref(client_queue));
  acceptor_thread.join();

  client_queue.shutdown();
  for (auto& t : service_threads) t.join();
  amx_queue.shutdown();
  for (auto& t : amx_threads) t.join();
  close(listen_fd);
  return 0;
}
