#include "ssa_oracle_resume_fault_allocator.h"

#include <stdlib.h>

static size_t failureOrdinal;
static size_t allocationOrdinal;
static int failed;

void ssa_oracle_resume_fail_allocation(size_t ordinal) {
    failureOrdinal = ordinal;
    allocationOrdinal = 0u;
    failed = 0;
}

int ssa_oracle_resume_allocation_failed(void) { return failed; }

static int allocation_should_fail(void) {
    ++allocationOrdinal;
    if (failureOrdinal != 0u && allocationOrdinal == failureOrdinal) {
        failed = 1;
        return 1;
    }
    return 0;
}

static void *resume_malloc(size_t size) {
    if (allocation_should_fail()) return NULL;
    return malloc(size);
}

static void *resume_calloc(size_t count, size_t size) {
    if (allocation_should_fail()) return NULL;
    return calloc(count, size);
}

/* Compile the production consumer with local allocation interception.
 * OracleResultFree belongs to another translation unit, so ownership and
 * deallocation retain the ordinary allocator throughout this executable. */
#define malloc resume_malloc
#define calloc resume_calloc
#include "../../zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c"
