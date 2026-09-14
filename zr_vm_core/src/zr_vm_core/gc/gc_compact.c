#include "zr_vm_core/gc_compact.h"

#include <limits.h>
#include <string.h>

static void gc_compact_diag(SZrGcCompactDiagnostic *diagnostic,
                            EZrGcCompactDiagnosticCode code,
                            TZrUInt32 index,
                            TZrUInt64 expected,
                            TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->code = code;
    diagnostic->index = index;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

static TZrUInt64 gc_compact_sat_add(TZrUInt64 left, TZrUInt64 right) {
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
}

static TZrBool gc_compact_threshold_met(TZrUInt64 reclaimable,
                                         TZrUInt64 used,
                                         TZrUInt32 thresholdPercent) {
    TZrUInt64 whole;
    TZrUInt64 remainder;
    TZrUInt64 required;
    TZrUInt64 extra;

    if (used == 0u) {
        return ZR_FALSE;
    }
    if (thresholdPercent == 0u) {
        return reclaimable != 0u;
    }
    whole = used / 100u;
    remainder = used % 100u;
    required = whole * (TZrUInt64)thresholdPercent;
    extra = (remainder * (TZrUInt64)thresholdPercent + 99u) / 100u;
    if (required > UINT64_MAX - extra) {
        return ZR_TRUE;
    }
    required += extra;
    return reclaimable >= required;
}

void ZrCore_GcCompact_DiagnosticClear(
        SZrGcCompactDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

const TZrChar *ZrCore_GcCompact_DiagnosticName(
        EZrGcCompactDiagnosticCode code) {
    switch (code) {
        case ZR_GC_COMPACT_DIAGNOSTIC_NONE:
            return "none";
        case ZR_GC_COMPACT_DIAGNOSTIC_INVALID_ARGUMENT:
            return "invalid-argument";
        case ZR_GC_COMPACT_DIAGNOSTIC_INVALID_MAGIC:
            return "invalid-magic";
        case ZR_GC_COMPACT_DIAGNOSTIC_INVALID_SCHEMA:
            return "invalid-schema";
        case ZR_GC_COMPACT_DIAGNOSTIC_INVALID_REGION:
            return "invalid-region";
        case ZR_GC_COMPACT_DIAGNOSTIC_OVERFLOW:
            return "overflow";
        case ZR_GC_COMPACT_DIAGNOSTIC_BUDGET:
            return "budget";
        default:
            return "unknown";
    }
}

void ZrCore_GcCompact_Init(SZrGcCompactPlan *plan) {
    if (plan == ZR_NULL) {
        return;
    }
    memset(plan, 0, sizeof(*plan));
    plan->magic = ZR_GC_COMPACT_CONTRACT_MAGIC;
    plan->schemaVersion = ZR_GC_COMPACT_CONTRACT_SCHEMA_VERSION;
    plan->mode = ZR_GC_COMPACT_MODE_NON_MOVING;
}

TZrBool ZrCore_GcCompact_Validate(
        const SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic) {
    ZrCore_GcCompact_DiagnosticClear(diagnostic);
    if (plan == ZR_NULL) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_ARGUMENT,
                        0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (plan->magic != ZR_GC_COMPACT_CONTRACT_MAGIC) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_MAGIC,
                        1u, ZR_GC_COMPACT_CONTRACT_MAGIC, plan->magic);
        return ZR_FALSE;
    }
    if (plan->schemaVersion != ZR_GC_COMPACT_CONTRACT_SCHEMA_VERSION) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_SCHEMA,
                        2u, ZR_GC_COMPACT_CONTRACT_SCHEMA_VERSION,
                        plan->schemaVersion);
        return ZR_FALSE;
    }
    if (plan->mode >= ZR_GC_COMPACT_MODE_COUNT ||
        plan->candidateRegionCount > plan->regionCount ||
        plan->pinnedRegionCount > plan->regionCount ||
        plan->plannedBytes > plan->movableBytes ||
        (plan->deferred && plan->eligible == ZR_FALSE &&
         plan->candidateRegionCount == 0u)) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_REGION,
                        3u, plan->regionCount, plan->candidateRegionCount);
        return ZR_FALSE;
    }
    if (plan->mode == ZR_GC_COMPACT_MODE_NON_MOVING &&
        plan->plannedBytes != 0u) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_BUDGET,
                        4u, 0u, plan->plannedBytes);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_GcCompact_Plan(
        const SZrGcCompactRequest *request,
        SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic) {
    SZrGcCompactPlan candidate;
    TZrSize index;
    TZrUInt64 plannedRegionBytes = 0u;
    TZrUInt32 plannedRegionCount = 0u;
    TZrBool budgetLimited = ZR_FALSE;

    ZrCore_GcCompact_DiagnosticClear(diagnostic);
    if (plan != ZR_NULL) {
        ZrCore_GcCompact_Init(plan);
    }
    if (request == ZR_NULL || plan == ZR_NULL ||
        (request->regionCount != 0u && request->regions == ZR_NULL) ||
        request->fragmentationThresholdPercent > 100u) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_ARGUMENT,
                        0u, 1u, request != ZR_NULL
                                       ? request->fragmentationThresholdPercent
                                       : 0u);
        return ZR_FALSE;
    }
    if (request->regionCount > (TZrSize)UINT32_MAX) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_OVERFLOW,
                        0u, UINT32_MAX, request->regionCount);
        return ZR_FALSE;
    }
    ZrCore_GcCompact_Init(&candidate);
    candidate.regionCount = (TZrUInt32)request->regionCount;
    candidate.mode = request->allowMoving
                             ? ZR_GC_COMPACT_MODE_SELECTIVE_MOVING
                             : ZR_GC_COMPACT_MODE_NON_MOVING;

    for (index = 0u; index < request->regionCount; ++index) {
        const SZrGarbageCollectRegionDescriptor *region =
                &request->regions[index];
        TZrUInt64 reclaimable;

        if (region->kind <= ZR_GARBAGE_COLLECT_REGION_KIND_INVALID ||
            region->kind >= ZR_GARBAGE_COLLECT_REGION_KIND_MAX ||
            region->usedBytes > region->capacityBytes ||
            region->liveBytes > region->usedBytes) {
            gc_compact_diag(diagnostic,
                            ZR_GC_COMPACT_DIAGNOSTIC_INVALID_REGION,
                            (TZrUInt32)index, region->capacityBytes,
                            region->usedBytes);
            return ZR_FALSE;
        }
        if (region->kind == ZR_GARBAGE_COLLECT_REGION_KIND_PINNED ||
            region->kind == ZR_GARBAGE_COLLECT_REGION_KIND_LARGE ||
            region->kind == ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT) {
            ++candidate.pinnedRegionCount;
            candidate.pinnedBytes = gc_compact_sat_add(
                    candidate.pinnedBytes, region->usedBytes);
            continue;
        }
        if (region->kind != ZR_GARBAGE_COLLECT_REGION_KIND_OLD) {
            continue;
        }
        reclaimable = region->usedBytes - region->liveBytes;
        if (!gc_compact_threshold_met(
                    reclaimable, region->usedBytes,
                    request->fragmentationThresholdPercent)) {
            continue;
        }
        ++candidate.candidateRegionCount;
        candidate.candidateBytes = gc_compact_sat_add(
                candidate.candidateBytes, reclaimable);
        candidate.movableBytes = gc_compact_sat_add(
                candidate.movableBytes, region->liveBytes);
        candidate.estimatedWorkUnits = gc_compact_sat_add(
                candidate.estimatedWorkUnits, region->liveBytes);

        if (!request->allowMoving) {
            continue;
        }
        if (request->budgetBytes != 0u &&
            region->liveBytes > request->budgetBytes -
                                (plannedRegionBytes <= request->budgetBytes
                                         ? plannedRegionBytes
                                         : request->budgetBytes)) {
            budgetLimited = ZR_TRUE;
            continue;
        }
        plannedRegionBytes = gc_compact_sat_add(
                plannedRegionBytes, region->liveBytes);
        ++plannedRegionCount;
    }

    if (!request->allowMoving) {
        candidate.deferred = candidate.candidateRegionCount != 0u;
    } else {
        candidate.plannedBytes = plannedRegionBytes;
        candidate.eligible = plannedRegionCount != 0u;
        candidate.deferred = budgetLimited ||
                             plannedRegionCount < candidate.candidateRegionCount;
        if (request->budgetBytes != 0u &&
            candidate.plannedBytes > request->budgetBytes) {
            gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_OVERFLOW,
                            0u, request->budgetBytes,
                            candidate.plannedBytes);
            return ZR_FALSE;
        }
    }
    if (candidate.eligible) {
        candidate.estimatedWorkUnits = candidate.plannedBytes;
    }
    *plan = candidate;
    return ZR_TRUE;
}

TZrBool ZrCore_GarbageCollector_PlanCompaction(
        struct SZrGlobalState *global,
        TZrUInt64 budgetBytes,
        TZrUInt32 fragmentationThresholdPercent,
        TZrBool allowMoving,
        SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic) {
    SZrGcCompactRequest request;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        ZrCore_GcCompact_DiagnosticClear(diagnostic);
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_ARGUMENT,
                        0u, 1u, 0u);
        return ZR_FALSE;
    }
    request.regions = global->garbageCollector->regions;
    request.regionCount = global->garbageCollector->regionCount;
    request.budgetBytes = budgetBytes;
    request.fragmentationThresholdPercent = fragmentationThresholdPercent;
    request.allowMoving = allowMoving;
    return ZrCore_GcCompact_Plan(&request, plan, diagnostic);
}
