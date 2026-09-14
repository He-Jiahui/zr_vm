#include "zr_vm_core/optimization_remark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZR_OPTIMIZATION_REMARK_DEFAULT_MAX_RECORDS ((TZrUInt32)4096u)

/* Keep allocation arithmetic in the project's size type.  Some supported
 * host toolchains expose SIZE_MAX with a wider integer type than size_t;
 * comparing a TZrSize directly with that macro then triggers -Wtype-limits
 * (and can hide the actual overflow check). */
static TZrSize zr_remark_max_count_for_item_size(TZrSize itemSize) {
    if (itemSize == 0u) return (TZrSize)-1;
    return ((TZrSize)-1) / itemSize;
}

static TZrSize zr_remark_required_with_nul(TZrSize length) {
    return length == (TZrSize)-1 ? length : length + 1u;
}

static TZrBool zr_remark_fail(SZrOptimizationRemarkDiagnostic *diagnostic,
                              EZrOptimizationRemarkDiagnosticCode code,
                              TZrUInt32 field,
                              TZrUInt64 expected,
                              TZrUInt64 actual) {
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->field = field;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return ZR_FALSE;
}

void ZrCore_OptimizationRemarkDiagnostic_Clear(
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    if (diagnostic == ZR_NULL) return;
    diagnostic->code = ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_NONE;
    diagnostic->field = 0u;
    diagnostic->expected = 0u;
    diagnostic->actual = 0u;
}

const TZrChar *ZrCore_OptimizationRemark_DiagnosticName(
        EZrOptimizationRemarkDiagnosticCode code) {
    switch (code) {
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_NONE: return "none";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT: return "invalid_argument";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_SCHEMA: return "schema";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_RANGE: return "invalid_range";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_STATUS: return "invalid_status";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_REASON: return "invalid_reason";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_EVIDENCE: return "invalid_evidence";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_UNKNOWN_BACKEND: return "unknown_backend";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_UNKNOWN_COUNTER: return "unknown_counter";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_COUNTER_UNAVAILABLE: return "counter_unavailable";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY: return "invalid_query";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_OUT_OF_MEMORY: return "out_of_memory";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_BUFFER_TOO_SMALL: return "buffer_too_small";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_NOT_FOUND: return "not_found";
        case ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_COUNT: break;
    }
    return "unknown";
}

const TZrChar *ZrCore_OptimizationRemark_StatusName(
        EZrOptimizationRemarkStatus status) {
    switch (status) {
        case ZR_OPTIMIZATION_REMARK_SUCCESS: return "success";
        case ZR_OPTIMIZATION_REMARK_MISSED: return "missed";
        case ZR_OPTIMIZATION_REMARK_BLOCKED: return "blocked";
        case ZR_OPTIMIZATION_REMARK_STATUS_COUNT: break;
    }
    return "unknown";
}

const TZrChar *ZrCore_OptimizationRemark_ReasonName(
        EZrOptimizationRemarkReason reason) {
    switch (reason) {
        case ZR_OPTIMIZATION_REMARK_REASON_NONE: return "none";
        case ZR_OPTIMIZATION_REMARK_REASON_ALIAS_UNKNOWN: return "alias_unknown";
        case ZR_OPTIMIZATION_REMARK_REASON_ESCAPES: return "escapes";
        case ZR_OPTIMIZATION_REMARK_REASON_ABI_VISIBLE: return "abi_visible";
        case ZR_OPTIMIZATION_REMARK_REASON_EFFECT_ORDER: return "effect_order";
        case ZR_OPTIMIZATION_REMARK_REASON_CODE_BUDGET: return "code_budget";
        case ZR_OPTIMIZATION_REMARK_REASON_PROFILE_STALE: return "profile_stale";
        case ZR_OPTIMIZATION_REMARK_REASON_TARGET_UNSUPPORTED: return "target_unsupported";
        case ZR_OPTIMIZATION_REMARK_REASON_CAPABILITY_DENIED: return "capability_denied";
        case ZR_OPTIMIZATION_REMARK_REASON_NO_PROFILE: return "no_profile";
        case ZR_OPTIMIZATION_REMARK_REASON_BOXING: return "boxing";
        case ZR_OPTIMIZATION_REMARK_REASON_BOUNDS: return "bounds";
        case ZR_OPTIMIZATION_REMARK_REASON_VECTOR: return "vector";
        case ZR_OPTIMIZATION_REMARK_REASON_BARRIER: return "barrier";
        case ZR_OPTIMIZATION_REMARK_REASON_INLINING: return "inlining";
        case ZR_OPTIMIZATION_REMARK_REASON_AOT: return "aot";
        case ZR_OPTIMIZATION_REMARK_REASON_LAYOUT: return "layout";
        case ZR_OPTIMIZATION_REMARK_REASON_ALLOCATION: return "allocation";
        case ZR_OPTIMIZATION_REMARK_REASON_DEOPT: return "deopt";
        case ZR_OPTIMIZATION_REMARK_REASON_SOFTWARE_IC_MISS: return "software_ic_miss";
        case ZR_OPTIMIZATION_REMARK_REASON_HARDWARE_CACHE_MISS: return "hardware_cache_miss";
        case ZR_OPTIMIZATION_REMARK_REASON_BOUNDS_UNKNOWN: return "bounds_unknown";
        case ZR_OPTIMIZATION_REMARK_REASON_CANCELLED: return "cancelled";
        case ZR_OPTIMIZATION_REMARK_REASON_TRUNCATED: return "truncated";
        case ZR_OPTIMIZATION_REMARK_REASON_COUNT: break;
    }
    return "unknown";
}

