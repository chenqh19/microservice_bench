#pragma once
#include "regression.h"
#include <cstdint>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#
namespace decision {
#
namespace {
static constexpr const char* SHM_NAME = "/compression_hw_counters_v5";
#
// Key at submission time: (inflight count, current task size)
struct HwSubmitKey {
	uint64_t inflight_count;
	uint64_t current_task_size;   // bytes
};
#
struct SharedCounters {
	alignas(64) uint64_t total;      // total HW submissions since start
	alignas(64) uint64_t inflight;   // current in-flight HW operations
	alignas(64) uint64_t ring_write_index;
	static constexpr uint32_t RING_CAPACITY = 1024;
	alignas(64) uint64_t latencies[RING_CAPACITY]; // us
	alignas(64) uint64_t inflight_at_submit[RING_CAPACITY];
	alignas(64) uint64_t current_task_size_at_submit[RING_CAPACITY];  // bytes
};
#
static SharedCounters* g_shared = nullptr;
static int g_shm_fd = -1;
#
inline void map_shared(bool create) {
	if (g_shared) return;
	int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;
	g_shm_fd = shm_open(SHM_NAME, flags, 0666);
	if (g_shm_fd < 0) return;
	if (create) {
		ftruncate(g_shm_fd, sizeof(SharedCounters));
	}
	void* ptr = mmap(nullptr, sizeof(SharedCounters), PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd, 0);
	if (ptr == MAP_FAILED) {
		close(g_shm_fd);
		g_shm_fd = -1;
		return;
	}
	g_shared = reinterpret_cast<SharedCounters*>(ptr);
}
} // anon
#
inline void init_hw_counters_master() {
	map_shared(true);
	if (!g_shared) return;
	std::memset(g_shared, 0, sizeof(*g_shared));
}

// Call in master process after fork so it re-maps the shm and sees workers' updates.
// Before fork the master had the only mapping; workers inherit CoW copies, so the
// master's view of ring_write_index etc. never advanced.
inline void reattach_shm_after_fork() {
	if (!g_shared) return;
	munmap(g_shared, sizeof(SharedCounters));
	if (g_shm_fd >= 0) { close(g_shm_fd); g_shm_fd = -1; }
	g_shared = nullptr;
	map_shared(false);
}
#
inline void init_hw_counters_worker() {
	map_shared(false);
}
#
inline void record_hw_submission() {
	if (!g_shared) init_hw_counters_worker();
	if (g_shared) {
		__atomic_fetch_add(&g_shared->total, 1ULL, __ATOMIC_RELAXED);
	}
}
#
inline void record_hw_start() {
	if (!g_shared) init_hw_counters_worker();
	if (g_shared) {
		__atomic_fetch_add(&g_shared->inflight, 1ULL, __ATOMIC_RELAXED);
	}
}
#
// Returns submission-time key (inflight count, current task size) after adding this task.
inline HwSubmitKey record_hw_start_get_inflight_after(uint64_t current_task_size) {
	HwSubmitKey key{0, current_task_size};
	if (!g_shared) init_hw_counters_worker();
	if (!g_shared) return key;
	uint64_t before_inflight = __atomic_fetch_add(&g_shared->inflight, 1ULL, __ATOMIC_RELAXED);
	key.inflight_count = before_inflight + 1;
	return key;
}
#
inline void record_hw_finish(const HwSubmitKey& key, uint64_t latency_us) {
	if (!g_shared) return;
	__atomic_fetch_sub(&g_shared->inflight, 1ULL, __ATOMIC_RELAXED);
	uint64_t idx = __atomic_fetch_add(&g_shared->ring_write_index, 1ULL, __ATOMIC_RELAXED);
	uint64_t pos = idx % SharedCounters::RING_CAPACITY;
	g_shared->latencies[pos] = latency_us;
	g_shared->inflight_at_submit[pos] = key.inflight_count;
	g_shared->current_task_size_at_submit[pos] = key.current_task_size;
}

inline void record_hw_finish_nosample(uint64_t current_task_size) {
	if (!g_shared) return;
	__atomic_fetch_sub(&g_shared->inflight, 1ULL, __ATOMIC_RELAXED);
}
#
inline void start_global_hw_logger() {
	static std::atomic<bool> started{false};
	bool expected = false;
	if (started.compare_exchange_strong(expected, true)) {
		std::thread([](){
			uint64_t last_total = 0;
			for (;;) {
				std::this_thread::sleep_for(std::chrono::seconds(10));
				if (!g_shared) { map_shared(false); }
				if (!g_shared) continue;
				// Print total delta
				uint64_t current_total = __atomic_load_n(&g_shared->total, __ATOMIC_RELAXED);
				uint64_t delta_total = current_total - last_total;
				last_total = current_total;
				std::cout << "HW submissions (all workers) in last 10s: " << delta_total << std::endl;
				// Read ring for regression (x1=inflight_at_submit, x2=task_size_bytes, y=latency_us)
				uint64_t current_ring = __atomic_load_n(&g_shared->ring_write_index, __ATOMIC_ACQUIRE);
				uint64_t available = (current_ring >= SharedCounters::RING_CAPACITY)
					? SharedCounters::RING_CAPACITY
					: static_cast<uint64_t>(current_ring);
				uint64_t start = (current_ring >= SharedCounters::RING_CAPACITY)
					? (current_ring - SharedCounters::RING_CAPACITY)
					: 0;
				if (available >= 3) {
					std::vector<uint64_t> x1_vals, x2_vals, y_vals;
					x1_vals.reserve(available);
					x2_vals.reserve(available);
					y_vals.reserve(available);
					for (uint64_t idx = start; idx < start + available; ++idx) {
						uint64_t pos = idx % SharedCounters::RING_CAPACITY;
						x1_vals.push_back(__atomic_load_n(&g_shared->inflight_at_submit[pos], __ATOMIC_ACQUIRE));
						x2_vals.push_back(__atomic_load_n(&g_shared->current_task_size_at_submit[pos], __ATOMIC_ACQUIRE));
						y_vals.push_back(__atomic_load_n(&g_shared->latencies[pos], __ATOMIC_ACQUIRE));
					}
					// x1 -> y
					LinearFit fit1 = linear_regression(x1_vals.data(), nullptr, nullptr, y_vals.data(), available, 1);
					if (fit1.valid)
						std::cout << "HW regression x1->y: latency_us ~ " << fit1.intercept << " + " << fit1.coef[0] << "*x1  (x1=inflight_at_submit, R²=" << fit1.r_squared << ", n=" << available << ")" << std::endl;
					// x2 -> y
					LinearFit fit2 = linear_regression(x2_vals.data(), nullptr, nullptr, y_vals.data(), available, 1);
					if (fit2.valid)
						std::cout << "HW regression x2->y: latency_us ~ " << fit2.intercept << " + " << fit2.coef[0] << "*x2  (x2=task_size_bytes, R²=" << fit2.r_squared << ", n=" << available << ")" << std::endl;
					// (x1,x2) -> y
					LinearFit fit12 = linear_regression(x1_vals.data(), x2_vals.data(), nullptr, y_vals.data(), available, 2);
					if (fit12.valid)
						std::cout << "HW regression (x1,x2)->y: latency_us ~ " << fit12.intercept << " + " << fit12.coef[0] << "*x1 + " << fit12.coef[1] << "*x2  (R²=" << fit12.r_squared << ", n=" << available << ")" << std::endl;
				}
			}
		}).detach();
	}
}
#
} // namespace decision


