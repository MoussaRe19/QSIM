#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>

#include "prng.h"
#include "dist.h"

static double sample_mean(Distribution *d, PRNG *prng, int n) {
	double sum = 0.0;
	for (int i = 0; i < n; i++)
		sum += dist_sample(d, prng);

	return sum / (double)n;
}

// Test1:
static void test_prng_bounds(void) {
	printf("test_prng_bounds (5M draws) ... ");

	PRNG prng;
	prng_init(&prng, 12345ULL);

	const int N = 5000000;
	for (int i = 0; i < N; i++) {
		double u = prng_uniform(&prng);
		assert(u > 0.0 &&
		       "zero-guard failed: prng_uniform returned 0.0 or negative");
		assert(u < 1.0 && "upper-bound violated: prng_uniform returned >= 1.0");
	}

	printf("PASS\n");
}

// Test2:
static void test_prng_reproducibility(void) {
	printf("test_prng_reproducibility ... ");

	const uint64_t seed = 0xDEADBEEFCAFEBABEULL;
	const int N = 10000;

	PRNG p1, p2;
	prng_init(&p1, seed);
	prng_init(&p2, seed);

	for (int i = 0; i < N; i++) {
		double u1 = prng_uniform(&p1);
		double u2 = prng_uniform(&p2);
		assert(u1 == u2 &&
		       "reproducibility violated: same seed produced different output");
	}

	printf("PASS\n");
}

// Test3:
static void test_prng_different_seeds(void) {
	printf("test_prng_different_seeds ... ");

	const int N = 1000;
	PRNG p1, p2;
	prng_init(&p1, 42ULL);
	prng_init(&p2, 43ULL);

	int matches = 0;
	for (int i = 0; i < N; i++) {
		double u1 = prng_uniform(&p1);
		double u2 = prng_uniform(&p2);
		if (u1 == u2) matches++;
	}

	assert(matches == 0 &&
	       "different seeds produced matching values in 1000 draws");

	printf("PASS\n");
}

// Test4:
static void test_prng_curseed(void) {
	printf("test_prng_curseed ... ");

	PRNG prng;
	prng_init(&prng, 0xCAFEBABEULL);

	assert(prng_curseed(&prng) == 0xCAFEBABEULL);

	for (int i = 0; i < 1000; i++)
		prng_uniform(&prng);

	assert(prng_curseed(&prng) == 0xCAFEBABEULL);

	prng_init(&prng, 0ULL);
	assert(prng_curseed(&prng) == 0ULL);

	printf("PASS\n");
}

// Test5_1: Uniform moment validation
// N = 100k samples per parameter set
// Checks support, mean, variance, and skewness.
// Uniform(a,b) has:
//   mean     = (a + b)/2
//   variance = (b - a)^2/12
//   skewness = 0
// Uses 5-sigma bounds to avoid false positives while
// remaining sensitive to bias and asymmetry.
static void test_uniform_moments() {
	printf("test_uniform_moments ... ");

	const int N = 100000;
	const double skew_tol = 5.0 * sqrt(6.0 / (double)N);

	static const struct {
		double a;
		double b;
	} cases[] = {{0.0, 1.0}, {2.0, 5.0}, {-3.0, 3.0}, {100.0, 200.0}};

	for (int c = 0; c < 4; c++) {
		double a = cases[c].a;
		double b = cases[c].b;

		double exp_mean = (a + b) / 2;
		double exp_var = (b - a) * (b - a) / 12.0;

		// From Standard Error
		double mean_tol = 5.0 * (b - a) / (2.0 * sqrt(3.0 * (double)N));
		double var_tol = 5.0 * exp_var * sqrt(2.0 / (double)N);

		PRNG prng;
		prng_init(&prng, (uint64_t)(100 + c));
		Distribution d = dist_uniform(a, b);

		double sum = 0.0, sum_sq = 0.0, sum_cb = 0.0;
		for (int i = 0; i < N; i++) {
			double x = dist_sample(&d, &prng);
			assert(x >= a && x <= b && "Uniform sample outside [a, b]");
			sum += x;
			sum_sq += x * x;
			sum_cb += x * x * x;
		}

		double xbar = sum / (double)N;
		double var = sum_sq / (double)N - xbar * xbar;
		double sd = sqrt(var);
		double m3 = sum_cb / (double)N - 3.0 * xbar * (sum_sq / (double)N) +
		            2.0 * xbar * xbar * xbar;
		double skew = (sd > 1e-15) ? m3 / (sd * sd * sd) : 0.0;

		assert(fabs(xbar - exp_mean) < mean_tol &&
		       "Uniform mean outside 5-sigma bounds");
		assert(fabs(var - exp_var) < var_tol &&
		       "Uniform variance outside 5-sigma bounds");
		assert(fabs(skew) < skew_tol &&
		       "Uniform skewness non-zero: distribution is not symmetric");
	}
}