const TZrChar *ZrCore_OptimizationRemark_EvidenceName(
        EZrOptimizationRemarkEvidence evidence) {
    switch (evidence) {
        case ZR_OPTIMIZATION_REMARK_EVIDENCE_PROVEN: return "proven";
        case ZR_OPTIMIZATION_REMARK_EVIDENCE_ESTIMATED: return "estimated";
        case ZR_OPTIMIZATION_REMARK_EVIDENCE_MEASURED: return "measured";
        case ZR_OPTIMIZATION_REMARK_EVIDENCE_UNAVAILABLE: return "unavailable";
        case ZR_OPTIMIZATION_REMARK_EVIDENCE_COUNT: break;
    }
    return "unknown";
}

void ZrCore_OptimizationRemark_Init(SZrOptimizationRemark *remark) {
    if (remark == ZR_NULL) return;
    memset(remark, 0, sizeof(*remark));
    remark->schemaVersion = ZR_OPTIMIZATION_REMARK_SCHEMA_VERSION;
    remark->status = ZR_OPTIMIZATION_REMARK_SUCCESS;
    remark->reason = ZR_OPTIMIZATION_REMARK_REASON_NONE;
    remark->evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_PROVEN;
}

TZrBool ZrCore_OptimizationRemark_Validate(
        const SZrOptimizationRemark *remark,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    TZrUInt32 unknownBits;

    ZrCore_OptimizationRemarkDiagnostic_Clear(diagnostic);
    if (remark == ZR_NULL) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 1u, 0u);
    }
    if (remark->schemaVersion != ZR_OPTIMIZATION_REMARK_SCHEMA_VERSION) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_SCHEMA,
                              1u,
                              ZR_OPTIMIZATION_REMARK_SCHEMA_VERSION,
                              remark->schemaVersion);
    }
    if (remark->pass[0] == '\0' ||
        memchr(remark->pass, '\0', sizeof(remark->pass)) == ZR_NULL ||
        (remark->module[0] != '\0' &&
         memchr(remark->module, '\0', sizeof(remark->module)) == ZR_NULL)) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                              2u, 0u, 1u);
    }
    if (remark->sourceRange.endOffset < remark->sourceRange.startOffset) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_RANGE,
                              3u,
                              remark->sourceRange.startOffset,
                              remark->sourceRange.endOffset);
    }
    if ((TZrUInt32)remark->status >=
        (TZrUInt32)ZR_OPTIMIZATION_REMARK_STATUS_COUNT) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_STATUS,
                              4u,
                              ZR_OPTIMIZATION_REMARK_STATUS_COUNT - 1u,
                              (TZrUInt32)remark->status);
    }
    if ((TZrUInt32)remark->reason >=
        (TZrUInt32)ZR_OPTIMIZATION_REMARK_REASON_COUNT) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_REASON,
                              5u,
                              ZR_OPTIMIZATION_REMARK_REASON_COUNT - 1u,
                              (TZrUInt32)remark->reason);
    }
    if ((TZrUInt32)remark->evidence >=
        (TZrUInt32)ZR_OPTIMIZATION_REMARK_EVIDENCE_COUNT) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_EVIDENCE,
                              6u,
                              ZR_OPTIMIZATION_REMARK_EVIDENCE_COUNT - 1u,
                              (TZrUInt32)remark->evidence);
    }
    unknownBits = remark->backendMask & ~ZR_OPTIMIZATION_REMARK_BACKEND_KNOWN_MASK;
    if (unknownBits != 0u) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_UNKNOWN_BACKEND,
                              7u, 0u, unknownBits);
    }
    if (remark->backendMask == 0u) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                              7u, 1u, 0u);
    }
    unknownBits = remark->measuredCounterMask &
                  ~ZR_OPTIMIZATION_REMARK_COUNTER_KNOWN_MASK;
    if (unknownBits != 0u) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_UNKNOWN_COUNTER,
                              8u, 0u, unknownBits);
    }
    if (remark->evidence == ZR_OPTIMIZATION_REMARK_EVIDENCE_MEASURED &&
        remark->measuredCounterMask == 0u) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_COUNTER_UNAVAILABLE,
                              8u, 1u, 0u);
    }
    if (remark->evidence != ZR_OPTIMIZATION_REMARK_EVIDENCE_MEASURED &&
        remark->measuredCounterMask != 0u) {
        /* A producer must not smuggle live measurements into a proven,
         * estimated, or unavailable row.  Consumers use the evidence kind to
         * decide whether a counter is safe to display. */
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_COUNTER_UNAVAILABLE,
                              8u, 0u, remark->measuredCounterMask);
    }
    if ((remark->status == ZR_OPTIMIZATION_REMARK_MISSED ||
         remark->status == ZR_OPTIMIZATION_REMARK_BLOCKED) &&
        remark->reason == ZR_OPTIMIZATION_REMARK_REASON_NONE) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_REASON,
                              5u, 1u, 0u);
    }
    return ZR_TRUE;
}

