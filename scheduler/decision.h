#pragma once
#include "config.h"
#include <cstdint>
#include <chrono>
#include <functional>
#include <string>
#include "metrics.h"

namespace decision {

// Operation-agnostic path: no dependency on QPL or compression.
enum class HwSwPath { Software, Hardware };

// Sampling configuration: record 1 / (2^HW_LATENCY_SAMPLE_LOG2) of HW latencies
#ifndef HW_LATENCY_SAMPLE_LOG2
#define HW_LATENCY_SAMPLE_LOG2 0
#endif

// Threshold in bytes for selecting hardware when USE_HARDWARE_COMPRESSION==2
// (Used generically for any HW/SW operation decision.)
#ifndef HW_DECISION_THRESHOLD_BYTES
#define HW_DECISION_THRESHOLD_BYTES (1004 * 1024)
#endif

inline bool should_use_hardware_for_request(size_t input_size) {
#if USE_HARDWARE_COMPRESSION == 1
	return true;
#elif USE_HARDWARE_COMPRESSION == 0
	return false;
#elif USE_HARDWARE_COMPRESSION == 2
	return input_size >= static_cast<size_t>(HW_DECISION_THRESHOLD_BYTES);
#else
	return true;
#endif
}

// Operation callback: (data, path, out_latency_us) -> result string.
// out_latency_us may be null; when path is Hardware and non-null, implementation may write latency (us).
using ExecuteOp = std::function<std::string(const std::string&, HwSwPath, uint64_t*)>;

// Execute one operation on the given path with HW metrics (submit/start/finish, optional sampling).
// Does not depend on compression; the actual work is done by op().
inline std::string execute_with_path(const std::string& data, HwSwPath path,
	ExecuteOp op, uint64_t* out_latency_us = nullptr) {
	if (path == HwSwPath::Hardware) {
		record_hw_submission();
		HwSubmitKey key = record_hw_start_get_inflight_after();
		struct InflightGuard {
			bool committed_ = false;
			void commit() { committed_ = true; }
			~InflightGuard() { if (!committed_) record_hw_finish_nosample(); }
		} hw_guard;
		thread_local uint64_t rng = 0x9e3779b97f4a7c15ull;
		auto next = [&]() {
			uint64_t x = rng;
			x ^= x >> 12;
			x ^= x << 25;
			x ^= x >> 27;
			rng = x;
			return x * 2685821657736338717ull;
		};
		bool sample = ((next() & ((1ull << HW_LATENCY_SAMPLE_LOG2) - 1ull)) == 0ull);
		uint64_t latency_us = 0;
		std::string result = op(data, HwSwPath::Hardware, &latency_us);
		if (out_latency_us) *out_latency_us = latency_us;
		if (sample) {
			record_hw_finish(key, latency_us);
			hw_guard.commit();
		} else {
			record_hw_finish_nosample();
			hw_guard.commit();
		}
		return result;
	}
	return op(data, HwSwPath::Software, out_latency_us);
}

} // namespace decision
