#include "zr_vm_core/gc_young_allocation.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static void gc_young_set_diagnostic(
        SZrGcYoungDiagnostic *diagnostic,
        EZrGcYoungDiagnosticCode code,
        TZrUInt32 field,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->field = field;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
}

void ZrCore_GcYoung_DiagnosticClear(SZrGcYoungDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

const TZrChar *ZrCore_GcYoung_DiagnosticName(EZrGcYoungDiagnosticCode code) {
    switch (code) {
        case ZR_GC_YOUNG_DIAGNOSTIC_NONE:
            return "none";
        case ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT:
            return "invalid-argument";
        case ZR_GC_YOUNG_DIAGNOSTIC_INVALID_SCHEMA:
            return "invalid-schema";
        case ZR_GC_YOUNG_DIAGNOSTIC_INVALID_MAGIC:
            return "invalid-magic";
        case ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ALIGNMENT:
            return "invalid-alignment";
        case ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW:
            return "overflow";
        case ZR_GC_YOUNG_DIAGNOSTIC_BOUNDS:
            return "bounds";
        case ZR_GC_YOUNG_DIAGNOSTIC_TLAB_EXHAUSTED:
            return "tlab-exhausted";
        case ZR_GC_YOUNG_DIAGNOSTIC_SAFEPOINT_REQUIRED:
            return "safepoint-required";
        case ZR_GC_YOUNG_DIAGNOSTIC_INVALID_REGION:
            return "invalid-region";
        case ZR_GC_YOUNG_DIAGNOSTIC_INVALID_GENERATION:
            return "invalid-generation";
        case ZR_GC_YOUNG_DIAGNOSTIC_TOSPACE_EXHAUSTED:
            return "to-space-exhausted";
        case ZR_GC_YOUNG_DIAGNOSTIC_MUTATORS_NOT_STOPPED:
            return "mutators-not-stopped";
        case ZR_GC_YOUNG_DIAGNOSTIC_ROOTS_UNAVAILABLE:
            return "roots-unavailable";
        case ZR_GC_YOUNG_DIAGNOSTIC_INVALID_PHASE:
            return "invalid-phase";
        case ZR_GC_YOUNG_DIAGNOSTIC_UNRESOLVED_FORWARDING:
            return "unresolved-forwarding";
        case ZR_GC_YOUNG_DIAGNOSTIC_INCONSISTENT_BOUNDARY:
            return "inconsistent-boundary";
        case ZR_GC_YOUNG_DIAGNOSTIC_END_OF_SCAN:
            return "end-of-scan";
        case ZR_GC_YOUNG_DIAGNOSTIC_ALREADY_COMPLETE:
            return "already-complete";
        case ZR_GC_YOUNG_DIAGNOSTIC_COUNT:
        default:
            return "unknown";
    }
}

static TZrBool gc_young_power_of_two(TZrSize value) {
    return value != 0u && (value & (value - 1u)) == 0u;
}

static TZrBool gc_young_add_size(TZrSize left, TZrSize right, TZrSize *out) {
    if (out == ZR_NULL || right > SIZE_MAX - left) {
        return ZR_FALSE;
    }
    *out = left + right;
    return ZR_TRUE;
}

static TZrBool gc_young_align_size(TZrSize value, TZrSize alignment, TZrSize *out) {
    TZrSize mask;

    if (out == ZR_NULL || !gc_young_power_of_two(alignment)) {
        return ZR_FALSE;
    }
    mask = alignment - 1u;
    if (value > SIZE_MAX - mask) {
        return ZR_FALSE;
    }
    *out = (value + mask) & ~mask;
    return *out != 0u;
}

static TZrBool gc_young_address_range_valid(
        const TZrByte *begin,
        TZrSize capacity) {
    uintptr_t start;

    if (begin == ZR_NULL || capacity == 0u) {
        return ZR_FALSE;
    }
    start = (uintptr_t)(const void *)begin;
    return capacity <= (TZrSize)(UINTPTR_MAX - start);
}

static TZrBool gc_young_tlab_is_retired(const SZrGcTlab *tlab) {
    return tlab != ZR_NULL && tlab->begin == ZR_NULL &&
           tlab->cursor == ZR_NULL && tlab->limit == ZR_NULL &&
           tlab->capacity == 0u && tlab->regionId == 0u;
}

TZrBool ZrCore_GcTlab_Init(
        SZrGcTlab *tlab,
        TZrByte *storage,
        TZrSize capacity,
        TZrUInt32 regionId,
        TZrSize wasteLimit) {
    if (tlab == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(tlab, 0, sizeof(*tlab));
    if (!gc_young_address_range_valid(storage, capacity) || regionId == 0u) {
        return ZR_FALSE;
    }
    if (wasteLimit == 0u) {
        wasteLimit = capacity / 4u;
        if (wasteLimit == 0u) {
            wasteLimit = 1u;
        }
    }
    if (wasteLimit > capacity) {
        return ZR_FALSE;
    }
    tlab->begin = storage;
    tlab->cursor = storage;
    tlab->limit = storage + capacity;
    tlab->regionId = regionId;
    tlab->capacity = capacity;
    tlab->wasteLimit = wasteLimit;
    return ZR_TRUE;
}

TZrBool ZrCore_GcTlab_Validate(
        const SZrGcTlab *tlab,
        SZrGcYoungDiagnostic *diagnostic) {
    uintptr_t begin;
    uintptr_t cursor;
    uintptr_t limit;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (tlab == ZR_NULL) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (gc_young_tlab_is_retired(tlab)) {
        return ZR_TRUE;
    }
    if (tlab->begin == ZR_NULL || tlab->cursor == ZR_NULL ||
        tlab->limit == ZR_NULL || tlab->capacity == 0u ||
        tlab->regionId == 0u || tlab->wasteLimit > tlab->capacity) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_INVALID_REGION,
                                1u, 1u, 0u);
        return ZR_FALSE;
    }

    begin = (uintptr_t)(void *)tlab->begin;
    cursor = (uintptr_t)(void *)tlab->cursor;
    limit = (uintptr_t)(void *)tlab->limit;
    if (limit < begin || cursor < begin || cursor > limit ||
        (TZrSize)(limit - begin) != tlab->capacity) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_BOUNDS,
                                2u, (TZrUInt64)tlab->capacity,
                                (TZrUInt64)(limit >= begin ? limit - begin : 0u));
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrPtr ZrCore_GcTlab_AllocateFast(
        SZrGcTlab *tlab,
        TZrSize size,
        TZrSize alignment,
        SZrGcYoungDiagnostic *diagnostic) {
    uintptr_t cursor;
    uintptr_t limit;
    uintptr_t aligned;
    TZrSize padding;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (tlab == ZR_NULL || size == 0u) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                0u, 1u, 0u);
        return ZR_NULL;
    }
    if (!gc_young_power_of_two(alignment)) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ALIGNMENT,
                                1u, 1u, (TZrUInt64)alignment);
        return ZR_NULL;
    }
    if (!ZrCore_GcTlab_Validate(tlab, diagnostic)) {
        return ZR_NULL;
    }
    if (gc_young_tlab_is_retired(tlab)) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_TLAB_EXHAUSTED,
                                2u, 1u, 0u);
        return ZR_NULL;
    }

    cursor = (uintptr_t)(void *)tlab->cursor;
    limit = (uintptr_t)(void *)tlab->limit;
    padding = (TZrSize)((alignment - (cursor & (uintptr_t)(alignment - 1u))) &
                        (uintptr_t)(alignment - 1u));
    if (padding > (TZrSize)(UINTPTR_MAX - cursor)) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW,
                                2u, UINTPTR_MAX, (TZrUInt64)cursor);
        return ZR_NULL;
    }
    aligned = cursor + (uintptr_t)padding;
    if (aligned > limit || size > (TZrSize)(limit - aligned)) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_TLAB_EXHAUSTED,
                                2u, (TZrUInt64)(limit - cursor),
                                (TZrUInt64)size);
        return ZR_NULL;
    }
    if (tlab->allocationCount == SIZE_MAX) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW,
                                3u, SIZE_MAX - 1u, SIZE_MAX);
        return ZR_NULL;
    }
    memset((void *)aligned, 0, size);
    tlab->cursor = (TZrByte *)(void *)(aligned + (uintptr_t)size);
    tlab->allocationCount++;
    return (TZrPtr)(void *)aligned;
}

