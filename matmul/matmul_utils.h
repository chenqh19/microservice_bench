#pragma once
// Matmul: SW (s8*s8->s32 loop) and AMX (direct intrinsics). Hybrid: AMX for aligned blocks + SW remainder.
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <sstream>

#if defined(__x86_64__)
#include <x86intrin.h>
#if __has_include(<amxintrin.h>)
#include <amxintrin.h>
#endif
#if defined(__linux__)
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include <asm/prctl.h>
#ifndef ARCH_REQ_XCOMP_PERM
#define ARCH_REQ_XCOMP_PERM 0x1023
#endif
#ifndef XFEATURE_XTILEDATA
#define XFEATURE_XTILEDATA 18
#endif
#endif
#endif

#define MATMUL_LOG(x) do { std::ostringstream _ss; _ss << x; std::cerr << "[matmul] " << _ss.str() << std::endl; } while(0)

namespace microservice {
namespace matmul {

inline bool parse_matmul_dims(const std::string& data, int64_t& M, int64_t& K, int64_t& N, int64_t max_dim) {
	M = K = N = 0;
	size_t c1 = data.find(',');
	if (c1 == std::string::npos) return false;
	size_t c2 = data.find(',', c1 + 1);
	if (c2 == std::string::npos || data.find(',', c2 + 1) != std::string::npos) return false;
	try {
		M = std::stoll(data.substr(0, c1));
		K = std::stoll(data.substr(c1 + 1, c2 - (c1 + 1)));
		N = std::stoll(data.substr(c2 + 1));
	} catch (...) {
		return false;
	}
	if (M <= 0 || K <= 0 || N <= 0) return false;
	if (M > max_dim || K > max_dim || N > max_dim) return false;
	return true;
}

inline std::string sw_matmul_impl(int64_t M, int64_t K, int64_t N,
	const int8_t* A, const int8_t* B, uint64_t* out_latency_us) {
	size_t c_sz = static_cast<size_t>(M * N);
	std::vector<int32_t> C(c_sz, 0);
	auto t0 = std::chrono::steady_clock::now();
	for (int64_t i = 0; i < M; ++i) {
		for (int64_t j = 0; j < N; ++j) {
			int32_t sum = 0;
			for (int64_t kk = 0; kk < K; ++kk)
				sum += static_cast<int32_t>(A[i * K + kk]) * static_cast<int32_t>(B[kk * N + j]);
			C[i * N + j] = sum;
		}
	}
	auto t1 = std::chrono::steady_clock::now();
	if (out_latency_us)
		*out_latency_us = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
	return "OK";
}

#if defined(__x86_64__)
static constexpr int64_t kAMX_M = 16;
static constexpr int64_t kAMX_K = 64;
static constexpr int64_t kAMX_N = 16;

#if defined(__linux__)
inline bool amx_request_perm() {
	static int done = 0;
	if (done) return done > 0;
	long ret = syscall(SYS_arch_prctl, ARCH_REQ_XCOMP_PERM, XFEATURE_XTILEDATA);
	done = (ret == 0) ? 1 : -1;
	return done > 0;
}
#else
inline bool amx_request_perm() { return true; }
#endif

void amx_matmul_blocked(int64_t M, int64_t K, int64_t N,
	const int8_t* A, const int8_t* B, int32_t* C);

inline std::string amx_matmul_impl(int64_t M, int64_t K, int64_t N,
	const int8_t* A, const int8_t* B, uint64_t* out_latency_us) {
	if (!amx_request_perm())
		return sw_matmul_impl(M, K, N, A, B, out_latency_us);

	int64_t K0 = (K / kAMX_K) * kAMX_K;
	const bool use_amx_block = (K0 > 0) && (M % kAMX_M == 0) && (N % kAMX_N == 0);
	const bool has_remainder = (K0 < K);

	if (!use_amx_block)
		return sw_matmul_impl(M, K, N, A, B, out_latency_us);

	size_t c_sz = static_cast<size_t>(M * N);
	std::vector<int32_t> C(c_sz, 0);
	auto t0 = std::chrono::steady_clock::now();

	if (use_amx_block) {
#if defined(__linux__)
		sigset_t block_all, old_mask;
		sigfillset(&block_all);
		sigprocmask(SIG_BLOCK, &block_all, &old_mask);
#endif
		amx_matmul_blocked(M, K0, N, A, B, C.data());
#if defined(__linux__)
		sigprocmask(SIG_SETMASK, &old_mask, nullptr);
#endif
	}

	if (has_remainder) {
		for (int64_t i = 0; i < M; ++i) {
			for (int64_t j = 0; j < N; ++j) {
				int32_t sum = 0;
				for (int64_t kk = K0; kk < K; ++kk)
					sum += static_cast<int32_t>(A[i * K + kk]) * static_cast<int32_t>(B[kk * N + j]);
				C[i * N + j] += sum;
			}
		}
	}

	auto t1 = std::chrono::steady_clock::now();
	if (out_latency_us)
		*out_latency_us = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
	return "OK";
}

#else
inline std::string amx_matmul_impl(int64_t M, int64_t K, int64_t N,
	const int8_t* A, const int8_t* B, uint64_t* out_latency_us) {
	return sw_matmul_impl(M, K, N, A, B, out_latency_us);
}
#endif

inline std::string matmul_run(int64_t M, int64_t K, int64_t N,
	const int8_t* ptr_A, const int8_t* ptr_B, bool use_amx, uint64_t* out_latency_us) {
	if (use_amx) {
#if defined(__x86_64__)
		std::cout << "[matmul] executed on: hardware (AMX)" << std::endl;
#else
		std::cout << "[matmul] executed on: hardware requested but not x86_64 (SW fallback)" << std::endl;
#endif
		return amx_matmul_impl(M, K, N, ptr_A, ptr_B, out_latency_us);
	}
	std::cout << "[matmul] executed on: software (s8*s8->s32 loop)" << std::endl;
	return sw_matmul_impl(M, K, N, ptr_A, ptr_B, out_latency_us);
}

} // namespace matmul
} // namespace microservice