void ZrCore_OptimizationRemarks_StoreInit(
        SZrOptimizationRemarkStore *store) {
    if (store == ZR_NULL) return;
    memset(store, 0, sizeof(*store));
    store->maxRecords = ZR_OPTIMIZATION_REMARK_DEFAULT_MAX_RECORDS;
}

void ZrCore_OptimizationRemarks_StoreFree(
        SZrOptimizationRemarkStore *store) {
    if (store == ZR_NULL) return;
    free(store->items);
    memset(store, 0, sizeof(*store));
}

static TZrBool zr_remark_store_reserve(SZrOptimizationRemarkStore *store,
                                        TZrUInt32 required,
                                        SZrOptimizationRemarkDiagnostic *diagnostic) {
    TZrUInt32 capacity;
    SZrOptimizationRemark *replacement;

    if (required <= store->capacity) return ZR_TRUE;
    capacity = store->capacity != 0u ? store->capacity : 16u;
    while (capacity < required) {
        if (capacity > UINT32_MAX / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    if ((TZrSize)capacity >
        zr_remark_max_count_for_item_size((TZrSize)sizeof(*replacement))) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_OUT_OF_MEMORY,
                              9u,
                              zr_remark_max_count_for_item_size(
                                      (TZrSize)sizeof(*replacement)),
                              capacity);
    }
    replacement = (SZrOptimizationRemark *)realloc(
            store->items, (TZrSize)capacity * sizeof(*replacement));
    if (replacement == ZR_NULL) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_OUT_OF_MEMORY,
                              9u, 0u, required);
    }
    store->items = replacement;
    store->capacity = capacity;
    return ZR_TRUE;
}

TZrBool ZrCore_OptimizationRemarks_Append(
        SZrOptimizationRemarkStore *store,
        const SZrOptimizationRemark *remark,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    TZrUInt32 limit;

    ZrCore_OptimizationRemarkDiagnostic_Clear(diagnostic);
    if (store == ZR_NULL || remark == ZR_NULL ||
        (store != ZR_NULL &&
         ((store->count != 0u && store->items == ZR_NULL) ||
          store->count > store->capacity))) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 1u, 0u);
    }
    if (!ZrCore_OptimizationRemark_Validate(remark, diagnostic)) return ZR_FALSE;
    limit = store->maxRecords != 0u
            ? store->maxRecords
            : ZR_OPTIMIZATION_REMARK_DEFAULT_MAX_RECORDS;
    if (store->count >= limit) {
        store->truncated = ZR_TRUE;
        if (store->droppedCount != UINT64_MAX) store->droppedCount++;
        return ZR_TRUE;
    }
    if (store->count == UINT32_MAX ||
        !zr_remark_store_reserve(store, store->count + 1u, diagnostic)) {
        return ZR_FALSE;
    }
    store->items[store->count++] = *remark;
    return ZR_TRUE;
}

void ZrCore_OptimizationRemarkQuery_Init(
        SZrOptimizationRemarkQuery *query) {
    if (query == ZR_NULL) return;
    memset(query, 0, sizeof(*query));
}

void ZrCore_OptimizationRemarks_PageFree(
        SZrOptimizationRemarkPage *page) {
    if (page == ZR_NULL) return;
    free(page->items);
    memset(page, 0, sizeof(*page));
}

