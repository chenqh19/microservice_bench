#pragma once
#include "config.h"
#include "decision.h"
#include "utils/compression_utils.h"
#include <string>
#include <chrono>
#include <algorithm>
#include <vector>
#include <iostream>
#include <atomic>
#include <thread>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>
#
namespace decision {
#
// ----------------------------------------------------------------------------
// Cross-worker hardware submission counter using POSIX shared memory
// ----------------------------------------------------------------------------
namespace {
static constexpr const char* SHM_NAME = "/compression_hw_counters_v1";

struct SharedCounters {
	alignas(64) uint64_t total; // global total submissions
};

static SharedCounters* g_shared = nullptr;
static int g_shm_fd = -1;

inline void map_shared(bool create) {
	if (g_shared) return;
	int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;
	g_shm_fd = shm_open(SHM_NAME, flags, 0666);
	if (g_shm_fd < 0) {
		return;
	}
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
} // anonymous namespace

inline void init_hw_counters_master() {
	map_shared(true);
	if (!g_shared) return;
	std::memset(g_shared, 0, sizeof(*g_shared));
	g_shared->total = 0;
}

inline void init_hw_counters_worker() {
	map_shared(false);
	if (!g_shared) return;
	// Nothing else to do for single global counter
}

inline void record_hw_submission() {
	if (!g_shared) {
		init_hw_counters_worker();
	}
	if (g_shared) {
		// Atomic fetch_add on shared memory global counter
		__atomic_fetch_add(&g_shared->total, 1ULL, __ATOMIC_RELAXED);
	}
}

inline void start_global_hw_logger() {
	static std::atomic<bool> started{false};
	bool expected = false;
	if (started.compare_exchange_strong(expected, true)) {
		std::thread([](){
			uint64_t last = 0;
			for (;;) {
				std::this_thread::sleep_for(std::chrono::seconds(10));
				if (!g_shared) { map_shared(false); }
				if (!g_shared) continue;
				// Read total without locking; compute delta since last print
				uint64_t current = __atomic_load_n(&g_shared->total, __ATOMIC_RELAXED);
				uint64_t delta = current - last;
				last = current;
				std::cout << "HW submissions (all workers) in last 10s: " << delta << std::endl;
			}
		}).detach();
	}
}


inline void compress_collect(const std::string& slice,
	std::vector<std::string>& compressed_out,
	long long& latency_us) {
	bool use_hw = should_use_hardware_for_request(slice.size());
	const size_t kChunkSize = 32 * 1024;
	latency_us = 0;
	compressed_out.clear();
	if (use_hw) {
		record_hw_submission();
		auto t0 = std::chrono::steady_clock::now();
		std::string compressed = compress_with_path(slice, qpl_path_hardware);
		auto t1 = std::chrono::steady_clock::now();
#if ENABLE_TIMING
		std::cout << compressed << std::endl;
#endif
		latency_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
		compressed_out.push_back(std::move(compressed));
	} else {
		for (size_t offset = 0; offset < slice.size(); offset += kChunkSize) {
			size_t len = std::min(kChunkSize, slice.size() - offset);
			auto t0 = std::chrono::steady_clock::now();
			std::string compressed = compress_with_path(
				std::string(slice.data() + offset, len), qpl_path_software);
			auto t1 = std::chrono::steady_clock::now();
#if ENABLE_TIMING
			std::cout << compressed << std::endl;
#endif
			latency_us += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
			compressed_out.push_back(std::move(compressed));
		}
	}
}
#
} // namespace decision


