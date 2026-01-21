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
#include <sys/file.h>
#include <sys/stat.h>
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
static constexpr const char* LOCK_PATH = "/tmp/compression_hw_counters.lock";
static constexpr uint32_t SHM_MAGIC = 0x48445743; // 'H''D''W''C'
static constexpr uint32_t NUM_SLOTS = 512;

struct SharedSlot {
	pid_t pid;
	uint64_t count;
};

struct SharedCounters {
	uint32_t magic;
	uint32_t num_slots;
	SharedSlot slots[NUM_SLOTS];
};

static SharedCounters* g_shared = nullptr;
static int g_shm_fd = -1;
static int g_lock_fd = -1;
thread_local int g_slot_index = -1;

inline void ensure_lock_file() {
	if (g_lock_fd >= 0) return;
	g_lock_fd = open(LOCK_PATH, O_CREAT | O_RDWR, 0666);
}

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
	ensure_lock_file();
	map_shared(true);
	if (!g_shared) return;
	flock(g_lock_fd, LOCK_EX);
	std::memset(g_shared, 0, sizeof(*g_shared));
	g_shared->magic = SHM_MAGIC;
	g_shared->num_slots = NUM_SLOTS;
	flock(g_lock_fd, LOCK_UN);
}

inline void init_hw_counters_worker() {
	ensure_lock_file();
	map_shared(false);
	if (!g_shared) return;
	// Claim a slot for this worker pid
	flock(g_lock_fd, LOCK_EX);
	int free_idx = -1;
	pid_t me = getpid();
	for (uint32_t i = 0; i < NUM_SLOTS; ++i) {
		if (g_shared->slots[i].pid == me) { g_slot_index = static_cast<int>(i); break; }
		if (free_idx == -1 && g_shared->slots[i].pid == 0) free_idx = static_cast<int>(i);
	}
	if (g_slot_index == -1 && free_idx != -1) {
		g_shared->slots[free_idx].pid = me;
		g_shared->slots[free_idx].count = 0;
		g_slot_index = free_idx;
	}
	flock(g_lock_fd, LOCK_UN);
}

inline void record_hw_submission() {
	if (!g_shared || g_slot_index < 0) {
		init_hw_counters_worker();
	}
	if (g_shared && g_slot_index >= 0) {
		// Only this worker writes its own slot; no locking needed
		++g_shared->slots[g_slot_index].count;
	}
}

inline void start_global_hw_logger() {
	static std::atomic<bool> started{false};
	bool expected = false;
	if (started.compare_exchange_strong(expected, true)) {
		std::thread([](){
			for (;;) {
				std::this_thread::sleep_for(std::chrono::seconds(10));
				if (!g_shared) { map_shared(false); }
				if (!g_shared) continue;
				uint64_t total = 0;
				ensure_lock_file();
				flock(g_lock_fd, LOCK_EX);
				for (uint32_t i = 0; i < NUM_SLOTS; ++i) {
					total += g_shared->slots[i].count;
					g_shared->slots[i].count = 0;
				}
				flock(g_lock_fd, LOCK_UN);
				std::cout << "HW submissions (all workers) in last 10s: " << total << std::endl;
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