static TZrBool zr_remark_matches(const SZrOptimizationRemark *remark,
                                 const SZrOptimizationRemarkQuery *query) {
    if ((query->hasModuleHash || query->moduleHash != 0u) &&
        remark->moduleHash != query->moduleHash) return ZR_FALSE;
    if ((query->hasIrHash || query->irHash != 0u) &&
        remark->irHash != query->irHash) return ZR_FALSE;
    if ((query->hasSourceVersion || query->sourceVersion != 0u) &&
        remark->sourceVersion != query->sourceVersion) return ZR_FALSE;
    if ((query->hasSourceId || query->sourceId != 0u) &&
        remark->sourceId != query->sourceId) return ZR_FALSE;
    if (query->reasonMask != 0u &&
        (query->reasonMask & ZR_OPTIMIZATION_REMARK_REASON_MASK(remark->reason)) == 0u) {
        return ZR_FALSE;
    }
    if (query->statusMask != 0u &&
        (query->statusMask & ZR_OPTIMIZATION_REMARK_STATUS_MASK(remark->status)) == 0u) {
        return ZR_FALSE;
    }
    if (query->backendMask != 0u &&
        (query->backendMask & remark->backendMask) == 0u) return ZR_FALSE;
    if (query->evidenceMask != 0u &&
        (query->evidenceMask & ZR_OPTIMIZATION_REMARK_EVIDENCE_MASK(remark->evidence)) == 0u) {
        return ZR_FALSE;
    }
    if (query->hasSourceRange &&
        (remark->sourceRange.startOffset < query->sourceRange.startOffset ||
         remark->sourceRange.endOffset > query->sourceRange.endOffset)) return ZR_FALSE;
    if (query->hasPass && strncmp(remark->pass, query->pass,
                                  ZR_OPTIMIZATION_REMARK_PASS_NAME_MAX) != 0) {
        return ZR_FALSE;
    }
    if (query->hasModule && strncmp(remark->module, query->module,
                                    ZR_OPTIMIZATION_REMARK_MODULE_NAME_MAX) != 0) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static int zr_remark_compare(const void *left, const void *right) {
    const SZrOptimizationRemark *a = (const SZrOptimizationRemark *)left;
    const SZrOptimizationRemark *b = (const SZrOptimizationRemark *)right;
    int comparison;

#define ZR_REMARK_COMPARE_U64(field) \
    do { \
        if (a->field < b->field) return -1; \
        if (a->field > b->field) return 1; \
    } while (0)
#define ZR_REMARK_COMPARE_U32(field) \
    do { \
        if (a->field < b->field) return -1; \
        if (a->field > b->field) return 1; \
    } while (0)

    if (a->sourceRange.startOffset < b->sourceRange.startOffset) return -1;
    if (a->sourceRange.startOffset > b->sourceRange.startOffset) return 1;
    if (a->sourceRange.endOffset < b->sourceRange.endOffset) return -1;
    if (a->sourceRange.endOffset > b->sourceRange.endOffset) return 1;
    if (a->sourceId < b->sourceId) return -1;
    if (a->sourceId > b->sourceId) return 1;
    comparison = strcmp(a->pass, b->pass);
    if (comparison != 0) return comparison;
    if ((TZrUInt32)a->status < (TZrUInt32)b->status) return -1;
    if ((TZrUInt32)a->status > (TZrUInt32)b->status) return 1;
    if ((TZrUInt32)a->reason < (TZrUInt32)b->reason) return -1;
    if ((TZrUInt32)a->reason > (TZrUInt32)b->reason) return 1;
    if (a->proofId < b->proofId) return -1;
    if (a->proofId > b->proofId) return 1;

    /* The public sort keys above are the human-facing order.  Complete the
     * ordering with identity/fact fields so rows that share a source/pass
     * key still have deterministic output when a query spans modules or
     * evidence kinds.  Exact duplicate records are observationally equal. */
    comparison = strcmp(a->module, b->module);
    if (comparison != 0) return comparison;
    ZR_REMARK_COMPARE_U64(siteKey);
    ZR_REMARK_COMPARE_U64(moduleHash);
    ZR_REMARK_COMPARE_U64(irHash);
    ZR_REMARK_COMPARE_U64(moduleVersion);
    ZR_REMARK_COMPARE_U64(irVersion);
    ZR_REMARK_COMPARE_U64(sourceVersion);
    ZR_REMARK_COMPARE_U32(backendMask);
    ZR_REMARK_COMPARE_U32(evidence);
    ZR_REMARK_COMPARE_U64(profileCount);
    ZR_REMARK_COMPARE_U64(estimatedCost);
    ZR_REMARK_COMPARE_U32(before);
    ZR_REMARK_COMPARE_U32(after);
    ZR_REMARK_COMPARE_U32(measuredCounterMask);
    ZR_REMARK_COMPARE_U32(boxingFlags);
    ZR_REMARK_COMPARE_U32(allocationFlags);
    ZR_REMARK_COMPARE_U32(cacheFlags);
    ZR_REMARK_COMPARE_U32(deoptFlags);
    ZR_REMARK_COMPARE_U64(softwareIcMisses);
    ZR_REMARK_COMPARE_U64(hardwareCacheMisses);
    ZR_REMARK_COMPARE_U64(branchMisses);
    ZR_REMARK_COMPARE_U64(allocations);
    ZR_REMARK_COMPARE_U64(deopts);
#undef ZR_REMARK_COMPARE_U64
#undef ZR_REMARK_COMPARE_U32
    return 0;
}

TZrBool ZrCore_OptimizationRemarks_Query(
        const SZrOptimizationRemarkStore *store,
        const SZrOptimizationRemarkQuery *query,
        SZrOptimizationRemarkPage *page,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    TZrUInt32 matchCount = 0u;
    TZrUInt32 index;
    TZrUInt32 begin;
    TZrUInt32 limit;

    ZrCore_OptimizationRemarkDiagnostic_Clear(diagnostic);
    if (store == ZR_NULL || query == ZR_NULL || page == ZR_NULL ||
        (store->count != 0u && store->items == ZR_NULL) ||
        store->count > store->capacity) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 1u, 0u);
    }
    if (query->reasonMask &
        ~((TZrUInt32)((((TZrUInt64)1u <<
                        (TZrUInt32)ZR_OPTIMIZATION_REMARK_REASON_COUNT)) - 1u))) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY,
                              1u, 0u, query->reasonMask);
    }
    if (query->statusMask &
        ~((TZrUInt32)((((TZrUInt64)1u <<
                        (TZrUInt32)ZR_OPTIMIZATION_REMARK_STATUS_COUNT)) - 1u))) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY,
                              2u, 0u, query->statusMask);
    }
    if (query->backendMask & ~ZR_OPTIMIZATION_REMARK_BACKEND_KNOWN_MASK) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY,
                              3u, 0u, query->backendMask);
    }
    if (query->evidenceMask &
        ~((TZrUInt32)((((TZrUInt64)1u <<
                        (TZrUInt32)ZR_OPTIMIZATION_REMARK_EVIDENCE_COUNT)) - 1u))) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY,
                              4u, 0u, query->evidenceMask);
    }
    if (query->hasSourceRange &&
        query->sourceRange.endOffset < query->sourceRange.startOffset) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY,
                              5u, query->sourceRange.startOffset,
                              query->sourceRange.endOffset);
    }
    if (query->hasPass &&
        memchr(query->pass, '\0', sizeof(query->pass)) == ZR_NULL) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY,
                              6u, 1u, 0u);
    }
    if (query->hasModule &&
        memchr(query->module, '\0', sizeof(query->module)) == ZR_NULL) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY,
                              7u, 1u, 0u);
    }
    /* Stores are public POD containers; callers can mutate them directly.
     * Revalidate each row before using enum values in the matcher (in
     * particular before shifting a reason into a mask) so malformed data
     * cannot trigger undefined behaviour or leak an "unknown" row. */
    for (index = 0u; index < store->count; index++) {
        if (!ZrCore_OptimizationRemark_Validate(&store->items[index], diagnostic)) {
            return ZR_FALSE;
        }
    }

    /* The page may be a first-use automatic variable (as in the public test),
     * so do not attempt to free an uninitialised pointer here.  Callers that
     * reuse a page should call PageFree before issuing another query. */
    memset(page, 0, sizeof(*page));
    for (index = 0u; index < store->count; index++) {
        if (zr_remark_matches(&store->items[index], query)) {
            if (matchCount != UINT32_MAX) matchCount++;
        }
    }
    page->totalMatches = matchCount;
    page->pageOffset = query->pageOffset;
    page->pageLimit = query->pageLimit;
    page->droppedCount = store->droppedCount;
    page->truncated = store->truncated;
    begin = query->pageOffset < matchCount ? query->pageOffset : matchCount;
    limit = query->pageLimit != 0u ? query->pageLimit : matchCount - begin;
    if (limit > matchCount - begin) limit = matchCount - begin;
    if (limit == 0u) return ZR_TRUE;

    if ((TZrSize)limit >
                zr_remark_max_count_for_item_size((TZrSize)sizeof(*page->items)) ||
        (TZrSize)matchCount >
                zr_remark_max_count_for_item_size((TZrSize)sizeof(*page->items))) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_OUT_OF_MEMORY,
                              4u,
                              zr_remark_max_count_for_item_size(
                                      (TZrSize)sizeof(*page->items)),
                              limit > matchCount ? limit : matchCount);
    }

    page->items = (SZrOptimizationRemark *)malloc(
            (TZrSize)limit * sizeof(*page->items));
    if (page->items == ZR_NULL) {
        page->totalMatches = 0u;
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_OUT_OF_MEMORY,
                              4u, limit, 0u);
    }

    /* Build a sorted matching list before applying pagination. */
    {
        SZrOptimizationRemark *matches = (SZrOptimizationRemark *)malloc(
                (TZrSize)matchCount * sizeof(*matches));
        TZrUInt32 matchIndex = 0u;
        if (matches == ZR_NULL) {
            ZrCore_OptimizationRemarks_PageFree(page);
            page->totalMatches = 0u;
            return zr_remark_fail(diagnostic,
                                  ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_OUT_OF_MEMORY,
                                  4u, matchCount, 0u);
        }
        for (index = 0u; index < store->count; index++) {
            if (zr_remark_matches(&store->items[index], query)) {
                matches[matchIndex++] = store->items[index];
            }
        }
        qsort(matches, matchCount, sizeof(*matches), zr_remark_compare);
        memcpy(page->items, matches + begin,
               (TZrSize)limit * sizeof(*page->items));
        page->count = limit;
        free(matches);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_OptimizationRemarks_InvalidateSourceVersion(
        SZrOptimizationRemarkStore *store,
        TZrUInt64 moduleHash,
        TZrUInt64 sourceVersion,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    TZrUInt32 readIndex;
    TZrUInt32 writeIndex = 0u;
    TZrBool removed = ZR_FALSE;

    ZrCore_OptimizationRemarkDiagnostic_Clear(diagnostic);
    if (store == ZR_NULL ||
        (store->count != 0u && store->items == ZR_NULL) ||
        store->count > store->capacity) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                              0u, 1u, 0u);
    }
    for (readIndex = 0u; readIndex < store->count; readIndex++) {
        if (!ZrCore_OptimizationRemark_Validate(
                    &store->items[readIndex], diagnostic)) {
            return ZR_FALSE;
        }
    }
    for (readIndex = 0u; readIndex < store->count; readIndex++) {
        const SZrOptimizationRemark *remark = &store->items[readIndex];
        if (remark->moduleHash == moduleHash &&
            remark->sourceVersion == sourceVersion) {
            removed = ZR_TRUE;
            continue;
        }
        if (writeIndex != readIndex) store->items[writeIndex] = *remark;
        writeIndex++;
    }
    store->count = writeIndex;
    if (!removed) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_NOT_FOUND,
                              5u, sourceVersion, sourceVersion);
    }
    return ZR_TRUE;
}

