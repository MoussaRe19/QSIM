#ifndef QSIM_KERNAL_H
#define QSIM_KERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "fel.h"

typedef struct {
	double clock;
	uint64_t event_id_ctr;
	bool terminted; // Stop flag "check the course"
	FEL fel;
} KernelState;

void kernel_init(KernelState *k);
void kernel_destroy(KernelState *k);

void clock_advance(KernelState *k, double new_time);
double clock_read(const KernelState *k);

extern void (*kernel_on_fatal)(const char *msg);
void kernel_default_fatal(const char *msg);

#endif