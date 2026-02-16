#pragma once
#include "config.h"
#include "decision.h"
#include <string>
#include <algorithm>
#include <vector>

namespace decision {

// Generic collect: runs op on slice (possibly chunked), HW/SW chosen by should_use_hardware_for_request.
// No dependency on compression; op(data, path, out_latency_us) performs the actual operation.
inline void collect(const std::string& slice,
	std::vector<std::string>& results_out,
	ExecuteOp op,
	HwSwPath* path_used = nullptr,
	uint64_t* out_latency_us = nullptr) {
	bool use_hw = should_use_hardware_for_request(slice.size());
	results_out.clear();
	if (use_hw) {
		if (path_used) *path_used = HwSwPath::Hardware;
		results_out.push_back(execute_with_path(slice, HwSwPath::Hardware, op, out_latency_us));
	} else {
		if (path_used) *path_used = HwSwPath::Software;
		const size_t kChunkSize = 32 * 1024;
		for (size_t offset = 0; offset < slice.size(); offset += kChunkSize) {
			size_t len = std::min(kChunkSize, slice.size() - offset);
			results_out.push_back(execute_with_path(
				std::string(slice.data() + offset, len), HwSwPath::Software, op, nullptr));
		}
	}
}

} // namespace decision