typedef struct SZrRemarkWriter {
    TZrChar *buffer;
    TZrSize capacity;
    TZrSize length;
    TZrBool failed;
} SZrRemarkWriter;

static void zr_writer_append_bytes(SZrRemarkWriter *writer,
                                   const TZrChar *text,
                                   TZrSize length) {
    if (writer->length <= (TZrSize)-1 - length) {
        if (writer->buffer != ZR_NULL &&
            writer->length + length < writer->capacity) {
            memcpy(writer->buffer + writer->length, text, length);
        } else if (writer->buffer != ZR_NULL && writer->length + length >= writer->capacity) {
            writer->failed = ZR_TRUE;
        }
        writer->length += length;
    } else {
        writer->failed = ZR_TRUE;
    }
}

static void zr_writer_append(SZrRemarkWriter *writer, const TZrChar *text) {
    if (text != ZR_NULL) zr_writer_append_bytes(writer, text, strlen(text));
}

static void zr_writer_append_uint(SZrRemarkWriter *writer, TZrUInt64 value) {
    TZrChar number[32];
    int length = snprintf(number, sizeof(number), "%llu",
                          (unsigned long long)value);
    if (length > 0) zr_writer_append_bytes(writer, number, (TZrSize)length);
}

static void zr_writer_append_json_string(SZrRemarkWriter *writer,
                                         const TZrChar *text) {
    const unsigned char *cursor = (const unsigned char *)(text != ZR_NULL ? text : "");
    zr_writer_append(writer, "\"");
    while (*cursor != 0u) {
        switch (*cursor) {
            case '\\': zr_writer_append(writer, "\\\\"); break;
            case '"': zr_writer_append(writer, "\\\""); break;
            case '\n': zr_writer_append(writer, "\\n"); break;
            case '\r': zr_writer_append(writer, "\\r"); break;
            case '\t': zr_writer_append(writer, "\\t"); break;
            default:
                if (*cursor < 0x20u) {
                    TZrChar escaped[7];
                    int length = snprintf(escaped, sizeof(escaped), "\\u%04x", *cursor);
                    if (length > 0) zr_writer_append_bytes(writer, escaped, (TZrSize)length);
                } else {
                    zr_writer_append_bytes(writer, (const TZrChar *)cursor, 1u);
                }
                break;
        }
        cursor++;
    }
    zr_writer_append(writer, "\"");
}