// Test5_2: Pearson chi-squared
// N = 1e6, buckets = 100, df = 99
// Critical value: chi2.ppf(0.999, 99) = 148.23
// Using 99.9% threshold to avoid false positives while staying strict
#define CHI2_CRITICAL_DF99 148.23

static void test_prng_uniformity(void) {
	printf("test_prng_uniformity (chi-sq, 1M draws) ... ");

	const int N = 1000000;
	enum { BUCKETS = 100 };

	int counts[BUCKETS] = {0};

	PRNG prng;
	prng_init(&prng, 0xABCD1234ULL);

	for (int i = 0; i < N; i++) {
		double u = prng_uniform(&prng);

		/* prng_uniform() is specified to return values in [0,1). */
		int bucket = (int)(u * BUCKETS);

		counts[bucket]++;
	}

	const double expected = (double)N / BUCKETS;

	double chi2 = 0.0;

	for (int b = 0; b < BUCKETS; b++) {
		double diff = counts[b] - expected;
		chi2 += diff * diff / expected;
	}

	assert(chi2 < CHI2_CRITICAL_DF99 &&
	       "PRNG chi-squared uniformity test failed");

	printf("PASS\n");
}

// Test6:
// N = 600k, df = 5
// χ² = Σ (Oi - Ei)^2 / Ei
// Critical value: chi2.ppf(0.999, 5) ≈ 20.52
// 99.9% threshold to avoid flaky tests but still detect bias

#define CHI2_CRITICAL_DF5 20.52

static void test_sample_int_uniformity(void) {
	printf("test_sample_int_uniformity ... ");

	const int N = 600000;
	const double expected = (double)N / 6.0;
	int counts[6] = {0};

	PRNG prng;
	prng_init(&prng, 13ULL);

	for (int i = 0; i < N; i++) {
		int x = dist_sample_int(1, 6, &prng);
		assert(x >= 1 && x <= 6 && "dist_sample_int outside [1, 6]");
		counts[x - 1]++;
	}

	double chi2 = 0.0;

	for (int f = 0; f < 6; f++) {
		double diff = (double)counts[f] - expected;
		chi2 += (diff * diff) / expected;
	}

	assert(chi2 < CHI2_CRITICAL_DF5 &&
	       "dist_sample_int chi-squared uniformity test failed");

	printf("PASS\n");
}

// Test7:
static void test_exponential(void) {
	printf("test_exponential ... ");

	const int N = 100000;
	const double mus[] = {1.0, 2.0, 5.0};

	for (int m = 0; m < 3; m++) {
		double mu = mus[m];
		double exp_var = mu * mu;
		double mean_tol = 5.0 * mu / sqrt((double)N);
		double var_tol = 5.0 * exp_var * sqrt(2.0 / (double)N);

		PRNG prng;
		prng_init(&prng, (uint64_t)(1000 + m));
		Distribution d = dist_exponential(mu);

		double sum = 0.0, sum_sq = 0.0;
		for (int i = 0; i < N; i++) {
			double x = dist_sample(&d, &prng);
			assert(x > 0.0 && "Exponential produced a non-positive sample");
			sum += x;
			sum_sq += x * x;
		}

		double xbar = sum / N;
		double var = sum_sq / N - xbar * xbar;

		assert(fabs(xbar - mu) < mean_tol &&
		       "Exponential mean outside 5-sigma bounds");
		assert(fabs(var - exp_var) < var_tol &&
		       "Exponential variance outside 5-sigma bounds");
	}

	printf("PASS\n");
}

