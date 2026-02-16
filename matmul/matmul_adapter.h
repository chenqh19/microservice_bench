#pragma once
// Bridges decision layer (HwSwPath, ExecuteOp) with matmul (matmul_run).
// For use by services that include scheduler (decision.h) and matmul.
#include "decision.h"
#include "matmul_utils.h"

namespace decision {

inline ExecuteOp matmul_operation(const int8_t* ptr_A, const int8_t* ptr_B, int64_t max_dim) {
	return [ptr_A, ptr_B, max_dim](const std::string& data, HwSwPath path, uint64_t* out_lat) {
		int64_t M = 0, K = 0, N = 0;
		if (!microservice::matmul::parse_matmul_dims(data, M, K, N, max_dim))
			return std::string("INVALID_DIMS");
		return microservice::matmul::matmul_run(M, K, N, ptr_A, ptr_B, path == HwSwPath::Hardware, out_lat);
	};
}

} // namespace decision
