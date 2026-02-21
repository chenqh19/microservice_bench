// Thread pool + workload generator at fixed RPS with exponential inter-arrival.
// No TCP. Run: ./matmul_workload [rps] [duration_sec] [num_workers] [M] [K] [N]
// Example: ./matmul_workload 2 60 4 768 768 768  -> 2 req/s for 60s, 4 workers, 768^3 matmul
#include "matmul_utils.h"
#include "metrics.h"
#include <iostream>
#include <vector>
#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
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

struct Job {
  int64_t M, K, N;
};

static constexpr int64_t kMaxDim = 4096;

// Bounded queue for jobs.
struct JobQueue {
  std::mutex m;
  std::condition_variable cv_producer;  // not full
  std::condition_variable cv_consumer;  // not empty
  std::queue<Job> q;
  int max_size;
  std::atomic<bool> done{false};

  explicit JobQueue(int cap) : max_size(cap) {}

  bool push(Job j) {
    std::unique_lock<std::mutex> lock(m);
    while (q.size() >= static_cast<size_t>(max_size) && !done)
      cv_producer.wait(lock);
    if (done) return false;
    q.push(j);
    cv_consumer.notify_one();
    return true;
  }

  bool pop(Job& j) {
    std::unique_lock<std::mutex> lock(m);
    while (q.empty() && !done)
      cv_consumer.wait(lock);
    if (done && q.empty()) return false;
    if (q.empty()) return false;
    j = q.front();
    q.pop();
    cv_producer.notify_one();
    return true;
  }

  void shutdown() {
    done = true;
    cv_consumer.notify_all();
    cv_producer.notify_all();
  }
};

static void worker(JobQueue& queue, std::atomic<uint64_t>& completed, std::atomic<uint64_t>& total_latency_us, int worker_index) {
  int num_cpus = static_cast<int>(std::thread::hardware_concurrency());
  if (num_cpus > 0)
    pin_self_to_core(worker_index % num_cpus);

  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(-128, 127);
  while (true) {
    Job j;
    if (!queue.pop(j)) break;
    decision::record_hw_submission();
    auto key = decision::record_hw_start_get_inflight_after();
    size_t szA = static_cast<size_t>(j.M * j.K);
    size_t szB = static_cast<size_t>(j.K * j.N);
    std::vector<int8_t> A(szA), B(szB);
    for (size_t i = 0; i < szA; ++i) A[i] = static_cast<int8_t>(dist(rng));
    for (size_t i = 0; i < szB; ++i) B[i] = static_cast<int8_t>(dist(rng));
    uint64_t latency_us = 0;
    microservice::matmul::amx_matmul_impl(j.M, j.K, j.N, A.data(), B.data(), &latency_us);
    decision::record_hw_finish(key, latency_us);
    completed++;
    total_latency_us += latency_us;
  }
}

// Generate exactly N = round(rps * duration_sec) jobs with exponential-like spacing:
// draw N inter-arrivals from exponential(rps), scale so they sum to duration_sec, then
// schedule each submission at the scaled cumulative time. Zero variance in submitted count.
static void generator(double rps, double duration_sec, JobQueue& queue, Job job_template,
                      std::atomic<uint64_t>& submitted) {
  const size_t N = static_cast<size_t>(std::round(rps * duration_sec));
  if (N == 0) return;
  std::random_device rd;
  std::mt19937 rng(rd());
  std::exponential_distribution<double> exp_dist(rps);
  std::vector<double> gaps(N);
  double sum = 0.0;
  for (size_t i = 0; i < N; ++i) {
    gaps[i] = exp_dist(rng);
    sum += gaps[i];
  }
  if (sum <= 0.0) sum = 1.0;
  const double scale = duration_sec / sum;
  auto start = std::chrono::steady_clock::now();
  double t = 0.0;  // next target time from start
  for (size_t i = 0; i < N; ++i) {
    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    double to_sleep = t - elapsed;
    if (to_sleep > 1e-9)
      std::this_thread::sleep_for(std::chrono::duration<double>(to_sleep));
    if (!queue.push(job_template)) break;
    submitted++;
    t += gaps[i] * scale;
  }
}

int main(int argc, char** argv) {
  double rps = 2.0;
  double duration_sec = 60.0;
  int num_workers = 4;
  int64_t M = 768, K = 768, N = 768;
  if (argc >= 2) rps = std::atof(argv[1]);
  if (argc >= 3) duration_sec = std::atof(argv[2]);
  if (argc >= 4) num_workers = std::atoi(argv[3]);
  if (argc >= 7) {
    M = std::atol(argv[4]);
    K = std::atol(argv[5]);
    N = std::atol(argv[6]);
  }
  if (rps <= 0 || duration_sec <= 0 || num_workers <= 0 ||
      M <= 0 || K <= 0 || N <= 0 || M > kMaxDim || K > kMaxDim || N > kMaxDim) {
    std::cerr << "Usage: " << (argv[0] ? argv[0] : "matmul_workload")
              << " [rps] [duration_sec] [num_workers] [M K N]\n"
              << "  Default: rps=2 duration=60 workers=4 M=K=N=768\n";
    return 1;
  }

  const int queue_cap = 512;
  JobQueue queue(queue_cap);
  std::atomic<uint64_t> completed{0};
  std::atomic<uint64_t> total_latency_us{0};
  std::atomic<uint64_t> submitted{0};

  std::cout << "rps=" << rps << " duration=" << duration_sec << "s workers=" << num_workers
            << " M=" << M << " K=" << K << " N=" << N << std::endl;

  decision::init_hw_counters_master();
  decision::start_global_hw_logger_to_file("matmul_ring.log");

  std::vector<std::thread> workers;
  for (int i = 0; i < num_workers; ++i)
    workers.emplace_back(worker, std::ref(queue), std::ref(completed), std::ref(total_latency_us), i);

  std::thread gen_thread(generator, rps, duration_sec, std::ref(queue), Job{M, K, N}, std::ref(submitted));
  gen_thread.join();

  queue.shutdown();
  for (auto& t : workers) t.join();

  uint64_t c = completed.load();
  uint64_t tot = total_latency_us.load();
  std::cout << "submitted=" << submitted.load() << " completed=" << c
            << " avg_latency_us=" << (c ? (tot / c) : 0) << std::endl;
  return 0;
}
