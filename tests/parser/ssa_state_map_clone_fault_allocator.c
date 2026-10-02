#include "ssa_state_map_clone_fault_allocator.h"

#include <stdlib.h>

/* Only clone-owned pools use this intercepted allocator. Keep the
 * pointer ledger bounded without allocating any instrumentation storage. */
#define SSA_STATE_MAP_CLONE_TRACKED_POOLS (3u * 4u)

static size_t failureOrdinal;
static size_t allocationOrdinal;
static int failed;
static void *allocations[SSA_STATE_MAP_CLONE_TRACKED_POOLS];
static size_t outstanding;

void ssa_state_map_clone_fail_allocation(size_t ordinal) {
    failureOrdinal = ordinal;
    allocationOrdinal = 0u;
    failed = 0;
}

int ssa_state_map_clone_allocation_failed(void) { return failed; }
size_t ssa_state_map_clone_allocation_attempts(void) { return allocationOrdinal; }
size_t ssa_state_map_clone_outstanding_allocations(void) { return outstanding; }

static void *state_map_clone_malloc(size_t size) {
    void *result;
    size_t index;

    ++allocationOrdinal;
    if (failureOrdinal != 0u && allocationOrdinal == failureOrdinal) {
        failed = 1;
        return NULL;
    }
    result = malloc(size);
    if (result != NULL) {
        for (index = 0u; index < SSA_STATE_MAP_CLONE_TRACKED_POOLS; ++index) {
            if (allocations[index] == NULL) {
                allocations[index] = result;
                ++outstanding;
                return result;
            }
        }
        /* Exhausting instrumentation cannot silently hide an allocation. */
        abort();
    }
    return result;
}

static void state_map_clone_free(void *memory) {
    size_t index;

    for (index = 0u; index < SSA_STATE_MAP_CLONE_TRACKED_POOLS; ++index) {
        if (memory != NULL && allocations[index] == memory) {
            allocations[index] = NULL;
            --outstanding;
            break;
        }
    }
    free(memory);
}

/* Exercise the unchanged public clone and lifecycle bodies only in this test
 * executable. Other targets compile the ordinary production translation unit. */
#define malloc state_map_clone_malloc
#define free state_map_clone_free
#include "../../zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c"