static void zr_writer_append_bool(SZrRemarkWriter *writer, TZrBool value) {
    zr_writer_append(writer, value ? "true" : "false");
}

/* A counter is meaningful only when its producer explicitly declared the
 * corresponding source.  Serializing absent counters as zero would turn an
 * unavailable PMU/profile value into an apparently measured zero. */
static void zr_writer_append_optional_counter(SZrRemarkWriter *writer,
                                              TZrUInt32 counterMask,
                                              TZrUInt32 counterBit,
                                              TZrUInt64 value) {
    if ((counterMask & counterBit) != 0u) {
        zr_writer_append_uint(writer, value);
    } else {
        zr_writer_append(writer, "null");
    }
}

static void zr_remark_write_json_body(SZrRemarkWriter *writer,
                                      const SZrOptimizationRemark *remark) {
    zr_writer_append(writer, "{\"schemaVersion\":");
    zr_writer_append_uint(writer, remark->schemaVersion);
    zr_writer_append(writer, ",\"siteKey\":"); zr_writer_append_uint(writer, remark->siteKey);
    zr_writer_append(writer, ",\"moduleHash\":"); zr_writer_append_uint(writer, remark->moduleHash);
    zr_writer_append(writer, ",\"irHash\":"); zr_writer_append_uint(writer, remark->irHash);
    zr_writer_append(writer, ",\"moduleVersion\":"); zr_writer_append_uint(writer, remark->moduleVersion);
    zr_writer_append(writer, ",\"irVersion\":"); zr_writer_append_uint(writer, remark->irVersion);
    zr_writer_append(writer, ",\"sourceVersion\":"); zr_writer_append_uint(writer, remark->sourceVersion);
    zr_writer_append(writer, ",\"sourceId\":"); zr_writer_append_uint(writer, remark->sourceId);
    zr_writer_append(writer, ",\"sourceRange\":{\"startOffset\":");
    zr_writer_append_uint(writer, remark->sourceRange.startOffset);
    zr_writer_append(writer, ",\"endOffset\":"); zr_writer_append_uint(writer, remark->sourceRange.endOffset);
    zr_writer_append(writer, "},\"pass\":"); zr_writer_append_json_string(writer, remark->pass);
    zr_writer_append(writer, ",\"module\":"); zr_writer_append_json_string(writer, remark->module);
    zr_writer_append(writer, ",\"status\":"); zr_writer_append_json_string(writer, ZrCore_OptimizationRemark_StatusName(remark->status));
    zr_writer_append(writer, ",\"reason\":"); zr_writer_append_json_string(writer, ZrCore_OptimizationRemark_ReasonName(remark->reason));
    zr_writer_append(writer, ",\"backendMask\":"); zr_writer_append_uint(writer, remark->backendMask);
    zr_writer_append(writer, ",\"evidenceKind\":"); zr_writer_append_json_string(writer, ZrCore_OptimizationRemark_EvidenceName(remark->evidence));
    zr_writer_append(writer, ",\"proofId\":"); zr_writer_append_uint(writer, remark->proofId);
    zr_writer_append(writer, ",\"profileCount\":"); zr_writer_append_uint(writer, remark->profileCount);
    zr_writer_append(writer, ",\"estimatedCost\":"); zr_writer_append_uint(writer, remark->estimatedCost);
    zr_writer_append(writer, ",\"before\":"); zr_writer_append_uint(writer, remark->before);
    zr_writer_append(writer, ",\"after\":"); zr_writer_append_uint(writer, remark->after);
    zr_writer_append(writer, ",\"measuredCounterMask\":"); zr_writer_append_uint(writer, remark->measuredCounterMask);
    zr_writer_append(writer, ",\"measuredCounters\":{\"softwareIcMisses\":");
    zr_writer_append_optional_counter(writer, remark->measuredCounterMask,
                                      ZR_OPTIMIZATION_REMARK_COUNTER_SOFTWARE_IC,
                                      remark->measuredCounters.softwareIcMisses);
    zr_writer_append(writer, ",\"hardwareCacheMisses\":");
    zr_writer_append_optional_counter(writer, remark->measuredCounterMask,
                                      ZR_OPTIMIZATION_REMARK_COUNTER_HARDWARE_CACHE,
                                      remark->measuredCounters.hardwareCacheMisses);
    zr_writer_append(writer, ",\"branchMisses\":");
    zr_writer_append_optional_counter(writer, remark->measuredCounterMask,
                                      ZR_OPTIMIZATION_REMARK_COUNTER_BRANCH,
                                      remark->measuredCounters.branchMisses);
    zr_writer_append(writer, ",\"allocations\":");
    zr_writer_append_optional_counter(writer, remark->measuredCounterMask,
                                      ZR_OPTIMIZATION_REMARK_COUNTER_ALLOCATIONS,
                                      remark->measuredCounters.allocations);
    zr_writer_append(writer, ",\"deopts\":");
    zr_writer_append_optional_counter(writer, remark->measuredCounterMask,
                                      ZR_OPTIMIZATION_REMARK_COUNTER_DEOPT,
                                      remark->measuredCounters.deopts);
    zr_writer_append(writer, "},\"boxingFlags\":"); zr_writer_append_uint(writer, remark->boxingFlags);
    zr_writer_append(writer, ",\"allocationFlags\":"); zr_writer_append_uint(writer, remark->allocationFlags);
    zr_writer_append(writer, ",\"cacheFlags\":"); zr_writer_append_uint(writer, remark->cacheFlags);
    zr_writer_append(writer, ",\"deoptFlags\":"); zr_writer_append_uint(writer, remark->deoptFlags);
    zr_writer_append(writer, "}");
}

