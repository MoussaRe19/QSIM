#include <stdlib.h>
#include <stdio.h>

#include "kernel.h"

void (*kernel_on_fatal)(const char *msg) = kernel_default_fatal;

void kernel_default_fatal(const char *msg) {
	fprintf(stderr, "FATAL: %s", msg);
	abort();
}

void kernel_init(KernelState *k) {
	k->clock = 0.0;
	k->event_id_ctr = 1;
	k->terminted = false;
	fel_init(&k->fel);
}

void kernel_destroy(KernelState *k) {
	fel_destroy(&k->fel);
}