// Shared partition logic: core layout, AMX queue, amx_worker, service_matmul_one.
// Used by matmul_workload_partitioned (worker) and matmul_server_partitioned (server).
#pragma once
#include "matmul_utils.h"
#include "decision.h"
#include "metrics.h"
#include <cstdint>
#include <memory>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace partitioned {

// Core partition: 4 AMX, 28 service, generator/acceptor on separate set.
static constexpr int AMX_CORE_START = 0;
static constexpr int NUM_AMX_CORES = 4;
static constexpr int SERVICE_CORE_START = 4;
static constexpr int NUM_SERVICE_CORES = 28;
static constexpr int GENERATOR_CORE_START = 32;

#if defined(__linux__)
inline void pin_self_to_core(int core_id) {
  if (core_id < 0) return;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  (void)pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
}
#else
inline void pin_self_to_core(int core_id) { (void)core_id; }
#endif

struct AmxJob {
  int64_t M, K, N;
  std::shared_ptr<std::vector<int8_t>> A;
  std::shared_ptr<std::vector<int8_t>> B;
  std::shared_ptr<std::promise<uint64_t>> latency_promise;
};

struct AmxJobQueue {
  std::mutex m;
  std::condition_variable cv;
  std::queue<AmxJob> q;
  std::atomic<bool> done{false};

  void push(AmxJob j) {
    std::lock_guard<std::mutex> lock(m);
    if (done) return;
    q.push(std::move(j));
    cv.notify_one();
  }

  bool pop(AmxJob& j) {
    std::unique_lock<std::mutex> lock(m);
    while (q.empty() && !done)
      cv.wait(lock);
    if (done && q.empty()) return false;
    if (q.empty()) return false;
    j = std::move(q.front());
    q.pop();
    return true;
  }

  void shutdown() {
    done = true;
    cv.notify_all();
  }
};

// AMX worker (cores 0-3): blackbox—no control logic, no stats. Just pop and run.
inline void amx_worker(AmxJobQueue& amx_queue, int worker_index) {
  pin_self_to_core(AMX_CORE_START + worker_index);
  while (true) {
    AmxJob j;
    if (!amx_queue.pop(j)) break;
    uint64_t latency_us = 0;
    microservice::matmul::amx_matmul_impl(j.M, j.K, j.N, j.A->data(), j.B->data(), &latency_us);
    if (j.latency_promise)
      j.latency_promise->set_value(latency_us);
  }
}

// Service path (cores 4-30): all stats before submission and after return. Cores 0-3 are a blackbox.
inline void service_matmul_one(int64_t M, int64_t K, int64_t N,
                               const std::shared_ptr<std::vector<int8_t>>& A,
                               const std::shared_ptr<std::vector<int8_t>>& B,
                               bool use_amx, AmxJobQueue& amx_queue, uint64_t* out_latency_us) {
  uint64_t latency_us = 0;
  decision::ExecuteOp op = [&](const std::string&, decision::HwSwPath path, uint64_t* out_lat) {
    if (path == decision::HwSwPath::Hardware) {
      auto t0 = std::chrono::steady_clock::now();
      auto prom = std::make_shared<std::promise<uint64_t>>();
      std::future<uint64_t> fut = prom->get_future();
      amx_queue.push(AmxJob{M, K, N, A, B, prom});
      (void)fut.get();
      uint64_t e2e_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - t0).count());
      if (out_lat) *out_lat = e2e_us;
      return std::string("OK");
    } else {
      uint64_t lat = 0;
      microservice::matmul::sw_matmul_impl(M, K, N, A->data(), B->data(), &lat);
      if (out_lat) *out_lat = lat;
      return std::string("OK");
    }
  };
  (void)decision::execute_with_path(std::string(""), use_amx ? decision::HwSwPath::Hardware : decision::HwSwPath::Software, op, &latency_us);
  if (out_latency_us) *out_latency_us = latency_us;
}

} // namespace partitioned
