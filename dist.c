#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <assert.h>
#include <stdio.h>

#include "prng.h"
#include "dist.h"

double dist_sample(Distribution *d, PRNG *prng) {
	switch (d->type) {
	case DIST_EXPONENTIAL:
		return -d->mean * log(prng_uniform(prng));

	case DIST_DETERMINISTIC:
		(void)prng;
		return d->value;

	case DIST_ERLANG: {
		double sum = 0.0;
		for (int i = 0; i < d->erlang.k; i++) {
			sum += -d->erlang.phase_mean * log(prng_uniform(prng));
		}
		return sum;
	}

	case DIST_UNIFORM:
		return d->uniform.a +
		       (d->uniform.b - d->uniform.a) * prng_uniform(prng);

	case DIST_BERNOULLI:
		return (prng_uniform(prng) < d->p) ? 1.0 : 0.0;

	case DIST_NORMAL: {
		/* Box-Muller transform: one normal sample from two uniform draws; no
		 * caching. */
		double u1 = prng_uniform(prng);
		double u2 = prng_uniform(prng);
		double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
		return d->normal.mean + d->normal.stddev * z;
	}

	case DIST_POISSON: {
		/* Knuth's algorithm: exact Poisson sampler, efficient for small lambda.
		 */
		double L = exp(-d->lambda);
		double p = 1.0;
		int k = 0;
		do {
			k++;
			p *= prng_uniform(prng);
		} while (p > L);
		return (double)(k - 1);
	}
	}

	/* Unreachable unless an unknown DistType is encountered. */
	fprintf(
	    stderr,
	    "FATAL: dist_sample reached unreachable branch — unknown DistType\n");
	assert(0);
	return 0.0;
}

// floor(a + (b - a + 1) * U) maps Uniform(0,1) to integers [a, b].
int dist_sample_int(int a, int b, PRNG *prng) {
	assert(a <= b && "dist_sample_int: a must be <= b");

	return a + (int)(((double)(b - a + 1)) * prng_uniform(prng));
}

int dist_sample_categorical(int n, const double weights[], PRNG *prng) {
	assert(n > 0);
	assert(weights != NULL);

	double u = prng_uniform(prng);
	double cumulative = 0.0;

	for (int i = 0; i < n - 1; i++) {
		cumulative += weights[i];
		if (u < cumulative) {
			return i;
		}
	}

	return n - 1;
}

double dist_sample_hyperexponential(int n, const double means[],
                                    const double weights[], PRNG *prng) {
	assert(n > 0);
	assert(means != NULL && weights != NULL);

	int idx = dist_sample_categorical(n, weights, prng);
	Distribution exp_d = dist_exponential(means[idx]);
	return dist_sample(&exp_d, prng);
}

double dist_accept_reject(pdf_func target_pdf, pdf_func proposal_pdf,
                          Distribution proposal, double M, PRNG *prng) {
    assert(target_pdf != NULL);
    assert(proposal_pdf != NULL); 
    assert(M > 0.0); 

    for(;;) {
        double x = dist_sample(&proposal, prng);
        double u = prng_uniform(prng);
        double g = proposal_pdf(x); 
        assert(g > 0.0 && "accept-reject: proposal density is zero at sampled point");
        double ratio = target_pdf(x) / (M * g);
        if(u <= ratio) {
            return x;
        }
        /* Reject and try again. The loop terminates in expected M iterations. */
    }
}