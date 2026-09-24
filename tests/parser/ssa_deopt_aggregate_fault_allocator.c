#include "ssa_deopt_aggregate_fault_allocator.h"

#include <stdlib.h>

static size_t failureOrdinal;
static size_t allocationOrdinal;
static int failed;

void ssa_deopt_aggregate_fail_allocation(size_t ordinal) {
    failureOrdinal = ordinal;
    allocationOrdinal = 0u;
    failed = 0;
}

int ssa_deopt_aggregate_allocation_failed(void) { return failed; }

static void *aggregate_malloc(size_t size) {
    if (++allocationOrdinal == failureOrdinal) {
        failed = 1;
        return NULL;
    }
    return malloc(size);
}

/* The exact production preparation module runs with deterministic failures.
 * Storage lifecycle remains linked normally and uses the matching allocator. */
#define malloc aggregate_malloc
#include "../../zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c"
