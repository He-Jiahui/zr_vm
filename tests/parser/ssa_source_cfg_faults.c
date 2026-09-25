#include "ssa_source_cfg_faults.h"
#include <stdlib.h>
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

static size_t failureOrdinal, allocationOrdinal, outstanding;
static TZrBool failed;
static ESsaSourceCfgPromotionFault promotionFault;

void ssa_source_cfg_fail_promotion(ESsaSourceCfgPromotionFault fault) {
    promotionFault = fault;
}

static TZrBool source_cfg_ensure_active(SZrCompilerState *compiler) {
    TZrBool activated = compiler_semantic_cfg_ensure_active(compiler);
    return (TZrBool)(activated &&
            promotionFault != SSA_SOURCE_CFG_PROMOTION_AFTER_ACTIVATION);
}

static TZrBool source_cfg_finish(SZrCompilerState *compiler) {
    TZrBool finished = compiler_semantic_cfg_finish(compiler);
    return (TZrBool)(finished &&
            promotionFault != SSA_SOURCE_CFG_PROMOTION_AFTER_FINISH);
}

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

/* Intercept preflight scratch allocations and report failure after the real
 * promotion helpers mutate state. No production allocation hook is added.
 * Compile inside the parser DLL where private helpers reside. */
#define compiler_semantic_cfg_finalize ssa_source_cfg_finalize
#define compiler_semantic_cfg_ensure_active source_cfg_ensure_active
#define compiler_semantic_cfg_finish source_cfg_finish
#define realloc source_cfg_realloc
#define free source_cfg_free
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_finalize.c"
