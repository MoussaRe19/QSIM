#ifndef MM1_INIT_H
#define MM1_INIT_H

#include <stdint.h>
#include "dispatch.h"

typedef struct {
	double arrival_mean;
	double service_mean;
	uint64_t seed;
} MM1_Config;

void mm1_init(MM1_Config cfg);

InterpretResult mm1_run(double tau_max);

void mm1_print_counts(void);

#endif