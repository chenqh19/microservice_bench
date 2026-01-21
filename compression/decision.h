#pragma once
#include "config.h"
#include <cstdint>
#include <chrono>
#include "utils/compression_utils.h"

namespace decision {

// Threshold in bytes for selecting hardware when USE_HARDWARE_COMPRESSION==2
#ifndef HW_DECISION_THRESHOLD_BYTES
#define HW_DECISION_THRESHOLD_BYTES (430 * 1024)
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
	auto compressed = mgr->compress_to_string(data);
	auto stats = mgr->get_compression_stats(data);
	if (stats.success && !compressed.empty()) {
		return std::string("COMPRESSED:") + compressed;
	}
	return data;
}

} // namespace decision


