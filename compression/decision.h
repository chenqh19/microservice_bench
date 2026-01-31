#pragma once
#include "config.h"
#include <cstdint>
#include <chrono>
#include "utils/compression_utils.h"
#include "metrics.h"

namespace decision {

// Sampling configuration: record 1 / (2^HW_LATENCY_SAMPLE_LOG2) of HW latencies
#ifndef HW_LATENCY_SAMPLE_LOG2
#define HW_LATENCY_SAMPLE_LOG2 2
#endif

// Threshold in bytes for selecting hardware when USE_HARDWARE_COMPRESSION==2
#ifndef HW_DECISION_THRESHOLD_BYTES
#define HW_DECISION_THRESHOLD_BYTES (1600 * 1024)
#endif

inline bool should_use_hardware_for_request(size_t uncompressed_size) {
#if USE_HARDWARE_COMPRESSION == 1
	return true;
#elif USE_HARDWARE_COMPRESSION == 0
	return false;
#elif USE_HARDWARE_COMPRESSION == 2
	// Size-based decision: use hardware for large inputs
	return uncompressed_size >= static_cast<size_t>(HW_DECISION_THRESHOLD_BYTES);
#else
	return true;
#endif
}

// Do not modify utils; provide per-path compression helpers here
thread_local std::unique_ptr<microservice::compression::CompressionManager> g_hw_mgr = nullptr;
thread_local std::unique_ptr<microservice::compression::CompressionManager> g_sw_mgr = nullptr;

inline microservice::compression::CompressionManager* get_mgr_for(qpl_path_t path) {
	if (path == qpl_path_hardware) {
		if (!g_hw_mgr) g_hw_mgr = std::make_unique<microservice::compression::CompressionManager>(qpl_path_hardware);
		return g_hw_mgr.get();
	}
	if (!g_sw_mgr) g_sw_mgr = std::make_unique<microservice::compression::CompressionManager>(qpl_path_software);
	return g_sw_mgr.get();
}

inline std::string compress_with_path(const std::string& data, qpl_path_t path) {
	microservice::compression::CompressionManager* mgr = get_mgr_for(path);
	if (path == qpl_path_hardware) {
		record_hw_submission();
		uint64_t task_size = static_cast<uint64_t>(data.size());
		HwSubmitKey key = record_hw_start_get_inflight_after(task_size);
		// Ensure we always decrement inflight when leaving this path (e.g. on exception)
		struct InflightGuard {
			uint64_t task_size_;
			bool committed_ = false;
			void commit() { committed_ = true; }
			~InflightGuard() { if (!committed_) record_hw_finish_nosample(task_size_); }
		} hw_guard{task_size};
		// Lightweight sampling: thread-local xorshift, sample if low bits are zero
		thread_local uint64_t rng = 0x9e3779b97f4a7c15ull;
		// xorshift64*
		auto next = [&]() {
			uint64_t x = rng;
			x ^= x >> 12;
			x ^= x << 25;
			x ^= x >> 27;
			rng = x;
			return x * 2685821657736338717ull;
		};
		bool sample = ((next() & ((1ull << HW_LATENCY_SAMPLE_LOG2) - 1ull)) == 0ull);
		std::string compressed;
		if (sample) {
			auto t0 = std::chrono::steady_clock::now();
			compressed = mgr->compress_to_string(data);
			auto t1 = std::chrono::steady_clock::now();
			uint64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
			record_hw_finish(key, latency_us);
			hw_guard.commit();
		} else {
			compressed = mgr->compress_to_string(data);
			record_hw_finish_nosample(task_size);
			hw_guard.commit();
		}
		if (!compressed.empty()) {
			return std::string("COMPRESSED:") + compressed;
		}
		return data;
	} else {
		auto compressed = mgr->compress_to_string(data);
		if (!compressed.empty()) {
			return std::string("COMPRESSED:") + compressed;
		}
		return data;
	}
}

} // namespace decision