TZrBool ZrCore_GcTlab_Retire(
        SZrGcTlab *tlab,
        SZrGcYoungDiagnostic *diagnostic) {
    uintptr_t begin;
    uintptr_t cursor;
    uintptr_t limit;
    TZrSize used;
    TZrSize waste;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (tlab == ZR_NULL) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (gc_young_tlab_is_retired(tlab)) {
        return ZR_TRUE;
    }
    if (!ZrCore_GcTlab_Validate(tlab, diagnostic)) {
        return ZR_FALSE;
    }
    begin = (uintptr_t)(void *)tlab->begin;
    cursor = (uintptr_t)(void *)tlab->cursor;
    limit = (uintptr_t)(void *)tlab->limit;
    used = (TZrSize)(cursor - begin);
    waste = (TZrSize)(limit - cursor);
    if (!gc_young_add_size(tlab->retiredBytes, tlab->capacity, &tlab->retiredBytes) ||
        !gc_young_add_size(tlab->wasteBytes, waste, &tlab->wasteBytes)) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW,
                                4u, SIZE_MAX, tlab->retiredBytes);
        return ZR_FALSE;
    }
    (void)used;
    tlab->begin = ZR_NULL;
    tlab->cursor = ZR_NULL;
    tlab->limit = ZR_NULL;
    tlab->regionId = 0u;
    tlab->capacity = 0u;
    tlab->wasteLimit = 0u;
    return ZR_TRUE;
}