static TZrBool zr_writer_finish(SZrRemarkWriter *writer,
                                TZrChar *buffer,
                                TZrSize capacity,
                                TZrSize *outWrittenSize,
                                SZrOptimizationRemarkDiagnostic *diagnostic) {
    if (outWrittenSize != ZR_NULL) *outWrittenSize = writer->length;
    if (buffer != ZR_NULL && capacity > writer->length) {
        buffer[writer->length] = '\0';
    }
    if (buffer == ZR_NULL && capacity == 0u) {
        if (!writer->failed) return ZR_TRUE;
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_BUFFER_TOO_SMALL,
                              10u,
                              zr_remark_required_with_nul(writer->length),
                              capacity);
    }
    if (buffer == ZR_NULL || capacity <= writer->length || writer->failed) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_BUFFER_TOO_SMALL,
                              10u, zr_remark_required_with_nul(writer->length),
                              capacity);
    }
    return ZR_TRUE;
}

TZrBool ZrCore_OptimizationRemark_WriteJson(
        const SZrOptimizationRemark *remark,
        TZrChar *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    SZrRemarkWriter writer;

    if (outWrittenSize != ZR_NULL) *outWrittenSize = 0u;
    ZrCore_OptimizationRemarkDiagnostic_Clear(diagnostic);
    if (!ZrCore_OptimizationRemark_Validate(remark, diagnostic)) return ZR_FALSE;
    writer.buffer = buffer;
    writer.capacity = bufferCapacity;
    writer.length = 0u;
    writer.failed = ZR_FALSE;
    zr_remark_write_json_body(&writer, remark);
    return zr_writer_finish(&writer, buffer, bufferCapacity, outWrittenSize, diagnostic);
}

