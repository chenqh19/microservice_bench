#pragma once
// Matmul: SW (scalar + VNNI blocked) and AMX. Software path uses cache-friendly blocked VNNI when available.
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <atomic>
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

// ---- Scalar fallback (s8*s8->s32) ----
inline void sw_matmul_scalar_loop(int64_t M, int64_t K, int64_t N,
	const int8_t* A, const int8_t* B, int32_t* C) {
	for (int64_t i = 0; i < M; ++i) {
		for (int64_t j = 0; j < N; ++j) {
			int32_t sum = 0;
			for (int64_t k = 0; k < K; ++k)
				sum += static_cast<int32_t>(A[i * K + k]) * static_cast<int32_t>(B[k * N + j]);
			C[i * N + j] = sum;
		}
	}
}

#if defined(__x86_64__)
#if defined(__linux__) && __has_include(<cpuid.h>)
#include <cpuid.h>
inline bool vnni_available() {
	unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
	if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
		// AVX-VNNI (256-bit): ECX bit 4; AVX-512 VNNI: EBX bit 11
		if ((ecx & (1u << 4)) || (ebx & (1u << 11))) return true;
	}
	return false;
}
#else
inline bool vnni_available() { return false; }
#endif

#if (defined(__AVXVNNI__) && defined(__AVX2__)) || (defined(__AVX512VL__) && defined(__AVX512VNNI__))
static constexpr int64_t kVNNI_K = 4;
static constexpr int64_t kVNNI_N = 8;

#if defined(__AVXVNNI__)
#define VNNI_DPBUSD(acc, a, b) _mm256_dpbusd_avx_epi32((acc), (a), (b))
#else
#define VNNI_DPBUSD(acc, a, b) _mm256_dpbusd_epi32((acc), (a), (b))
#endif

// Per-row VNNI kernel (small matrices or remainder).
inline void sw_matmul_vnni_impl(int64_t M, int64_t K, int64_t N,
	const int8_t* A, const int8_t* B, int32_t* C) {
	std::vector<int32_t> colsum_B(static_cast<size_t>(N), 0);
	for (int64_t k = 0; k < K; ++k)
		for (int64_t j = 0; j < N; ++j)
			colsum_B[static_cast<size_t>(j)] += static_cast<int32_t>(B[k * N + j]);

	const int64_t K4 = (K / kVNNI_K) * kVNNI_K;
	const int64_t N8 = (N / kVNNI_N) * kVNNI_N;

	for (int64_t i = 0; i < M; ++i) {
		for (int64_t j = 0; j < N8; j += kVNNI_N) {
			__m256i acc = _mm256_setzero_si256();
			for (int64_t k = 0; k < K4; k += kVNNI_K) {
				alignas(32) int8_t a_buf[4];
				for (int64_t t = 0; t < 4; ++t)
					a_buf[t] = static_cast<int8_t>(A[i * K + k + t] + 128);
				__m256i va = _mm256_set1_epi32(*reinterpret_cast<const int32_t*>(a_buf));
				__m128i r0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(B + (k + 0) * N + j));
				__m128i r1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(B + (k + 1) * N + j));
				__m128i r2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(B + (k + 2) * N + j));
				__m128i r3 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(B + (k + 3) * N + j));
				__m128i t0 = _mm_unpacklo_epi8(r0, r1);
				__m128i t1 = _mm_unpacklo_epi8(r2, r3);
				__m128i u0 = _mm_unpacklo_epi16(t0, t1);
				__m128i u1 = _mm_unpackhi_epi16(t0, t1);
				__m256i vb = _mm256_set_m128i(u1, u0);
				acc = VNNI_DPBUSD(acc, va, vb);
			}
			__m256i bias = _mm256_setr_epi32(
				colsum_B[static_cast<size_t>(j + 0)] * 128,
				colsum_B[static_cast<size_t>(j + 1)] * 128,
				colsum_B[static_cast<size_t>(j + 2)] * 128,
				colsum_B[static_cast<size_t>(j + 3)] * 128,
				colsum_B[static_cast<size_t>(j + 4)] * 128,
				colsum_B[static_cast<size_t>(j + 5)] * 128,
				colsum_B[static_cast<size_t>(j + 6)] * 128,
				colsum_B[static_cast<size_t>(j + 7)] * 128);
			acc = _mm256_sub_epi32(acc, bias);
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(C + i * N + j), acc);
		}
		for (int64_t j = N8; j < N; ++j) {
			int32_t sum = 0;
			for (int64_t k = 0; k < K; ++k)
				sum += static_cast<int32_t>(A[i * K + k]) * static_cast<int32_t>(B[k * N + j]);
			C[i * N + j] = sum;
		}
	}
	if (K4 < K) {
		for (int64_t i = 0; i < M; ++i) {
			for (int64_t j = 0; j < N; ++j) {
				int32_t sum = 0;
				for (int64_t k = K4; k < K; ++k)
					sum += static_cast<int32_t>(A[i * K + k]) * static_cast<int32_t>(B[k * N + j]);
				C[i * N + j] += sum;
			}
		}
	}
}

