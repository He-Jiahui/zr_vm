#include "ssa_owner_fault_allocator.h"

#include <stdlib.h>

static size_t failureOrdinal;
static size_t allocationOrdinal;
static size_t outstanding;
static int failed;

void ssa_owner_fail_allocation(size_t ordinal) {
    failureOrdinal = ordinal;
    allocationOrdinal = 0u;
    failed = 0;
}

int ssa_owner_allocation_failed(void) { return failed; }
size_t ssa_owner_outstanding_allocations(void) { return outstanding; }

static int allocation_should_fail(void) {
    ++allocationOrdinal;
    if (failureOrdinal != 0u && allocationOrdinal == failureOrdinal) {
        failed = 1;
        return 1;
    }
    return 0;
}

static void *owner_malloc(size_t size) {
    void *result;
    if (allocation_should_fail()) return NULL;
    result = malloc(size);
    if (result != NULL) ++outstanding;
    return result;
}

static void *owner_calloc(size_t count, size_t size) {
    void *result;
    if (allocation_should_fail()) return NULL;
    result = calloc(count, size);
    if (result != NULL) ++outstanding;
    return result;
}

static void owner_free(void *memory) {
    if (memory != NULL) --outstanding;
    free(memory);
}

/* Compile the production analysis with allocation interception only in this
 * test executable. Other state-map suites link its ordinary translation unit. */
#define malloc owner_malloc
#define calloc owner_calloc
#define free owner_free
#include "../../zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c"