TZrBool ZrCore_OptimizationRemarks_PageWriteJson(
        const SZrOptimizationRemarkPage *page,
        TZrChar *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    SZrRemarkWriter writer;
    TZrUInt32 index;

    if (outWrittenSize != ZR_NULL) *outWrittenSize = 0u;
    ZrCore_OptimizationRemarkDiagnostic_Clear(diagnostic);
    if (page == ZR_NULL || (page->count != 0u && page->items == ZR_NULL) ||
        page->count > page->totalMatches) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT,
                              11u, 0u, 1u);
    }
    for (index = 0u; index < page->count; index++) {
        if (!ZrCore_OptimizationRemark_Validate(&page->items[index], diagnostic)) {
            return ZR_FALSE;
        }
    }
    writer.buffer = buffer;
    writer.capacity = bufferCapacity;
    writer.length = 0u;
    writer.failed = ZR_FALSE;
    zr_writer_append(&writer, "{\"schemaVersion\":");
    zr_writer_append_uint(&writer, ZR_OPTIMIZATION_REMARK_SCHEMA_VERSION);
    zr_writer_append(&writer, ",\"pageOffset\":"); zr_writer_append_uint(&writer, page->pageOffset);
    zr_writer_append(&writer, ",\"pageLimit\":"); zr_writer_append_uint(&writer, page->pageLimit);
    zr_writer_append(&writer, ",\"totalMatches\":"); zr_writer_append_uint(&writer, page->totalMatches);
    zr_writer_append(&writer, ",\"count\":"); zr_writer_append_uint(&writer, page->count);
    zr_writer_append(&writer, ",\"droppedCount\":"); zr_writer_append_uint(&writer, page->droppedCount);
    zr_writer_append(&writer, ",\"truncated\":"); zr_writer_append_bool(&writer, page->truncated);
    zr_writer_append(&writer, ",\"items\":[");
    for (index = 0u; index < page->count; index++) {
        if (index != 0u) zr_writer_append(&writer, ",");
        zr_remark_write_json_body(&writer, &page->items[index]);
    }
    zr_writer_append(&writer, "]}");
    return zr_writer_finish(&writer, buffer, bufferCapacity, outWrittenSize, diagnostic);
}

TZrBool ZrCore_OptimizationRemark_WriteText(
        const SZrOptimizationRemark *remark,
        TZrChar *buffer,
        TZrSize bufferCapacity,
        TZrSize *outWrittenSize,
        SZrOptimizationRemarkDiagnostic *diagnostic) {
    SZrRemarkWriter writer;
    char line[512];
    int length;

    if (outWrittenSize != ZR_NULL) *outWrittenSize = 0u;
    ZrCore_OptimizationRemarkDiagnostic_Clear(diagnostic);
    if (!ZrCore_OptimizationRemark_Validate(remark, diagnostic)) return ZR_FALSE;
    length = snprintf(line, sizeof(line),
                      "%s: %s (%s) at source %u [%u..%u] module=%s "
                      "version=%llu site=%llu evidence=%s",
                      remark->pass,
                      ZrCore_OptimizationRemark_StatusName(remark->status),
                      ZrCore_OptimizationRemark_ReasonName(remark->reason),
                      (unsigned)remark->sourceId,
                      (unsigned)remark->sourceRange.startOffset,
                      (unsigned)remark->sourceRange.endOffset,
                      remark->module,
                      (unsigned long long)remark->sourceVersion,
                      (unsigned long long)remark->siteKey,
                      ZrCore_OptimizationRemark_EvidenceName(remark->evidence));
    if (length < 0 || (TZrSize)length >= sizeof(line)) {
        return zr_remark_fail(diagnostic,
                              ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_BUFFER_TOO_SMALL,
                              10u, sizeof(line) - 1u, (TZrUInt64)length);
    }
    writer.buffer = buffer;
    writer.capacity = bufferCapacity;
    writer.length = 0u;
    writer.failed = ZR_FALSE;
    zr_writer_append_bytes(&writer, line, (TZrSize)length);
    return zr_writer_finish(&writer, buffer, bufferCapacity, outWrittenSize, diagnostic);
}