// Blocked VNNI matmul (cache-friendly): pack panels, micro-kernel, accumulate over K-blocks.
// Block sizes tuned for L2: Mc=64, Nc=64, Kc=128.
static constexpr int64_t kBlockMc = 64;
static constexpr int64_t kBlockNc = 64;
static constexpr int64_t kBlockKc = 128;

inline void sw_matmul_vnni_blocked_impl(int64_t M, int64_t K, int64_t N,
	const int8_t* A, const int8_t* B, int32_t* C) {
	std::vector<int32_t> colsum_B(static_cast<size_t>(N), 0);
	for (int64_t k = 0; k < K; ++k)
		for (int64_t j = 0; j < N; ++j)
			colsum_B[static_cast<size_t>(j)] += static_cast<int32_t>(B[k * N + j]);

	const int64_t K4 = (K / kVNNI_K) * kVNNI_K;
	const int64_t N8 = (N / kVNNI_N) * kVNNI_N;

	std::vector<int8_t> pack_A(static_cast<size_t>(kBlockMc * kBlockKc), 0);
	std::vector<int8_t> pack_B(static_cast<size_t>(kBlockKc * kBlockNc), 0);

	for (int64_t ic = 0; ic < M; ic += kBlockMc) {
		int64_t mc = (ic + kBlockMc <= M) ? kBlockMc : (M - ic);
		for (int64_t jc = 0; jc < N; jc += kBlockNc) {
			int64_t nc = (jc + kBlockNc <= N) ? kBlockNc : (N - jc);
			for (int64_t ii = 0; ii < mc; ++ii)
				for (int64_t jj = 0; jj < nc && (jc + jj) < N8; ++jj)
					C[(ic + ii) * N + (jc + jj)] = 0;

			for (int64_t pc = 0; pc < K; pc += kBlockKc) {
				int64_t kc = (pc + kBlockKc <= K) ? kBlockKc : (K - pc);
				for (int64_t ii = 0; ii < mc; ++ii)
					std::memcpy(pack_A.data() + static_cast<size_t>(ii * kc),
						A + (ic + ii) * K + pc, static_cast<size_t>(kc));
				for (int64_t kk = 0; kk < kc; ++kk)
					std::memcpy(pack_B.data() + static_cast<size_t>(kk * nc),
						B + (pc + kk) * N + jc, static_cast<size_t>(nc));

				int64_t kc4 = (kc / kVNNI_K) * kVNNI_K;
				for (int64_t ii = 0; ii < mc; ++ii) {
					for (int64_t jj = 0; jj < nc && (jc + jj + kVNNI_N) <= N8; jj += kVNNI_N) {
						__m256i acc = _mm256_setzero_si256();
						for (int64_t kk = 0; kk < kc4; kk += kVNNI_K) {
							alignas(32) int8_t a_buf[4];
							for (int64_t t = 0; t < 4; ++t)
								a_buf[t] = static_cast<int8_t>(pack_A[static_cast<size_t>(ii * kc + kk + t)] + 128);
							__m256i va = _mm256_set1_epi32(*reinterpret_cast<const int32_t*>(a_buf));
							const int8_t* b_ptr = pack_B.data() + static_cast<size_t>(kk * nc + jj);
							__m128i r0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(b_ptr + 0 * nc));
							__m128i r1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(b_ptr + 1 * nc));
							__m128i r2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(b_ptr + 2 * nc));
							__m128i r3 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(b_ptr + 3 * nc));
							__m128i t0 = _mm_unpacklo_epi8(r0, r1);
							__m128i t1 = _mm_unpacklo_epi8(r2, r3);
							__m128i u0 = _mm_unpacklo_epi16(t0, t1);
							__m128i u1 = _mm_unpackhi_epi16(t0, t1);
							__m256i vb = _mm256_set_m128i(u1, u0);
							acc = VNNI_DPBUSD(acc, va, vb);
						}
						__m256i cur = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(C + (ic + ii) * N + jc + jj));
						_mm256_storeu_si256(reinterpret_cast<__m256i*>(C + (ic + ii) * N + jc + jj), _mm256_add_epi32(cur, acc));
					}
				}
			}
			for (int64_t ii = 0; ii < mc; ++ii) {
				for (int64_t jj = 0; jj < nc && (jc + jj + kVNNI_N) <= N8; jj += kVNNI_N) {
					__m256i cur = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(C + (ic + ii) * N + jc + jj));
					__m256i bias = _mm256_setr_epi32(
						colsum_B[static_cast<size_t>(jc + jj + 0)] * 128,
						colsum_B[static_cast<size_t>(jc + jj + 1)] * 128,
						colsum_B[static_cast<size_t>(jc + jj + 2)] * 128,
						colsum_B[static_cast<size_t>(jc + jj + 3)] * 128,
						colsum_B[static_cast<size_t>(jc + jj + 4)] * 128,
						colsum_B[static_cast<size_t>(jc + jj + 5)] * 128,
						colsum_B[static_cast<size_t>(jc + jj + 6)] * 128,
						colsum_B[static_cast<size_t>(jc + jj + 7)] * 128);
					_mm256_storeu_si256(reinterpret_cast<__m256i*>(C + (ic + ii) * N + jc + jj), _mm256_sub_epi32(cur, bias));
				}
			}
		}
	}

	if (N8 < N) {
		for (int64_t i = 0; i < M; ++i) {
			for (int64_t j = N8; j < N; ++j) {
				int32_t sum = 0;
				for (int64_t k = 0; k < K; ++k)
					sum += static_cast<int32_t>(A[i * K + k]) * static_cast<int32_t>(B[k * N + j]);
				C[i * N + j] = sum;
			}
		}
	}
	if (K4 < K) {
		for (int64_t i = 0; i < M; ++i) {
			for (int64_t j = 0; j < N; ++j) {
				int32_t sum = 0;
				for (int64_t k = K4; k < K; ++k)
					sum += static_cast<int32_t>(A[i * K + k]) * static_cast<int32_t>(B[k * N + j]);
				C[i * N + j] += sum;
			}
		}
	}
}
	#undef VNNI_DPBUSD