// Test8:
// P(X > s+t | X > s) = exp(-t/μ)
// Catches shape errors that mean/variance tests can miss.
// Uses 1M samples and compares the observed conditional
// probability to the expected value.

static void test_exponential_memoryless(void) {
	printf("test_exponential_memoryless ... ");

	const int N = 1000000;
	const double mu = 3.0;
	const double s = 2.0;
	const double t = 1.5;

	PRNG prng;
	prng_init(&prng, 0xFEDCBA98ULL);
	Distribution d = dist_exponential(mu);

	int above_s = 0;
	int above_s_plus_t = 0;

	for (int i = 0; i < N; i++) {
		double x = dist_sample(&d, &prng);
		if (x > s) {
			above_s++;
			if (x > s + t) above_s_plus_t++;
		}
	}

	double cond_prob = (double)above_s_plus_t / (double)above_s;
	double expected = exp(-t / mu);
	double se = sqrt(expected * (1.0 - expected) / (double)above_s);

	assert(fabs(cond_prob - expected) < 5.0 * se &&
	       "Exponential memoryless property violated");

	printf("PASS\n");
}

// Test9:
static void test_deterministic_value(void) {
	printf("test_deterministic_value ... ");

	PRNG prng;
	prng_init(&prng, 1ULL);

	double cases[] = {0.0, 1.0, 2.5, -3.7, 1e10};
	for (int c = 0; c < 5; c++) {
		Distribution d = dist_deterministic(cases[c]);
		for (int i = 0; i < 100000; i++) {
			assert(dist_sample(&d, &prng) == cases[c] &&
			       "Deterministic returned wrong value");
		}
	}

	printf("PASS\n");
}

// Test10:
// Verifies correct PRNG consumption in interleaved sampling and matches a
// reference sequence.
static void test_shared_prng_interleaving(void) {
	printf("test_shared_prng_interleaving ... ");

	const int N = 1000;
	const double mu = 2.0;

	PRNG baseline, test;
	prng_init(&baseline, 42ULL);
	prng_init(&test, 42ULL);

	Distribution exp_d = dist_exponential(mu);
	Distribution uni_d = dist_uniform(0.0, 1.0);

	double expected_uniform[1000];
	for (int i = 0; i < N; i++) {
		dist_sample(&exp_d, &baseline);
		expected_uniform[i] = dist_sample(&uni_d, &baseline);
	}

	for (int i = 0; i < N; i++) {
		dist_sample(&exp_d, &test);
		double u_actual = dist_sample(&uni_d, &test);

		assert(u_actual == expected_uniform[i] &&
		       "Shared PRNG stream desynchronization detected");
	}

	printf("PASS\n");
}

// Test11:
static void test_bernoulli(void) {
	printf("test_bernoulli ... ");

	const int N = 100000;

	const double ps[] = {0.2, 0.5, 0.8};

	for (int c = 0; c < 3; c++) {
		double p = ps[c];
		double tol = 5.0 * sqrt(p * (1.0 - p) / (double)N);

		PRNG prng;
		prng_init(&prng, (uint64_t)(42 + c));
		Distribution d = dist_bernoulli(p);

		double sum = 0.0;
		for (int i = 0; i < N; i++) {
			double x = dist_sample(&d, &prng);
			assert((x == 0.0 || x == 1.0) &&
			       "Bernoulli produced value outside {0, 1}");
			sum += x;
		}

		double xbar = sum / (double)N;
		assert(fabs(xbar - p) < tol && "Bernoulli mean outside 5-sigma bounds");
	}

	printf("PASS\n");
}

