#ifndef ZR_TESTS_SSA_HOTPATCH_FAULT_CASES_H
#define ZR_TESTS_SSA_HOTPATCH_FAULT_CASES_H

#include "zr_vm_core/hotpatch_generation.h"

/* Shared fault-injection vocabulary for rollback/restricted-profile tests.
 * These are test controls, not production state: a harness may map each point
 * to an allocator/signature/publisher failure without changing the runtime
 * ABI. */
typedef enum EZrSsaHotPatchFaultPoint {
    ZR_SSA_HOTPATCH_FAULT_SIGNATURE = 0,
    ZR_SSA_HOTPATCH_FAULT_CONTENT,
    ZR_SSA_HOTPATCH_FAULT_PREPARE,
    ZR_SSA_HOTPATCH_FAULT_PUBLISH,
    ZR_SSA_HOTPATCH_FAULT_RETIRE,
    ZR_SSA_HOTPATCH_FAULT_CANCEL,
    ZR_SSA_HOTPATCH_FAULT_OOM,
    ZR_SSA_HOTPATCH_FAULT_STALE,
    ZR_SSA_HOTPATCH_FAULT_POINT_COUNT
} EZrSsaHotPatchFaultPoint;

typedef struct SZrSsaHotPatchFaultCase {
    EZrSsaHotPatchFaultPoint point;
    TZrBool preservesActiveGeneration;
    TZrBool requiresRetry;
} SZrSsaHotPatchFaultCase;

/* Short aliases mirror the names used in the rollback plan while keeping the
 * SSA-prefixed names available to tests that include several fault matrices. */
typedef EZrSsaHotPatchFaultPoint EZrHotPatchFaultPoint;
typedef SZrSsaHotPatchFaultCase SZrHotPatchFaultCase;
#define ZR_HOT_PATCH_FAULT_SIGNATURE ZR_SSA_HOTPATCH_FAULT_SIGNATURE
#define ZR_HOT_PATCH_FAULT_CONTENT ZR_SSA_HOTPATCH_FAULT_CONTENT
#define ZR_HOT_PATCH_FAULT_PREPARE ZR_SSA_HOTPATCH_FAULT_PREPARE
#define ZR_HOT_PATCH_FAULT_PUBLISH ZR_SSA_HOTPATCH_FAULT_PUBLISH
#define ZR_HOT_PATCH_FAULT_RETIRE ZR_SSA_HOTPATCH_FAULT_RETIRE
#define ZR_HOT_PATCH_FAULT_CANCEL ZR_SSA_HOTPATCH_FAULT_CANCEL
#define ZR_HOT_PATCH_FAULT_OOM ZR_SSA_HOTPATCH_FAULT_OOM
#define ZR_HOT_PATCH_FAULT_STALE ZR_SSA_HOTPATCH_FAULT_STALE

static inline const TZrChar *ZrTests_SsaHotPatchFaultPointName(
        EZrSsaHotPatchFaultPoint point) {
    switch (point) {
        case ZR_SSA_HOTPATCH_FAULT_SIGNATURE: return "signature";
        case ZR_SSA_HOTPATCH_FAULT_CONTENT: return "content";
        case ZR_SSA_HOTPATCH_FAULT_PREPARE: return "prepare";
        case ZR_SSA_HOTPATCH_FAULT_PUBLISH: return "publish";
        case ZR_SSA_HOTPATCH_FAULT_RETIRE: return "retire";
        case ZR_SSA_HOTPATCH_FAULT_CANCEL: return "cancel";
        case ZR_SSA_HOTPATCH_FAULT_OOM: return "oom";
        case ZR_SSA_HOTPATCH_FAULT_STALE: return "stale";
        default: return "unknown";
    }
}

#endif