#else
inline void sw_matmul_vnni_impl(int64_t, int64_t, int64_t, const int8_t*, const int8_t*, int32_t*) {}
inline void sw_matmul_vnni_blocked_impl(int64_t, int64_t, int64_t, const int8_t*, const int8_t*, int32_t*) {}
#endif

#else
inline bool vnni_available() { return false; }
#endif

inline std::string sw_matmul_impl(int64_t M, int64_t K, int64_t N,
	const int8_t* A, const int8_t* B, uint64_t* out_latency_us) {
	static std::atomic<int> sw_path_logged{0};
	if (sw_path_logged++ == 0) {
		const char* path = "scalar";
#if defined(__x86_64__) && defined(__AVX2__)
		bool vnni = vnni_available();
		if (vnni && K >= kVNNI_K && N >= kVNNI_N) {
			path = (M >= kBlockMc && N >= kBlockNc && K >= kBlockKc)
				? "VNNI (blocked)" : "VNNI (per-row)";
		}
		std::cerr << "[matmul] sw_matmul path: " << path
			<< " (vnni_available=" << (vnni ? 1 : 0)
			<< " M=" << M << " K=" << K << " N=" << N << ")" << std::endl;
#else
		std::cerr << "[matmul] sw_matmul path: " << path << " (not x86_64/AVX2)" << std::endl;
#endif
	}

	size_t c_sz = static_cast<size_t>(M * N);
	std::vector<int32_t> C(c_sz, 0);
	auto t0 = std::chrono::steady_clock::now();

#if defined(__x86_64__) && defined(__AVX2__)
	if (vnni_available() && K >= kVNNI_K && N >= kVNNI_N) {
		if (M >= kBlockMc && N >= kBlockNc && K >= kBlockKc)
			sw_matmul_vnni_blocked_impl(M, K, N, A, B, C.data());
		else
			sw_matmul_vnni_impl(M, K, N, A, B, C.data());
	} else
#endif
		sw_matmul_scalar_loop(M, K, N, A, B, C.data());

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
	std::cout << "[matmul] executed on: software (blocked VNNI or scalar)" << std::endl;
	return sw_matmul_impl(M, K, N, ptr_A, ptr_B, out_latency_us);
}

} // namespace matmul
} // namespace microservice
