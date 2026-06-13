#ifndef QSIM_PRNG_H
#define QSIM_PRNG_H

#include <stdint.h>

// State
typedef struct {
	uint64_t s[4];
	uint64_t initial_seed; // For reporting
} PRNG;

void prng_init(PRNG *prng, uint64_t seed);

double prng_uniform(PRNG *prng);

/* Advance the PRNG by 2^128 steps to create a non-overlapping stream (not yet
 * implemented). */
void prng_jump(PRNG *prng);

// Return initialization seed; UINT64_MAX if never initialized.
uint64_t prng_curseed(const PRNG *prng);

#endif