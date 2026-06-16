#include "kernel.h"

void clock_advance(KernelState *k, double new_time) {
	if (new_time < k->clock) {
		kernel_on_fatal("INV1: monotoicity violation - "
		                "attempt to move clock backwards");
	}
	k->clock = new_time;
}

double clock_read(const KernelState *k) {
	return k->clock;
}
