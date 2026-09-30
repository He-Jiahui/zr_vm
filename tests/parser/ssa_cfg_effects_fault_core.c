#include "ssa_cfg_effects_fault_allocator.h"

#include <stdlib.h>

static size_t failureOrdinal;
static size_t allocationOrdinal;
static TZrBool failed;

void ssa_cfg_effects_fault_fail_reallocation(size_t ordinal) {
    failureOrdinal = ordinal;
    allocationOrdinal = 0u;
    failed = ZR_FALSE;
}

size_t ssa_cfg_effects_fault_reallocation_attempts(void) {
    return allocationOrdinal;
}

TZrBool ssa_cfg_effects_fault_reallocation_failed(void) {
    return failed;
}

static void *ssa_cfg_effects_fault_realloc(void *storage, size_t bytes) {
    ++allocationOrdinal;
    if (failureOrdinal != 0u && allocationOrdinal == failureOrdinal) {
        failed = ZR_TRUE;
        return NULL;
    }
    return realloc(storage, bytes);
}

/* Compile the real core append-pool implementation with only its allocator
 * redirected. This makes the test hit the actual phi/token pool reallocs. */
#define realloc ssa_cfg_effects_fault_realloc
#include "../../zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c"
#undef realloc
