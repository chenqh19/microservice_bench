#pragma once
#include <cstdint>
#include <vector>
#include <cmath>
#include <utility>

namespace decision {

// Linear regression: y = slope * x + intercept (least squares).
// Input: pairs (x = inflight_at_submit, y = latency_us).
// Returns (slope, intercept). If fit is undefined (n < 2 or zero x variance), returns (0, mean_y).
struct LinearFit {
	double slope{0.0};
	double intercept{0.0};
	double r_squared{0.0};  // coefficient of determination [0,1]
	bool valid{false};      // true if slope/intercept are defined (n >= 2 and Var(x) > 0)
};

inline LinearFit linear_regression(const uint64_t* x, const uint64_t* y, uint64_t n) {
	LinearFit out;
	if (n == 0) return out;
	double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0, sum_y2 = 0;
	for (uint64_t i = 0; i < n; ++i) {
		double xi = static_cast<double>(x[i]);
		double yi = static_cast<double>(y[i]);
		sum_x  += xi;
		sum_y  += yi;
		sum_xy += xi * yi;
		sum_x2 += xi * xi;
		sum_y2 += yi * yi;
	}
	double mean_x = sum_x / static_cast<double>(n);
	double mean_y = sum_y / static_cast<double>(n);
	double denom = static_cast<double>(n) * sum_x2 - sum_x * sum_x;
	if (n < 2 || denom <= 0) {
		out.intercept = mean_y;
		out.valid = false;
		return out;
	}
	out.slope     = (static_cast<double>(n) * sum_xy - sum_x * sum_y) / denom;
	out.intercept = mean_y - out.slope * mean_x;
	out.valid     = true;
	// R² = 1 - SS_res / SS_tot, SS_res = sum((y_i - (slope*x_i + intercept))^2), SS_tot = sum((y_i - mean_y)^2)
	double ss_tot = 0, ss_res = 0;
	for (uint64_t i = 0; i < n; ++i) {
		double yi = static_cast<double>(y[i]);
		double pred = out.slope * static_cast<double>(x[i]) + out.intercept;
		ss_tot += (yi - mean_y) * (yi - mean_y);
		ss_res += (yi - pred) * (yi - pred);
	}
	if (ss_tot > 0)
		out.r_squared = 1.0 - (ss_res / ss_tot);
	else
		out.r_squared = 0.0;
	return out;
}

// Convenience: regression from vectors of (inflight, latency) collected from the ring.
inline LinearFit linear_regression_from_ring(
	const uint64_t* inflight_at_submit,
	const uint64_t* latencies_us,
	uint32_t count,
	uint32_t ring_capacity
) {
	if (count == 0 || count > ring_capacity) return LinearFit{};
	// We need contiguous indices; the caller typically has a window [start, start+count) in the ring.
	// This API expects the caller to pass pointers to the ring and the number of valid entries.
	// The ring is used with indices (idx % RING_CAPACITY), so the caller must pass the slice they read.
	return linear_regression(inflight_at_submit, latencies_us, static_cast<uint64_t>(count));
}

} // namespace decision
