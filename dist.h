#ifndef QSIM_DIST_H
#define QSIM_DIST_H

#include "prng.h"
#include <assert.h>

typedef enum {
	DIST_EXPONENTIAL,   /* X = -mean * ln(U) */
	DIST_DETERMINISTIC, /* X = constant value */
	DIST_ERLANG,        /* sum of k exponential phases */
	DIST_UNIFORM,       /* X = a + (b-a)*U */
	DIST_BERNOULLI,     /* X ∈ {0,1}, P(X=1)=p */
	DIST_NORMAL,        /* Gaussian distribution */
	DIST_POISSON        /* Poisson arrival process */
} DistType;

typedef struct {
	DistType type;
	union {
		double mean;  /* EXPONENTIAL */
		double value; /* DETERMINISTIC */
		struct {
			int k;
			double phase_mean;
		} erlang; /* ERLANG */
		struct {
			double a;
			double b;
		} uniform; /* UNIFORM */

		double p; /* BERNOULLI */
		struct {
			double mean;
			double stddev;
		} normal;      /* NORMAL */
		double lambda; /* POISSON */
	};
} Distribution;

// Returns one sample from the distribution.
double dist_sample(Distribution *d, PRNG *prng);

static inline Distribution dist_exponential(double mean) {
	assert(mean > 0.0 && "dist_exponential: mean must be strictly positive");
	Distribution d;
	d.type = DIST_EXPONENTIAL;
	d.mean = mean;
	return d;
}

static inline Distribution dist_deterministic(double value) {
	Distribution d;
	d.type = DIST_DETERMINISTIC;
	d.value = value;
	return d;
}

/* Parameterised by per-phase mean.
 * Total mean = k * phase_mean. */
static inline Distribution dist_erlang(int k, double phase_mean) {
	assert(k >= 1 && "dist_erlang: k must be >= 1");
	assert(phase_mean > 0.0 &&
	       "dist_erlang: phase_mean must be strictly positive");
	Distribution d;
	d.type = DIST_ERLANG;
	d.erlang.k = k;
	d.erlang.phase_mean = phase_mean;
	return d;
}

static inline Distribution dist_erlang_from_total(int k, double total_mean) {
	assert(k >= 1 && "dist_erlang_from_total: k must be >= 1");
	assert(total_mean > 0.0 &&
	       "dist_erlang_from_total: total_mean must be strictly positive");
	return dist_erlang(k, total_mean / (double)k);
}

static inline Distribution dist_uniform(double a, double b) {
	assert(a < b && "dist_uniform: a must be strictly less than b");
	Distribution d;
	d.type = DIST_UNIFORM;
	d.uniform.a = a;
	d.uniform.b = b;
	return d;
}

static inline Distribution dist_bernoulli(double p) {
	assert(p > 0.0 && p <= 1.0 && "dist_bernoulli: p must be in (0, 1]");
	Distribution d;
	d.type = DIST_BERNOULLI;
	d.p = p;
	return d;
}

static inline Distribution dist_normal(double mean, double stddev) {
	assert(stddev > 0.0 && "dist_normal: stddev must be strictly positive");
	Distribution d;
	d.type = DIST_NORMAL;
	d.normal.mean = mean;
	d.normal.stddev = stddev;
	return d;
}

static inline Distribution dist_poisson(double lambda) {
	assert(lambda > 0.0 && "dist_poisson: lambda must be strictly positive");
	/* Poisson distribution with rate lambda (Knuth method; accurate for small
	 * lambda <= 40). */
	Distribution d;
	d.type = DIST_POISSON;
	d.lambda = lambda;
	return d;
}

// uniform integer in [a, b] (inclusive).
int dist_sample_int(int a, int b, PRNG *prng);

/*
 * sample index from discrete probability weights.
 * weights must sum to 1.0.
 * O(n) linear scan (CDF inversion).
 */
int dist_sample_categorical(int n, const double weights[], PRNG *prng);

/*
 * Mixture of exponential distributions.
 * Selects a component by weight, then samples an exponential.
 */
double dist_sample_hyperexponential(int n, const double means[],
                             const double weights[], PRNG *prng);

/*
 * dist_accept_reject — rejection sampling for arbitrary distributions.
 * Uses proposal distribution and acceptance ratio f(x)/(M*g(x)).
 */
typedef double (*pdf_func)(double x);

double dist_sample_accept_reject(pdf_func target_pdf, pdf_func proposal_pdf,
                          Distribution proposal, double M, PRNG *prng);
#endif