TZrBool ZrCore_GcTlab_Refill(
        SZrGcTlab *tlab,
        TZrByte *storage,
        TZrSize capacity,
        TZrUInt32 regionId,
        TZrSize wasteLimit,
        TZrBool atSafepoint,
        SZrGcYoungDiagnostic *diagnostic) {
    TZrSize nextRefillCount;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (tlab == ZR_NULL || !gc_young_address_range_valid(storage, capacity) ||
        regionId == 0u) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (!atSafepoint) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_SAFEPOINT_REQUIRED,
                                5u, 1u, 0u);
        return ZR_FALSE;
    }
    if (wasteLimit == 0u) {
        wasteLimit = capacity / 4u;
        if (wasteLimit == 0u) {
            wasteLimit = 1u;
        }
    }
    if (wasteLimit > capacity || tlab->refillCount == SIZE_MAX) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW,
                                4u, SIZE_MAX - 1u,
                                (TZrUInt64)tlab->refillCount);
        return ZR_FALSE;
    }
    if (!gc_young_tlab_is_retired(tlab) &&
        !ZrCore_GcTlab_Retire(tlab, diagnostic)) {
        return ZR_FALSE;
    }
    nextRefillCount = tlab->refillCount + 1u;
    tlab->begin = storage;
    tlab->cursor = storage;
    tlab->limit = storage + capacity;
    tlab->regionId = regionId;
    tlab->capacity = capacity;
    tlab->wasteLimit = wasteLimit;
    tlab->refillCount = nextRefillCount;
    return ZR_TRUE;
}

void ZrCore_GcYoungAllocationRequest_Init(
        SZrGcYoungAllocationRequest *request) {
    if (request == ZR_NULL) {
        return;
    }
    memset(request, 0, sizeof(*request));
    request->largeObjectThreshold = ZR_GC_YOUNG_DEFAULT_LARGE_OBJECT_THRESHOLD;
}