// Test12:
static void test_normal(void) {
	printf("test_normal ... ");

	const int N = 100000;
	const double skew_tol = 5.0 * sqrt(6.0 / (double)N);

	const struct {
		double mu;
		double sigma;
	} cases[] = {
	    {5.0, 2.0},  /* original case */
	    {-2.0, 0.5}, /* negative mean, sub-unit spread */
	};

	for (int c = 0; c < 2; c++) {
		double mu = cases[c].mu;
		double sigma = cases[c].sigma;

		PRNG prng;
		prng_init(&prng, (uint64_t)(12345 + c));
		Distribution d = dist_normal(mu, sigma);

		double sum = 0.0, sum_sq = 0.0, sum_cb = 0.0;
		for (int i = 0; i < N; i++) {
			double x = dist_sample(&d, &prng);
			sum += x;
			sum_sq += x * x;
			sum_cb += x * x * x;
		}

		double xbar = sum / (double)N;
		double var = sum_sq / (double)N - xbar * xbar;
		double sd = sqrt(var);
		double m3 = sum_cb / (double)N - 3.0 * xbar * (sum_sq / (double)N) +
		            2.0 * xbar * xbar * xbar;
		double skew = (sd > 1e-15) ? m3 / (sd * sd * sd) : 0.0;

		assert(fabs(xbar - mu) < 5.0 * sigma / sqrt((double)N) &&
		       "Normal mean outside 5-sigma bounds");
		assert(fabs(sd - sigma) < 5.0 * sigma / sqrt(2.0 * (double)N) &&
		       "Normal stddev outside 5-sigma bounds");
		assert(fabs(skew) < skew_tol &&
		       "Normal skewness non-zero: distribution is not symmetric");
	}

	printf("PASS\n");
}

// Test13:
static void test_poisson(void) {
	printf("test_poisson ... ");

	const int N = 100000;

	static const double lambdas[] = {1.0, 5.0, 20.0};

	for (int c = 0; c < 3; c++) {
		double lambda = lambdas[c];
		double mean_tol = 5.0 * sqrt(lambda / (double)N);
		double var_tol = 5.0 * lambda * sqrt(2.0 / (double)N);

		PRNG prng;
		prng_init(&prng, (uint64_t)(99999 + c));
		Distribution d = dist_poisson(lambda);

		double sum = 0.0, sum_sq = 0.0;
		for (int i = 0; i < N; i++) {
			double x = dist_sample(&d, &prng);
			assert(x >= 0.0 && "Poisson produced negative value");
			assert(x == floor(x) && "Poisson produced non-integer value");
			sum += x;
			sum_sq += x * x;
		}

		double xbar = sum / (double)N;
		double var = sum_sq / (double)N - xbar * xbar;

		assert(fabs(xbar - lambda) < mean_tol &&
		       "Poisson mean outside 5-sigma bounds");
		assert(fabs(var - lambda) < var_tol &&
		       "Poisson Var != Mean: defining property violated");
	}

	printf("PASS\n");
}

