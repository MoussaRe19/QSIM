#ifndef QSIM_DISPATCH_H
#define QSIM_DISPATCH_H

#include "context.h"

// Execution outcomes
typedef enum {
	INTERPRET_OK,
	INTERPRET_RUNTIME_ERROR
	/* future: INTERPRET_PAUSED, INTERPRET_STEPPED, INTERPRET_WATCHPOINT   */
} InterpretResult;

bool termination_met(const KernelState *kernel, const TerminationCondition *tc);

InterpretResult dispatch_loop(Context *ctx, const TerminationCondition *tc);

#endif