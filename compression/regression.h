#pragma once
#include <cstdint>
#include <cmath>
#include <utility>

namespace decision {

// Linear regression: y = intercept + coef[0]*x1 + coef[1]*x2 + coef[2]*x3 (least squares).
// Predictors are not centered; intercept and coef[] are raw OLS estimates.
struct LinearFit {
	static constexpr int MAX_PREDICTORS = 3;
	double intercept{0.0};
	double coef[MAX_PREDICTORS]{0.0, 0.0, 0.0};
	int num_predictors{0};
	double r_squared{0.0};
	bool valid{false};
};

inline bool solve_square(double* A, double* b, int K) {
	for (int col = 0; col < K; ++col) {
		int pivot = col;
		for (int row = col + 1; row < K; ++row) {
			if (std::fabs(A[row * K + col]) > std::fabs(A[pivot * K + col])) pivot = row;
		}
		if (pivot != col) {
			for (int j = 0; j < K; ++j) std::swap(A[col * K + j], A[pivot * K + j]);
			std::swap(b[col], b[pivot]);
		}
		if (std::fabs(A[col * K + col]) < 1e-15) return false;
		double scale = 1.0 / A[col * K + col];
		for (int j = col; j < K; ++j) A[col * K + j] *= scale;
		b[col] *= scale;
		for (int row = 0; row < K; ++row) {
			if (row == col) continue;
			double f = A[row * K + col];
			for (int j = col; j < K; ++j) A[row * K + j] -= f * A[col * K + j];
			b[row] -= f * b[col];
		}
	}
	return true;
}

// num_predictors: 1, 2, or 3. x2/x3 may be nullptr when not used. Predictors not centered.
inline LinearFit linear_regression(
	const uint64_t* x1, const uint64_t* x2, const uint64_t* x3,
	const uint64_t* y, uint64_t n, int num_predictors
) {
	LinearFit out;
	out.num_predictors = num_predictors;
	int K = num_predictors + 1;
	if (n < static_cast<uint64_t>(K)) return out;
	if (num_predictors < 1 || num_predictors > 3) return out;
	if (num_predictors >= 2 && !x2) return out;
	if (num_predictors >= 3 && !x3) return out;

	double sum_y = 0;
	for (uint64_t i = 0; i < n; ++i) sum_y += static_cast<double>(y[i]);
	double mean_y = sum_y / static_cast<double>(n);

	double XtX[16] = {0};
	double Xty[4] = {0};
	for (uint64_t i = 0; i < n; ++i) {
		double x1_v = static_cast<double>(x1[i]);
		double x2_v = (num_predictors >= 2) ? static_cast<double>(x2[i]) : 0.0;
		double x3_v = (num_predictors >= 3) ? static_cast<double>(x3[i]) : 0.0;
		double v[4] = {1.0, x1_v, x2_v, x3_v};
		double yi = static_cast<double>(y[i]);
		for (int r = 0; r < K; ++r) {
			for (int c = 0; c < K; ++c) XtX[r * K + c] += v[r] * v[c];
			Xty[r] += v[r] * yi;
		}
	}
	if (!solve_square(XtX, Xty, K)) return out;
	out.intercept = Xty[0];
	for (int j = 0; j < num_predictors; ++j) out.coef[j] = Xty[j + 1];
	out.valid = true;

	double ss_tot = 0, ss_res = 0;
	for (uint64_t i = 0; i < n; ++i) {
		double yi = static_cast<double>(y[i]);
		double pred = out.intercept + out.coef[0] * static_cast<double>(x1[i]);
		if (num_predictors >= 2) pred += out.coef[1] * static_cast<double>(x2[i]);
		if (num_predictors >= 3) pred += out.coef[2] * static_cast<double>(x3[i]);
		ss_tot += (yi - mean_y) * (yi - mean_y);
		ss_res += (yi - pred) * (yi - pred);
	}
	if (ss_tot > 0) out.r_squared = 1.0 - (ss_res / ss_tot);
	return out;
}

inline LinearFit linear_regression(const uint64_t* x, const uint64_t* y, uint64_t n) {
	return linear_regression(x, nullptr, nullptr, y, n, 1);
}

} // namespace decision