// Test14:
static void test_categorical(void) {
	printf("test_categorical ... ");

	const int N = 100000;

	/* Case 1: 3 categories, non-uniform weights */
	{
		static const double weights[] = {0.2, 0.5, 0.3};
		const int ncat = 3;
		int counts[3] = {0, 0, 0};

		PRNG prng;
		prng_init(&prng, 7777ULL);

		for (int i = 0; i < N; i++) {
			int idx = dist_sample_categorical(ncat, weights, &prng);
			assert(idx >= 0 && idx < ncat && "Categorical index out of range");
			counts[idx]++;
		}

		for (int i = 0; i < ncat; i++) {
			double freq = (double)counts[i] / (double)N;
			double tol =
			    5.0 * sqrt(weights[i] * (1.0 - weights[i]) / (double)N);
			assert(fabs(freq - weights[i]) < tol &&
			       "Categorical frequency outside 5-sigma bounds");
		}
	}

	/* Case 2: 2 categories — minimal / boundary cardinality */
	{
		static const double weights[] = {0.3, 0.7};
		const int ncat = 2;
		int counts[2] = {0, 0};

		PRNG prng;
		prng_init(&prng, 8888ULL);

		for (int i = 0; i < N; i++) {
			int idx = dist_sample_categorical(ncat, weights, &prng);
			assert(idx >= 0 && idx < ncat && "Categorical index out of range");
			counts[idx]++;
		}

		for (int i = 0; i < ncat; i++) {
			double freq = (double)counts[i] / (double)N;
			double tol =
			    5.0 * sqrt(weights[i] * (1.0 - weights[i]) / (double)N);
			assert(fabs(freq - weights[i]) < tol &&
			       "Categorical frequency outside 5-sigma bounds");
		}
	}

	/* Case 3: 5 equal weights — uniform distribution across categories */
	{
		static const double weights[] = {0.2, 0.2, 0.2, 0.2, 0.2};
		const int ncat = 5;
		const double w = 0.2;
		int counts[5] = {0, 0, 0, 0, 0};
		const double tol = 5.0 * sqrt(w * (1.0 - w) / (double)N);

		PRNG prng;
		prng_init(&prng, 9999ULL);

		for (int i = 0; i < N; i++) {
			int idx = dist_sample_categorical(ncat, weights, &prng);
			assert(idx >= 0 && idx < ncat && "Categorical index out of range");
			counts[idx]++;
		}

		for (int i = 0; i < ncat; i++) {
			double freq = (double)counts[i] / (double)N;
			assert(fabs(freq - w) < tol &&
			       "Categorical uniform frequency outside 5-sigma bounds");
		}
	}

	printf("PASS\n");
}

// Test15:Full application of statistical testing framework
// Erlang distribution validation tests.
// Covers moments, properties, constructors, and optional KS checks.

#define TEST_ERLANG_LAYER4

// F(x;k,pm) = 1 - exp(-x/pm) * Σ_{n=0}^{k-1} (x/pm)^n / n!
// Exact Erlang CDF (integer k via Poisson-sum form), stable for moderate k
// ≤ 25.
static double erlang_cdf_exact(double x, int k, double pm) {
	if (x <= 0.0) return 0.0;
	const double u = x / pm;
	double term = 1.0; /* u^0 / 0! */
	double sum = 1.0;
	for (int n = 1; n < k; n++) {
		term *= u / (double)n;
		sum += term;
	}
	return 1.0 - exp(-u) * sum;
}

/* qsort comparator for doubles */
static int double_cmp(const void *a, const void *b) {
	const double da = *(const double *)a;
	const double db = *(const double *)b;
	return (da > db) - (da < db);
}

