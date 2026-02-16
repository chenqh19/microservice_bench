#pragma once
// Bridges decision layer (HwSwPath, ExecuteOp) with compression (qpl_path_t, compress_to_string_via_async).
// Include only in compression-specific code (e.g. main.cpp).
#include "decision.h"
#include "utils/compression_utils.h"

namespace decision {

inline qpl_path_t to_qpl_path(HwSwPath p) {
	return p == HwSwPath::Hardware ? qpl_path_hardware : qpl_path_software;
}

inline ExecuteOp compress_operation() {
	return [](const std::string& d, HwSwPath p, uint64_t* out_lat) {
		uint64_t lat = 0;
		std::string compressed = microservice::compression::compress_to_string_via_async(
			d, to_qpl_path(p), out_lat ? &lat : nullptr);
		if (out_lat) *out_lat = lat;
		if (!compressed.empty()) return std::string("COMPRESSED:") + compressed;
		return d;
	};
}

} // namespace decision
