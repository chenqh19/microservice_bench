// Standalone: run a single matmul request (AMX or SW). No HTTP, no server deps.
// Build: cd matmul/build && cmake .. && make run_matmul && ./run_matmul [M K N]
#include "matmul_utils.h"
#include <iostream>
#include <vector>
#include <random>
#include <cstdlib>

int main(int argc, char** argv) {
  int64_t M = 768, K = 768, N = 768;
  if (argc >= 4) {
    M = std::atol(argv[1]);
    K = std::atol(argv[2]);
    N = std::atol(argv[3]);
  }
  if (M <= 0 || K <= 0 || N <= 0) {
    std::cerr << "Usage: " << (argv[0] ? argv[0] : "run_matmul") << " [M K N]\n"
              << "  Default: M=768 K=768 N=768\n";
    return 1;
  }

  size_t szA = static_cast<size_t>(M * K);
  size_t szB = static_cast<size_t>(K * N);
  std::vector<int8_t> A(szA), B(szB);
  std::mt19937 rng(0x9e3779b9u);
  std::uniform_int_distribution<int> dist(-128, 127);
  for (size_t i = 0; i < szA; ++i) A[i] = static_cast<int8_t>(dist(rng));
  for (size_t i = 0; i < szB; ++i) B[i] = static_cast<int8_t>(dist(rng));

  std::cout << "matmul M=" << M << " K=" << K << " N=" << N << " (single request)\n";

  uint64_t latency_us = 0;
  std::string result = microservice::matmul::amx_matmul_impl(
      M, K, N, A.data(), B.data(), &latency_us);

  std::cout << "result=" << result << " latency_us=" << latency_us << "\n";
  return result == "OK" ? 0 : 1;
}