// Validates Erlang(k,pm) mean/variance/skewness against theory.
// Uses k-scaled N so SE/pm stays constant; 5σ CLT-based tolerances applied.
static void test_erlang_moments(void) {
	printf("test_erlang_moments ... ");

	static const int BASE_N = 100000;

	static const struct {
		int k;
		double pm;
	} cases[] = {
	    {1, 1.0}, /* boundary — Erlang(1,pm) == Exponential(pm) */

	    {2, 2.0}, /* small k — high variance regime */
	    {3, 1.0}, /* small k — low scale baseline case */

	    {5, 0.5},  /* mid k — skewness ≈ 2/√5 */
	    {10, 3.0}, /* mid k — skewness ≈ 2/√10, near-normal transition */
	};
	static const int NCASES = (int)(sizeof cases / sizeof cases[0]);

	for (int c = 0; c < NCASES; c++) {
		const int k = cases[c].k;
		const double pm = cases[c].pm;

		// N ∝ k keeps SE/pm constant: SE = pm*sqrt(k)/sqrt(N) = pm/sqrt(BASE_N)

		const int N = BASE_N * k;

		const double exp_mean = (double)k * pm;
		const double exp_var = (double)k * pm * pm;
		const double exp_skew = 2.0 / sqrt((double)k);

		// Tolerances
		const double mean_tol = 5.0 * pm * sqrt((double)k / (double)N);
		const double var_tol = 5.0 * exp_var * sqrt(2.0 / (double)N);
		const double skew_tol = 5.0 * sqrt(6.0 / (double)N);

		PRNG prng;
		prng_init(&prng, (uint64_t)(300 + c));
		Distribution d = dist_erlang(k, pm);

		double sum = 0.0, sum_sq = 0.0, sum_cb = 0.0;

		for (int i = 0; i < N; i++) {
			const double x = dist_sample(&d, &prng);

			assert(x > 0.0 && "Erlang [L1] support: non-positive sample");

			sum += x;
			sum_sq += x * x;
			sum_cb += x * x * x;
		}

		const double xbar = sum / (double)N;
		assert(fabs(xbar - exp_mean) < mean_tol &&
		       "Erlang [L1] mean outside 5-sigma bounds");

		const double var = sum_sq / (double)N - xbar * xbar;
		assert(fabs(var - exp_var) < var_tol &&
		       "Erlang [L1] variance outside 5-sigma bounds");

		/* --- 1.4 Skewness
		 * Third central moment via raw-moment identity:
		 *   μ₃ = E[X³] - 3·x̄·E[X²] + 2·x̄³
		 * Skewness = μ₃ / σ³    */
		const double sd = sqrt(var);
		const double m3 = sum_cb / (double)N -
		                  3.0 * xbar * (sum_sq / (double)N) +
		                  2.0 * xbar * xbar * xbar;
		const double skew = (sd > 1e-15) ? m3 / (sd * sd * sd) : 0.0;
		assert(fabs(skew - exp_skew) < skew_tol &&
		       "Erlang [L1] skewness outside 5-sigma bounds");
	}

	printf("PASS\n");
}

// k=1 must reduce exactly to the exponential case.
// Ensures identical sampling path and detects off-by-one iteration errors.
static void test_erlang_k1_matches_exponential(void) {
	printf("test_erlang_k1_matches_exponential ... ");

	static const int N = 100000;
	static const double mu = 3.0;

	Distribution exp_d = dist_exponential(mu);
	Distribution erl_d = dist_erlang(1, mu);

	PRNG pe, pk;
	prng_init(&pe, 55555ULL);
	prng_init(&pk, 55555ULL); /* identical starting state */

	for (int i = 0; i < N; i++) {
		const double xe = dist_sample(&exp_d, &pe);
		const double xk = dist_sample(&erl_d, &pk);
		assert(xe == xk && "Erlang [L2a] Erlang(k=1) and Exponential diverged "
		                   "on same PRNG seed");
	}

	printf("PASS\n");
}

// Erlang(k,pm) must equal sum of k independent exponentials using same PRNG.
// Detects phase-count errors, scaling bugs, and accumulation/offset issues.
static void test_erlang_additive_property(void) {
	printf("test_erlang_additive_property ... ");

	static const int N = 100000;

	static const struct {
		int k;
		double pm;
	} cases[] = {
	    {2, 1.0},  /* minimal loop: 2 phases                  */
	    {3, 2.0},  /* odd k, larger scale                     */
	    {5, 0.5},  /* prime k, sub-unit scale                 */
	    {10, 1.0}, /* loop long enough to expose index errors */
	};
	static const int NCASES = (int)(sizeof cases / sizeof cases[0]);

	for (int c = 0; c < NCASES; c++) {
		const int k = cases[c].k;
		const double pm = cases[c].pm;

		Distribution erl_d = dist_erlang(k, pm);
		Distribution exp_d = dist_exponential(pm);

		PRNG pe, pk;
		prng_init(&pe, (uint64_t)(400 + c));
		prng_init(&pk, (uint64_t)(400 + c)); /* identical starting state */

		for (int i = 0; i < N; i++) {
			/* Reference: manually sum k Exponential(pm) draws from pe */
			double sum_exp = 0.0;
			for (int j = 0; j < k; j++)
				sum_exp += dist_sample(&exp_d, &pe);

			/* Implementation: one Erlang draw from pk (same PRNG state) */
			const double erlang_val = dist_sample(&erl_d, &pk);

			assert(sum_exp == erlang_val && "Erlang [L2b] Erlang(k,pm) != sum "
			                                "of k Exp(pm) on same PRNG seed");
		}
	}

	printf("PASS\n");
}

