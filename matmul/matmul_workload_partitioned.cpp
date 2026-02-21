// Partitioned worker: 4 AMX cores, 28 service cores, generator on separate core.
// Run: ./matmul_workload_partitioned [rps] [duration_sec] [use_amx] [M K N]
#include "matmul_partitioned_common.h"
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

struct Job {
  int64_t M, K, N;
};

static constexpr int64_t kMaxDim = 4096;

struct JobQueue {
  std::mutex m;
  std::condition_variable cv_producer;
  std::condition_variable cv_consumer;
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

static void service_worker(JobQueue& job_queue, partitioned::AmxJobQueue& amx_queue,
                          std::atomic<uint64_t>& completed, std::atomic<uint64_t>& total_latency_us,
                          int worker_index, bool use_amx) {
  partitioned::pin_self_to_core(partitioned::SERVICE_CORE_START + worker_index);

  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(-128, 127);

  while (true) {
    Job j;
    if (!job_queue.pop(j)) break;

    size_t szA = static_cast<size_t>(j.M * j.K);
    size_t szB = static_cast<size_t>(j.K * j.N);
    auto A = std::make_shared<std::vector<int8_t>>(szA);
    auto B = std::make_shared<std::vector<int8_t>>(szB);
    for (size_t i = 0; i < szA; ++i) (*A)[i] = static_cast<int8_t>(dist(rng));
    for (size_t i = 0; i < szB; ++i) (*B)[i] = static_cast<int8_t>(dist(rng));

    uint64_t latency_us = 0;
    partitioned::service_matmul_one(j.M, j.K, j.N, A, B, use_amx, amx_queue, &latency_us);

    completed++;
    total_latency_us += latency_us;
  }
}

static void generator(double rps, double duration_sec, JobQueue& queue, Job job_template,
                      std::atomic<uint64_t>& submitted) {
  partitioned::pin_self_to_core(partitioned::GENERATOR_CORE_START);

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
  double t = 0.0;
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
  int use_amx = 1;
  int64_t M = 768, K = 768, N = 768;
  if (argc >= 2) rps = std::atof(argv[1]);
  if (argc >= 3) duration_sec = std::atof(argv[2]);
  if (argc >= 4) use_amx = std::atoi(argv[3]);
  if (argc >= 7) {
    M = std::atol(argv[4]);
    K = std::atol(argv[5]);
    N = std::atol(argv[6]);
  }
  if (rps <= 0 || duration_sec <= 0 || (use_amx != 0 && use_amx != 1) ||
      M <= 0 || K <= 0 || N <= 0 || M > kMaxDim || K > kMaxDim || N > kMaxDim) {
    std::cerr << "Usage: " << (argv[0] ? argv[0] : "matmul_workload_partitioned")
              << " [rps] [duration_sec] [use_amx] [M K N]\n"
              << "  use_amx: 1=service calls AMX cores, 0=service runs SW matmul on itself\n"
              << "  Cores: AMX=" << partitioned::AMX_CORE_START << ".." << (partitioned::AMX_CORE_START + partitioned::NUM_AMX_CORES - 1)
              << " Service=" << partitioned::SERVICE_CORE_START << ".." << (partitioned::SERVICE_CORE_START + partitioned::NUM_SERVICE_CORES - 1)
              << " Generator=" << partitioned::GENERATOR_CORE_START << "\n";
    return 1;
  }

#if defined(__x86_64__) && defined(__linux__)
  if (use_amx && !microservice::matmul::amx_request_perm()) {
    std::cerr << "AMX permission denied; run with use_amx=0 or on AMX-capable hardware.\n";
    return 1;
  }
#endif

  const int queue_cap = 512;
  JobQueue job_queue(queue_cap);
  partitioned::AmxJobQueue amx_queue;
  std::atomic<uint64_t> completed{0};
  std::atomic<uint64_t> total_latency_us{0};
  std::atomic<uint64_t> submitted{0};

  std::cout << "partitioned worker: rps=" << rps << " duration=" << duration_sec << "s use_amx=" << use_amx
            << " M=" << M << " K=" << K << " N=" << N
            << " AMX_cores=" << partitioned::NUM_AMX_CORES << " service_cores=" << partitioned::NUM_SERVICE_CORES << std::endl;

  decision::init_hw_counters_master();
  decision::start_global_hw_logger_to_file("matmul_ring.log");

  std::vector<std::thread> amx_threads;
  for (int i = 0; i < partitioned::NUM_AMX_CORES; ++i)
    amx_threads.emplace_back(partitioned::amx_worker, std::ref(amx_queue), i);

  std::vector<std::thread> service_threads;
  for (int i = 0; i < partitioned::NUM_SERVICE_CORES; ++i)
    service_threads.emplace_back(service_worker, std::ref(job_queue), std::ref(amx_queue),
                                  std::ref(completed), std::ref(total_latency_us), i, use_amx != 0);

  std::thread gen_thread(generator, rps, duration_sec, std::ref(job_queue), Job{M, K, N}, std::ref(submitted));
  gen_thread.join();

  job_queue.shutdown();
  for (auto& t : service_threads) t.join();
  amx_queue.shutdown();
  for (auto& t : amx_threads) t.join();

  uint64_t c = completed.load();
  uint64_t tot = total_latency_us.load();
  std::cout << "submitted=" << submitted.load() << " completed=" << c
            << " avg_latency_us=" << (c ? (tot / c) : 0) << std::endl;
  return 0;
}