static TZrBool gc_young_set_allocation_decision(
        SZrGcYoungAllocationDecision *decision,
        EZrGcYoungAllocationTarget target,
        EZrGarbageCollectRegionKind regionKind,
        EZrGarbageCollectStorageKind storageKind,
        EZrGarbageCollectPromotionReason reason,
        TZrSize alignedBytes,
        TZrBool requiresSafepoint,
        TZrBool usesYoungGeneration) {
    if (decision == ZR_NULL) {
        return ZR_FALSE;
    }
    decision->target = target;
    decision->regionKind = regionKind;
    decision->storageKind = storageKind;
    decision->reason = reason;
    decision->alignedBytes = alignedBytes;
    decision->requiresSafepoint = requiresSafepoint;
    decision->usesYoungGeneration = usesYoungGeneration;
    return ZR_TRUE;
}

TZrBool ZrCore_GcYoung_SelectAllocation(
        const SZrGcYoungAllocationRequest *request,
        SZrGcYoungAllocationDecision *decision,
        SZrGcYoungDiagnostic *diagnostic) {
    TZrSize threshold;
    TZrSize alignedBytes;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (request == ZR_NULL || decision == ZR_NULL || request->objectBytes == 0u) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                0u, 1u, 0u);
        return ZR_FALSE;
    }
    threshold = request->largeObjectThreshold != 0u
                    ? request->largeObjectThreshold
                    : ZR_GC_YOUNG_DEFAULT_LARGE_OBJECT_THRESHOLD;
    if (!gc_young_align_size(request->objectBytes, ZR_ALIGN_SIZE, &alignedBytes)) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_OVERFLOW,
                                0u, SIZE_MAX, (TZrUInt64)request->objectBytes);
        return ZR_FALSE;
    }
    if (request->nativeVisible || request->pinned) {
        return gc_young_set_allocation_decision(
                decision, ZR_GC_YOUNG_ALLOCATION_PINNED,
                ZR_GARBAGE_COLLECT_REGION_KIND_PINNED,
                ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED,
                ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED,
                alignedBytes, ZR_FALSE, ZR_FALSE);
    }
    if (request->objectBytes >= threshold) {
        return gc_young_set_allocation_decision(
                decision, ZR_GC_YOUNG_ALLOCATION_LARGE,
                ZR_GARBAGE_COLLECT_REGION_KIND_LARGE,
                ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT,
                ZR_GARBAGE_COLLECT_PROMOTION_REASON_LARGE_OBJECT,
                alignedBytes, ZR_FALSE, ZR_FALSE);
    }
    if (request->tlabAvailableBytes >= alignedBytes) {
        return gc_young_set_allocation_decision(
                decision, ZR_GC_YOUNG_ALLOCATION_TLAB,
                ZR_GARBAGE_COLLECT_REGION_KIND_EDEN,
                ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
                ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE,
                alignedBytes, ZR_FALSE, ZR_TRUE);
    }
    if (request->regionAvailableBytes >= alignedBytes) {
        return gc_young_set_allocation_decision(
                decision, ZR_GC_YOUNG_ALLOCATION_YOUNG_REGION,
                ZR_GARBAGE_COLLECT_REGION_KIND_EDEN,
                ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE,
                ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE,
                alignedBytes, ZR_TRUE, ZR_TRUE);
    }
    if (!request->atSafepoint) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_SAFEPOINT_REQUIRED,
                                6u, 1u, 0u);
    } else {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_TLAB_EXHAUSTED,
                                1u, (TZrUInt64)alignedBytes,
                                (TZrUInt64)request->tlabAvailableBytes);
    }
    return ZR_FALSE;
}

void ZrCore_GcPromotionRequest_Init(SZrGcPromotionRequest *request) {
    if (request == ZR_NULL) {
        return;
    }
    memset(request, 0, sizeof(*request));
    request->largeObjectThreshold = ZR_GC_YOUNG_DEFAULT_LARGE_OBJECT_THRESHOLD;
    request->survivorAgeThreshold = 1u;
}

