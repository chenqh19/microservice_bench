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
	std::vector<std::string>& compressed_out) {
	bool use_hw = should_use_hardware_for_request(slice.size());
	compressed_out.clear();
	if (use_hw) {
		std::string compressed = compress_with_path(slice, qpl_path_hardware);
		compressed_out.push_back(std::move(compressed));
	} else {
		const size_t kChunkSize = 32 * 1024;
		for (size_t offset = 0; offset < slice.size(); offset += kChunkSize) {
			size_t len = std::min(kChunkSize, slice.size() - offset);
			std::string compressed = compress_with_path(
				std::string(slice.data() + offset, len), qpl_path_software);
			compressed_out.push_back(std::move(compressed));
		}
	}
}
#
} // namespace decision