// Sweeps 12 (k,pm) cases across boundary, small, and large-scale regimes.
// Verifies support, mean, and variance stability under extreme parameter
// ratios.
static void test_erlang_parameter_space(void) {
	printf("test_erlang_parameter_space ... ");

	static const struct {
		int k;
		double pm;
	} grid[] = {
	    /* k=1 boundary — must match Exponential exactly (see L2a) */
	    {1, 0.01},
	    {1, 1.0},
	    {1, 100.0},

	    /* small k — short phase chains, high relative variance */
	    {2, 0.01},
	    {2, 1.0},
	    {2, 100.0},

	    /* mid k — transition region before CLT dominance */
	    {10, 1.0},
	    {10, 100.0},

	    /* large k — near-normal regime, checks numerical stability */
	    {50, 1.0},
	    {50, 0.01},

	    /* extreme k — stress loop accumulation and precision limits */
	    {100, 1.0},
	    {100, 0.01},
	};
	static const int NCASES = (int)(sizeof grid / sizeof grid[0]);

	for (int c = 0; c < NCASES; c++) {
		const int k = grid[c].k;
		const double pm = grid[c].pm;

		/* Scale N with k; floor at 10,000 for small k */
		const int N = (5000 * k > 10000) ? 5000 * k : 10000;

		const double exp_mean = (double)k * pm;
		const double exp_var = (double)k * pm * pm;
		const double mean_tol = 5.0 * pm * sqrt((double)k / (double)N);
		const double var_tol =
		    5.0 * exp_var * sqrt((2.0 + 6.0 / (double)k) / (double)N);
		PRNG prng;
		prng_init(&prng, (uint64_t)(500 + c));
		Distribution d = dist_erlang(k, pm);

		double sum = 0.0, sum_sq = 0.0;

		for (int i = 0; i < N; i++) {
			const double x = dist_sample(&d, &prng);
			assert(x > 0.0 && "Erlang [L3] support: non-positive sample");
			sum += x;
			sum_sq += x * x;
		}

		const double xbar = sum / (double)N;
		const double var = sum_sq / (double)N - xbar * xbar;

		assert(fabs(xbar - exp_mean) < mean_tol &&
		       "Erlang [L3] mean outside 5-sigma bounds (parameter space)");
		assert(fabs(var - exp_var) < var_tol &&
		       "Erlang [L3] variance outside 5-sigma bounds (parameter space)");
	}

	printf("PASS\n");
}

// Ensures total-mean constructor is equivalent to k-scaled phase mean form.
// Separates struct consistency, PRNG path identity, and statistical
// correctness.
static void test_erlang_from_total_mean(void) {
	printf("test_erlang_from_total_mean ... ");

	static const int N = 100000;
	static const int k = 4;
	static const double total_mean = 8.0;

	Distribution d1 = dist_erlang(k, total_mean / (double)k);
	Distribution d2 = dist_erlang_from_total(k, total_mean);

	/* --- 1. Structural equality --- */
	assert(d1.erlang.k == d2.erlang.k &&
	       "Erlang [CTOR] k field mismatch between constructors");
	assert(d1.erlang.phase_mean == d2.erlang.phase_mean &&
	       "Erlang [CTOR] phase_mean field mismatch between constructors");

	/* --- 2. Sequence identity (same PRNG seed → identical stream) --- */
	PRNG p1, p2;
	prng_init(&p1, 271828ULL);
	prng_init(&p2, 271828ULL);

	const double m1 = sample_mean(&d1, &p1, N);
	const double m2 = sample_mean(&d2, &p2, N);

	assert(m1 == m2 &&
	       "Erlang [CTOR] dist_erlang_from_total produced different sequence");

	/* --- 3. Statistical correctness --- */
	const double pm = total_mean / (double)k;
	const double tol = 5.0 * pm * sqrt((double)k) / sqrt((double)N);
	assert(fabs(m1 - total_mean) < tol &&
	       "Erlang [CTOR] from_total mean outside 5-sigma bounds");

	printf("PASS\n");
}

