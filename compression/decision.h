#pragma once
#include "config.h"
#include <cstdint>
#include <chrono>
#include "utils/compression_utils.h"
#include "metrics.h"

namespace decision {

// Sampling configuration: record 1 / (2^HW_LATENCY_SAMPLE_LOG2) of HW latencies
#ifndef HW_LATENCY_SAMPLE_LOG2
#define HW_LATENCY_SAMPLE_LOG2 0
#endif

// Threshold in bytes for selecting hardware when USE_HARDWARE_COMPRESSION==2
#ifndef HW_DECISION_THRESHOLD_BYTES
#define HW_DECISION_THRESHOLD_BYTES (1004 * 1024)
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

// Per-path compression uses async QPL (submit -> wait -> get result) via compress_to_string_via_async.
// When path is HW and out_compress_latency_us is non-null, writes compression wait time (us) there.
inline std::string compress_with_path(const std::string& data, qpl_path_t path,
	uint64_t* out_compress_latency_us = nullptr) {
	// Use async path (submit -> wait -> get result) with per-request path and HW latency sampling
	if (path == qpl_path_hardware) {
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
		std::string compressed = microservice::compression::compress_to_string_via_async(data, path, &latency_us);
		if (out_compress_latency_us) *out_compress_latency_us = latency_us;
		if (sample) {
			record_hw_finish(key, latency_us);
			hw_guard.commit();
		} else {
			record_hw_finish_nosample();
			hw_guard.commit();
		}
		if (!compressed.empty()) {
			return std::string("COMPRESSED:") + compressed;
		}
		return data;
	} else {
		std::string compressed = microservice::compression::compress_to_string_via_async(data, path);
		if (!compressed.empty()) {
			return std::string("COMPRESSED:") + compressed;
		}
		return data;
	}
}

// Decompress using the same path (HW/SW) as was used for compression.
// When path is HW and out_decompress_latency_us is non-null, writes decompression wait time (us) there.
inline std::string decompress_with_path(const std::string& data, qpl_path_t path,
	uint64_t* out_decompress_latency_us = nullptr) {
	return microservice::compression::decompress_from_string_via_async(data, path, out_decompress_latency_us);
}

} // namespace decision