static EZrGarbageCollectPromotionReason gc_young_escape_reason(TZrUInt32 flags) {
    if ((flags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_HOST_HANDLE) != 0u ||
        (flags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_HOST_HANDLE;
    }
    if ((flags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_PINNED_REFERENCE) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED;
    }
    if ((flags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_OLD_REFERENCE) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_OLD_REFERENCE;
    }
    if ((flags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_MODULE_ROOT;
    }
    if ((flags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT) != 0u) {
        return ZR_GARBAGE_COLLECT_PROMOTION_REASON_GLOBAL_ROOT;
    }
    return ZR_GARBAGE_COLLECT_PROMOTION_REASON_ESCAPE;
}

TZrBool ZrCore_GcYoung_DecidePromotion(
        const SZrGcPromotionRequest *request,
        SZrGcPromotionDecision *decision,
        SZrGcYoungDiagnostic *diagnostic) {
    TZrSize threshold;
    TZrUInt32 ageThreshold;
    EZrGarbageCollectPromotionReason reason;

    ZrCore_GcYoung_DiagnosticClear(diagnostic);
    if (request == ZR_NULL || decision == ZR_NULL || request->objectBytes == 0u) {
        gc_young_set_diagnostic(diagnostic,
                                ZR_GC_YOUNG_DIAGNOSTIC_INVALID_ARGUMENT,
                                0u, 1u, 0u);
        return ZR_FALSE;
    }
    threshold = request->largeObjectThreshold != 0u
                    ? request->largeObjectThreshold
                    : ZR_GC_YOUNG_DEFAULT_LARGE_OBJECT_THRESHOLD;
    ageThreshold = request->survivorAgeThreshold != 0u
                       ? request->survivorAgeThreshold
                       : 1u;

    if (request->pinFlags != 0u) {
        decision->target = ZR_GC_YOUNG_PROMOTION_PINNED;
        decision->regionKind = ZR_GARBAGE_COLLECT_REGION_KIND_PINNED;
        decision->storageKind = ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED;
        decision->reason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED;
        decision->nextSurvivalAge = request->survivalAge;
        return ZR_TRUE;
    }
    if (request->objectBytes >= threshold) {
        decision->target = ZR_GC_YOUNG_PROMOTION_LARGE;
        decision->regionKind = ZR_GARBAGE_COLLECT_REGION_KIND_LARGE;
        decision->storageKind = ZR_GARBAGE_COLLECT_STORAGE_KIND_LARGE_PERSISTENT;
        decision->reason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_LARGE_OBJECT;
        decision->nextSurvivalAge = request->survivalAge;
        return ZR_TRUE;
    }
    if (request->escapeFlags != 0u) {
        reason = gc_young_escape_reason(request->escapeFlags);
        decision->target = (reason == ZR_GARBAGE_COLLECT_PROMOTION_REASON_PINNED)
                               ? ZR_GC_YOUNG_PROMOTION_PINNED
                               : ZR_GC_YOUNG_PROMOTION_OLD;
        decision->regionKind = (decision->target == ZR_GC_YOUNG_PROMOTION_PINNED)
                                  ? ZR_GARBAGE_COLLECT_REGION_KIND_PINNED
                                  : ZR_GARBAGE_COLLECT_REGION_KIND_OLD;
        decision->storageKind = (decision->target == ZR_GC_YOUNG_PROMOTION_PINNED)
                                   ? ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_PINNED
                                   : ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE;
        decision->reason = reason;
        decision->nextSurvivalAge = request->survivalAge;
        return ZR_TRUE;
    }
    if (request->survivalAge >= ageThreshold) {
        decision->target = ZR_GC_YOUNG_PROMOTION_OLD;
        decision->regionKind = ZR_GARBAGE_COLLECT_REGION_KIND_OLD;
        decision->storageKind = ZR_GARBAGE_COLLECT_STORAGE_KIND_OLD_MOVABLE;
        decision->reason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL;
        decision->nextSurvivalAge = request->survivalAge;
        return ZR_TRUE;
    }
    decision->target = ZR_GC_YOUNG_PROMOTION_SURVIVOR;
    decision->regionKind = ZR_GARBAGE_COLLECT_REGION_KIND_SURVIVOR;
    decision->storageKind = ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE;
    decision->reason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_SURVIVAL;
    decision->nextSurvivalAge = request->survivalAge == UINT32_MAX
                                    ? UINT32_MAX
                                    : request->survivalAge + 1u;
    return ZR_TRUE;
}
