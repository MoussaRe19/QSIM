/* xoshiro256** PRNG by Blackman & Vigna
 * (https://prng.di.unimi.it/xoshiro256starstar.c). */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <assert.h>
#include "prng.h"

/* splitmix64 — simple 64-bit mix function used for PRNG seeding. */
static uint64_t splitmix64(uint64_t *state) {
	uint64_t z = (*state += UINT64_C(0x9e3779b97f4a7c15));
	z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
	return z ^ (z >> 31);
}

static inline uint64_t rotl(const uint64_t x, int k) {
	return (x << k) | (x >> (64 - k));
}

static uint64_t xoshiro256ss_next(PRNG *prng) {
	const uint64_t result = rotl(prng->s[1] * UINT64_C(5), 7) * UINT64_C(9);
	const uint64_t t = prng->s[1] << 17;

	prng->s[2] ^= prng->s[0];
	prng->s[3] ^= prng->s[1];
	prng->s[1] ^= prng->s[2];
	prng->s[0] ^= prng->s[3];
	prng->s[2] ^= t;
	prng->s[3] = rotl(prng->s[3], 45);

	return result;
}

void prng_init(PRNG *prng, uint64_t seed) {
	prng->initial_seed = seed;

	uint64_t sm = seed;
	prng->s[0] = splitmix64(&sm);
	prng->s[1] = splitmix64(&sm);
	prng->s[2] = splitmix64(&sm);
	prng->s[3] = splitmix64(&sm);

	assert((prng->s[0] | prng->s[1] | prng->s[2] | prng->s[3]) != 0 &&
	       "prng_init: all-zero state after seed expansion — xoshiro invariant "
	       "violated");
}

/* prng_uniform — returns a uniform random double in (0, 1), with safe scaling.
 */
double prng_uniform(PRNG *prng) {
	uint64_t raw = xoshiro256ss_next(prng);
	double u = (double)(raw >> 11) * (1.0 / (double)(UINT64_C(1) << 53));
	if (u == 0.0) u = DBL_MIN; /* zero guard*/
	return u;
}

void prng_jump(PRNG *prng)
{
    (void)prng;
    /* The jump polynomial for xoshiro256** requires 4 × 64-bit coefficients
     * published by Blackman & Vigna.  Implement when multi-stream variance
     * reduction is needed (Phase 6+). */
    assert(0 && "prng_jump: not yet implemented — deferred to Phase 6+");
}


uint64_t prng_curseed(const PRNG *prng) {
    return prng->initial_seed;
}