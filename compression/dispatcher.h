#pragma once
#include "config.h"
#include "decision.h"
#include "utils/compression_utils.h"
#include <string>
#include <algorithm>
#include <vector>
#
namespace decision {
#
inline void compress_collect(const std::string& slice,
	std::vector<std::string>& compressed_out,
	qpl_path_t* path_used = nullptr,
	uint64_t* out_compress_latency_us = nullptr) {
	bool use_hw = should_use_hardware_for_request(slice.size());
	// if (use_hw && get_hw_inflight() >= 3)
	// 	use_hw = false;
	compressed_out.clear();
	if (use_hw) {
		if (path_used) *path_used = qpl_path_hardware;
		std::string compressed = compress_with_path(slice, qpl_path_hardware, out_compress_latency_us);
		compressed_out.push_back(std::move(compressed));
	} else {
		if (path_used) *path_used = qpl_path_software;
		const size_t kChunkSize = 32 * 1024;
		for (size_t offset = 0; offset < slice.size(); offset += kChunkSize) {
			size_t len = std::min(kChunkSize, slice.size() - offset);
			std::string compressed = compress_with_path(
				std::string(slice.data() + offset, len), qpl_path_software, nullptr);
			compressed_out.push_back(std::move(compressed));
		}
	}
}
#
} // namespace decision


