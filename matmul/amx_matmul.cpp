// AMX matmul: follows ~/qat/amx/bench_matmul_sw_hw.c exactly (tile config, VNNI-packed B).
// Single TU compiled with -mamx-tile -mamx-int8.
#include <cstdint>
#include <cstring>

#if defined(__x86_64__)
#include <x86intrin.h>
#if __has_include(<amxintrin.h>)
#include <amxintrin.h>
#endif
#endif

#if defined(__x86_64__)
namespace microservice {
namespace matmul {

static constexpr int64_t kAMX_M = 16;
static constexpr int64_t kAMX_K = 64;
static constexpr int64_t kAMX_N = 16;

typedef struct __attribute__((packed, aligned(64))) {
  uint8_t palette_id;
  uint8_t start_row;
  uint8_t reserved0[14];
  uint16_t colsb[8];
  uint8_t reserved1[16];
  uint8_t rows[8];
  uint8_t reserved2[8];
} tilecfg_t;

static inline void amx_ldtilecfg(const void *cfg) {
  __asm__ volatile("ldtilecfg (%0)" : : "r"(cfg) : "memory");
}

static void packb_vnni_block(int64_t k0, int64_t j0, int64_t N,
                              const int8_t *B, int8_t *Bp) {
  const int Kb = 64, Nb = 16;
  const int ldbp = 64;
  for (int k4 = 0; k4 < Kb / 4; k4++) {
    int64_t row0 = (k0 + k4 * 4) * N + j0;
    for (int n = 0; n < Nb; n++) {
      Bp[k4 * ldbp + n * 4 + 0] = B[row0 + n];
      Bp[k4 * ldbp + n * 4 + 1] = B[row0 + N + n];
      Bp[k4 * ldbp + n * 4 + 2] = B[row0 + 2 * N + n];
      Bp[k4 * ldbp + n * 4 + 3] = B[row0 + 3 * N + n];
    }
  }
}

void amx_matmul_blocked(int64_t M, int64_t K, int64_t N,
                        const int8_t *A, const int8_t *B, int32_t *C) {
  static thread_local tilecfg_t cfg;
  std::memset(&cfg, 0, sizeof(cfg));
  cfg.palette_id = 1;
  cfg.colsb[0] = 16 * 4;
  cfg.rows[0] = 16;
  cfg.colsb[1] = static_cast<uint16_t>(kAMX_K);
  cfg.rows[1] = 16;
  cfg.colsb[2] = 16 * 4;
  cfg.rows[2] = static_cast<uint8_t>(kAMX_K / 4);
  amx_ldtilecfg(&cfg);

  alignas(64) static thread_local int8_t Bp_buf[16 * 64];

  for (int64_t i = 0; i < M; i += kAMX_M) {
    for (int64_t j = 0; j < N; j += kAMX_N) {
      _tile_zero(0);
      for (int64_t k = 0; k < K; k += kAMX_K) {
        packb_vnni_block(k, j, N, B, Bp_buf);
        _tile_loadd(1, &A[i * K + k], static_cast<unsigned long>(K));
        _tile_loadd(2, Bp_buf, 16 * 4);
        _tile_dpbssd(0, 1, 2);
      }
      _tile_stored(0, &C[i * N + j], static_cast<unsigned long>(N * sizeof(int32_t)));
    }
  }
  _tile_release();
}

}  // namespace matmul
}  // namespace microservice
#endif
