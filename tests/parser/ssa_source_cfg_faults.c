#include "ssa_source_cfg_faults.h"
#include <stdlib.h>
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

static size_t failureOrdinal, allocationOrdinal, outstanding;
static TZrBool failed;

void ssa_source_cfg_fail_allocation(size_t ordinal) {
    failureOrdinal = ordinal;
    allocationOrdinal = 0u;
    failed = ZR_FALSE;
}

TZrBool ssa_source_cfg_allocation_failed(void) { return failed; }
size_t ssa_source_cfg_outstanding_allocations(void) { return outstanding; }

static void *source_cfg_realloc(void *memory, size_t bytes) {
    TZrBool newAllocation = (TZrBool)(memory == NULL);
    void *result;
    ++allocationOrdinal;
    if (failureOrdinal != 0u && allocationOrdinal == failureOrdinal) {
        failed = ZR_TRUE;
        return NULL;
    }
    result = realloc(memory, bytes);
    if (result != NULL && newAllocation) ++outstanding;
    return result;
}

static void source_cfg_free(void *memory) {
    if (memory != NULL) --outstanding;
    free(memory);
}

/* Test only the new preflight scratch allocations. The graph operations and
 * compiler are the real implementations; no production allocation hook is
 * introduced. Compile inside the parser DLL where private helpers reside. */
#define compiler_semantic_cfg_finalize ssa_source_cfg_finalize
#define realloc source_cfg_realloc
#define free source_cfg_free
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_finalize.c"