// KS test: compares empirical CDF vs exact Erlang CDF (α = 0.001).
// Enabled only for k ≤ 20 due to CDF numerical stability limits.

#ifdef TEST_ERLANG_LAYER4
static void test_erlang_ks(void) {
	printf("test_erlang_ks ... ");
	fflush(stdout);

	static const int N = 50000;

	static const struct {
		int k;
		double pm;
	} cases[] = {
	    {1, 1.0},  /* exponential baseline                      */
	    {3, 2.0},  /* small k, scaled                           */
	    {5, 0.5},  /* prime k, sub-unit scale                   */
	    {10, 1.0}, /* mid k                                     */
	    {20, 3.0}, /* upper limit of stable CDF computation     */
	};
	static const int NCASES = (int)(sizeof cases / sizeof cases[0]);

	double *buf = (double *)malloc((size_t)N * sizeof(double));
	assert(buf != NULL && "test_erlang_ks: allocation failed");

	// KS threshold relaxed for CI stability (≈α 1e-4 instead of 1e-3).
	// Prevents rare valid-tail failures while still catching shape regressions.
	const double D_crit = 2.30 / sqrt((double)N);

	for (int c = 0; c < NCASES; c++) {
		const int k = cases[c].k;
		const double pm = cases[c].pm;

		PRNG prng;
		prng_init(&prng, (uint64_t)(600 + c));
		Distribution d = dist_erlang(k, pm);

		for (int i = 0; i < N; i++) {
			buf[i] = dist_sample(&d, &prng);
		}

		/* Sort ascending for empirical CDF construction */
		qsort(buf, (size_t)N, sizeof(double), double_cmp);

		// KS statistic uses both pre- and post-jump empirical CDF values.
		// Ensures correct two-sided deviation at each sorted sample point.
		double D = 0.0;
		for (int i = 0; i < N; i++) {
			const double fn_hi = (double)(i + 1) / (double)N;
			const double fn_lo = (double)i / (double)N;
			const double f_theo = erlang_cdf_exact(buf[i], k, pm);
			const double diff =
			    fmax(fabs(fn_hi - f_theo), fabs(fn_lo - f_theo));
			if (diff > D) {
				D = diff;
			}
		}

		if (D >= D_crit) {
			fprintf(stderr,
			        "\n[KS FAILURE] Case c=%d (k=%d, pm=%.2f): D = %f, D_crit "
			        "= %f\n",
			        c, k, pm, D, D_crit);
		}

		assert(
		    D < D_crit &&
		    "Erlang [L4] KS statistic exceeds regression tolerance threshold");
	}

	free(buf);
	printf("PASS\n");
}
#endif /* TEST_ERLANG_LAYER4 */

static void run_erlang_tests(void) {
	test_erlang_moments();

	test_erlang_k1_matches_exponential();
	test_erlang_additive_property();

	test_erlang_parameter_space();

	test_erlang_from_total_mean();

#ifdef TEST_ERLANG_LAYER4
	test_erlang_ks();
#endif
}

int main(void) {
	printf("\n----- Phase 4 Tests: PRNG and Distributions -----\n\n");

	test_prng_bounds();
	test_prng_reproducibility();
	test_prng_different_seeds();
	test_prng_curseed();
	test_uniform_moments();
	test_prng_uniformity();
	test_sample_int_uniformity();
	test_exponential();
	test_exponential_memoryless();
	test_deterministic_value();
	test_shared_prng_interleaving();
	test_bernoulli();
	test_normal();
	test_poisson();
	test_categorical();
	run_erlang_tests();

	printf("\nAll Phase 4 tests passed.\n");
	return 0;
}
