#include "zr_vm_core/optimization_remark.h"
#include "zr_vm_parser/diagnostic_registry.h"
#include "zr_vm_parser/optimization_remarks.h"

#include <assert.h>
#include <string.h>

static SZrOptimizationRemark make_remark(TZrUInt64 moduleHash,
                                         TZrUInt64 sourceVersion,
                                         TZrUInt32 sourceId,
                                         TZrUInt32 startOffset,
                                         EZrOptimizationRemarkStatus status,
                                         EZrOptimizationRemarkReason reason,
                                         TZrUInt32 backendMask,
                                         const TZrChar *pass,
                                         TZrUInt64 proofId) {
    SZrOptimizationRemark remark;
    ZrCore_OptimizationRemark_Init(&remark);
    remark.moduleHash = moduleHash;
    remark.siteKey = UINT64_C(0x5000) + sourceId;
    remark.irHash = UINT64_C(0x2222);
    remark.sourceVersion = sourceVersion;
    remark.sourceId = sourceId;
    remark.sourceRange.startOffset = startOffset;
    remark.sourceRange.endOffset = startOffset + 4u;
    remark.status = status;
    remark.reason = reason;
    remark.backendMask = backendMask;
    remark.evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_PROVEN;
    remark.proofId = proofId;
    remark.profileCount = 10u;
    remark.estimatedCost = 3u;
    (void)strncpy(remark.pass, pass, sizeof(remark.pass) - 1u);
    return remark;
}

static void test_proven_records_query_by_source_version_and_reason(void) {
    SZrOptimizationRemarkStore store;
    SZrOptimizationRemark query;
    SZrOptimizationRemarkPage page = {0};
    SZrOptimizationRemarkQuery filter;

    ZrCore_OptimizationRemarks_StoreInit(&store);
    query = make_remark(11u, 7u, 101u, 40u,
                        ZR_OPTIMIZATION_REMARK_SUCCESS,
                        ZR_OPTIMIZATION_REMARK_REASON_NONE,
                        ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC,
                        "sroa", 9u);
    assert(ZrCore_OptimizationRemarks_Append(&store, &query, NULL));
    query = make_remark(11u, 7u, 102u, 12u,
                        ZR_OPTIMIZATION_REMARK_MISSED,
                        ZR_OPTIMIZATION_REMARK_REASON_ALIAS_UNKNOWN,
                        ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC,
                        "gvn", 10u);
    assert(ZrCore_OptimizationRemarks_Append(&store, &query, NULL));

    ZrCore_OptimizationRemarkQuery_Init(&filter);
    filter.moduleHash = 11u;
    filter.sourceVersion = 7u;
    filter.reasonMask = ZR_OPTIMIZATION_REMARK_REASON_MASK(
            ZR_OPTIMIZATION_REMARK_REASON_ALIAS_UNKNOWN);
    filter.pageLimit = 8u;
    assert(ZrCore_OptimizationRemarks_Query(&store, &filter, &page, NULL));
    assert(page.count == 1u);
    assert(page.items[0].sourceVersion == 7u);
    assert(page.items[0].sourceId == 102u);
    assert(page.items[0].reason == ZR_OPTIMIZATION_REMARK_REASON_ALIAS_UNKNOWN);
    ZrCore_OptimizationRemarks_PageFree(&page);

    filter.sourceVersion = 8u;
    assert(ZrCore_OptimizationRemarks_Query(&store, &filter, &page, NULL));
    assert(page.count == 0u);
    assert(page.totalMatches == 0u);
    ZrCore_OptimizationRemarks_PageFree(&page);
    ZrCore_OptimizationRemarks_StoreFree(&store);
}

static void test_estimated_and_measured_evidence_are_distinct(void) {
    SZrOptimizationRemark estimated;
    SZrOptimizationRemark measured;
    SZrOptimizationRemarkDiagnostic diagnostic;

    estimated = make_remark(12u, 2u, 201u, 3u,
                            ZR_OPTIMIZATION_REMARK_MISSED,
                            ZR_OPTIMIZATION_REMARK_REASON_NO_PROFILE,
                            ZR_OPTIMIZATION_REMARK_BACKEND_AOT,
                            "layout", 0u);
    estimated.evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_ESTIMATED;
    assert(ZrCore_OptimizationRemark_Validate(&estimated, &diagnostic));
    measured = estimated;
    measured.evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_MEASURED;
    measured.measuredCounterMask = ZR_OPTIMIZATION_REMARK_COUNTER_SOFTWARE_IC;
    measured.softwareIcMisses = 4u;
    assert(ZrCore_OptimizationRemark_Validate(&measured, &diagnostic));
    measured.measuredCounterMask = 0u;
    assert(!ZrCore_OptimizationRemark_Validate(&measured, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_COUNTER_UNAVAILABLE);
    measured.evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_ESTIMATED;
    measured.measuredCounterMask = ZR_OPTIMIZATION_REMARK_COUNTER_SOFTWARE_IC;
    assert(!ZrCore_OptimizationRemark_Validate(&measured, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_COUNTER_UNAVAILABLE);
}

static void test_required_reason_codes_are_stable_and_source_located(void) {
    static const struct {
        EZrOptimizationRemarkReason reason;
        const TZrChar *name;
    } required[] = {
        {ZR_OPTIMIZATION_REMARK_REASON_BOXING, "boxing"},
        {ZR_OPTIMIZATION_REMARK_REASON_BOUNDS, "bounds"},
        {ZR_OPTIMIZATION_REMARK_REASON_VECTOR, "vector"},
        {ZR_OPTIMIZATION_REMARK_REASON_BARRIER, "barrier"},
        {ZR_OPTIMIZATION_REMARK_REASON_INLINING, "inlining"},
        {ZR_OPTIMIZATION_REMARK_REASON_AOT, "aot"},
        {ZR_OPTIMIZATION_REMARK_REASON_DEOPT, "deopt"},
        {ZR_OPTIMIZATION_REMARK_REASON_LAYOUT, "layout"},
        {ZR_OPTIMIZATION_REMARK_REASON_SOFTWARE_IC_MISS, "software_ic_miss"},
        {ZR_OPTIMIZATION_REMARK_REASON_HARDWARE_CACHE_MISS, "hardware_cache_miss"}
    };
    TZrSize index;

    for (index = 0u; index < sizeof(required) / sizeof(required[0]); index++) {
        SZrOptimizationRemark remark = make_remark(
                120u, 6u, (TZrUInt32)(700u + index),
                (TZrUInt32)(30u + index), ZR_OPTIMIZATION_REMARK_MISSED,
                required[index].reason, ZR_OPTIMIZATION_REMARK_BACKEND_JIT,
                "remark-contract", 0u);
        assert(strcmp(ZrCore_OptimizationRemark_ReasonName(required[index].reason),
                      required[index].name) == 0);
        assert(remark.sourceRange.endOffset > remark.sourceRange.startOffset);
        assert(ZrCore_OptimizationRemark_Validate(&remark, NULL));
    }
}

static void test_reason_codes_are_registered_as_informational_diagnostics(void) {
    const SZrDiagnosticDescriptor *descriptor;

    descriptor = ZrParser_DiagnosticRegistry_FindByCode(
            "optimization_remark_bounds");
    assert(descriptor != NULL);
    assert(descriptor->id == 5011u);
    assert(descriptor->defaultSeverity == ZR_STRUCTURED_DIAGNOSTIC_INFO);
    assert(descriptor->category == ZR_LINT_CATEGORY_STYLE);
    assert(ZrParser_DiagnosticRegistry_FindById(5020u) != NULL);
    assert(ZrParser_DiagnosticRegistry_FindByCode(
                   "optimization_remark_not_a_reason") == NULL);
}

static void test_invalid_records_report_specific_reason(void) {
    SZrOptimizationRemark remark;
    SZrOptimizationRemarkDiagnostic diagnostic;

    remark = make_remark(13u, 1u, 301u, 5u,
                         ZR_OPTIMIZATION_REMARK_BLOCKED,
                         ZR_OPTIMIZATION_REMARK_REASON_CODE_BUDGET,
                         ZR_OPTIMIZATION_REMARK_BACKEND_JIT,
                         "inline", 4u);
    remark.sourceRange.endOffset = remark.sourceRange.startOffset - 1u;
    assert(!ZrCore_OptimizationRemark_Validate(&remark, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_RANGE);

    ZrCore_OptimizationRemark_Init(&remark);
    remark.schemaVersion = ZR_OPTIMIZATION_REMARK_SCHEMA_VERSION + 1u;
    assert(!ZrCore_OptimizationRemark_Validate(&remark, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_SCHEMA);
}

static void test_bounded_store_reports_truncation_and_invalidation(void) {
    SZrOptimizationRemarkStore store;
    SZrOptimizationRemark remark;
    SZrOptimizationRemarkPage page;
    SZrOptimizationRemarkQuery filter;
    SZrOptimizationRemarkDiagnostic diagnostic;

    ZrCore_OptimizationRemarks_StoreInit(&store);
    store.maxRecords = 1u;
    remark = make_remark(14u, 3u, 401u, 10u,
                         ZR_OPTIMIZATION_REMARK_SUCCESS,
                         ZR_OPTIMIZATION_REMARK_REASON_NONE,
                         ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC,
                         "bounds", 5u);
    assert(ZrCore_OptimizationRemarks_Append(&store, &remark, &diagnostic));
    remark.sourceId = 402u;
    assert(ZrCore_OptimizationRemarks_Append(&store, &remark, &diagnostic));
    assert(store.count == 1u);
    assert(store.truncated == ZR_TRUE);
    assert(store.droppedCount == 1u);

    ZrCore_OptimizationRemarkQuery_Init(&filter);
    filter.moduleHash = 14u;
    filter.sourceVersion = 3u;
    filter.pageLimit = 4u;
    assert(ZrCore_OptimizationRemarks_Query(&store, &filter, &page, NULL));
    assert(page.truncated == ZR_TRUE && page.droppedCount == 1u);
    ZrCore_OptimizationRemarks_PageFree(&page);

    assert(ZrCore_OptimizationRemarks_InvalidateSourceVersion(
            &store, 14u, 3u, &diagnostic));
    assert(store.count == 0u);
    assert(!ZrCore_OptimizationRemarks_InvalidateSourceVersion(
            &store, 14u, 3u, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_NOT_FOUND);
    ZrCore_OptimizationRemarks_StoreFree(&store);
}

static void test_execir_sink_projection_preserves_canonical_source_identity(void) {
    SZrExecIrOptimizationRemark sourceRemark;
    SZrExecIrRemarkSink sink;
    SZrExecIrFunction function;
    SZrExecIrSourceMap sourceMap;
    SZrParserOptimizationRemarkContext context;
    SZrOptimizationRemarkStore store;

    memset(&sourceRemark, 0, sizeof(sourceRemark));
    sourceRemark.pass = "loop-specialize";
    sourceRemark.sourceId = 77u;
    sourceRemark.outcome = ZR_EXEC_IR_REMARK_MISSED;
    sourceRemark.reasonCode = ZR_EXEC_IR_PASS_REASON_BUDGET;
    sourceRemark.beforeHash = UINT64_C(0x1111222233334444);
    sourceRemark.afterHash = UINT64_C(0x5555666677778888);
    sink.items = &sourceRemark;
    sink.count = 1u;
    sink.capacity = 1u;
    memset(&function, 0, sizeof(function));
    sourceMap.sourceId = 77u;
    sourceMap.startOffset = 18u;
    sourceMap.endOffset = 31u;
    function.sourceMaps = &sourceMap;
    function.sourceMapCount = 1u;
    ZrParser_OptimizationRemarkContext_Init(&context);
    context.moduleHash = 900u;
    context.irHash = 901u;
    context.sourceVersion = 12u;
    context.backendMask = ZR_OPTIMIZATION_REMARK_BACKEND_JIT;
    context.siteKey = UINT64_C(0x12345678);
    ZrCore_OptimizationRemarks_StoreInit(&store);
    assert(ZrParser_OptimizationRemarks_ImportExecIr(
            &sink, &function, &context, &store, NULL));
    assert(store.count == 1u);
    assert(store.items[0].moduleHash == 900u);
    assert(store.items[0].siteKey == UINT64_C(0x12345678));
    assert(store.items[0].sourceVersion == 12u);
    assert(store.items[0].sourceRange.startOffset == 18u);
    assert(store.items[0].sourceRange.endOffset == 31u);
    assert(store.items[0].status == ZR_OPTIMIZATION_REMARK_MISSED);
    assert(store.items[0].reason == ZR_OPTIMIZATION_REMARK_REASON_CODE_BUDGET);
    assert(store.items[0].evidence == ZR_OPTIMIZATION_REMARK_EVIDENCE_ESTIMATED);
    assert(strcmp(store.items[0].pass, "loop-specialize") == 0);
    ZrCore_OptimizationRemarks_StoreFree(&store);
}

static void test_json_projection_is_deterministic_and_never_claims_unknown_measurement(void) {
    SZrOptimizationRemark remark;
    SZrOptimizationRemarkDiagnostic diagnostic;
    TZrChar json[4096];
    TZrSize written = 0u;

    remark = make_remark(15u, 4u, 501u, 20u,
                         ZR_OPTIMIZATION_REMARK_MISSED,
                         ZR_OPTIMIZATION_REMARK_REASON_NO_PROFILE,
                         ZR_OPTIMIZATION_REMARK_BACKEND_AOT,
                         "vectorize", 0u);
    remark.evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_ESTIMATED;
    assert(ZrCore_OptimizationRemark_WriteJson(
            &remark, json, sizeof(json), &written, &diagnostic));
    assert(written > 0u && json[written] == '\0');
    assert(strstr(json, "\"evidenceKind\":\"estimated\"") != NULL);
    assert(strstr(json, "\"reason\":\"no_profile\"") != NULL);
    assert(strstr(json, "\"hardwareCacheMisses\":null") != NULL);
    assert(strstr(json, "\"evidenceKind\":\"measured\"") == NULL);
    assert(!ZrCore_OptimizationRemark_WriteJson(
            &remark, json, 8u, &written, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_BUFFER_TOO_SMALL);
}

static void test_import_is_transactional_when_a_later_record_is_invalid(void) {
    SZrOptimizationRemark existing;
    SZrExecIrOptimizationRemark sourceItems[2];
    SZrExecIrRemarkSink sink;
    SZrOptimizationRemarkStore store;
    SZrOptimizationRemarkDiagnostic diagnostic;

    existing = make_remark(16u, 5u, 601u, 2u,
                           ZR_OPTIMIZATION_REMARK_SUCCESS,
                           ZR_OPTIMIZATION_REMARK_REASON_NONE,
                           ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC,
                           "existing", 1u);
    memset(sourceItems, 0, sizeof(sourceItems));
    sourceItems[0].pass = "first";
    sourceItems[0].sourceId = 602u;
    sourceItems[0].outcome = ZR_EXEC_IR_REMARK_SUCCESS;
    sourceItems[0].reasonCode = ZR_EXEC_IR_PASS_REASON_NONE;
    sourceItems[1] = sourceItems[0];
    sourceItems[1].pass = "invalid";
    sourceItems[1].sourceId = 603u;
    sourceItems[1].outcome = (EZrExecIrRemarkOutcome)99u;
    sink.items = sourceItems;
    sink.count = 2u;
    sink.capacity = 2u;
    ZrCore_OptimizationRemarks_StoreInit(&store);
    assert(ZrCore_OptimizationRemarks_Append(&store, &existing, &diagnostic));
    assert(!ZrParser_OptimizationRemarks_ImportExecIr(
            &sink, NULL, NULL, &store, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_STATUS);
    assert(store.count == 1u);
    assert(store.items[0].sourceId == 601u);
    ZrCore_OptimizationRemarks_StoreFree(&store);
}

static void test_import_rejects_malformed_sink_without_reading_past_capacity(void) {
    SZrExecIrOptimizationRemark sourceRemark;
    SZrExecIrRemarkSink sink;
    SZrOptimizationRemarkStore store;
    SZrOptimizationRemarkDiagnostic diagnostic;

    memset(&sourceRemark, 0, sizeof(sourceRemark));
    sourceRemark.pass = "bounded";
    sourceRemark.outcome = ZR_EXEC_IR_REMARK_SUCCESS;
    sink.items = &sourceRemark;
    sink.count = 2u;
    sink.capacity = 1u;
    ZrCore_OptimizationRemarks_StoreInit(&store);
    assert(!ZrParser_OptimizationRemarks_ImportExecIr(
            &sink, NULL, NULL, &store, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT);
    assert(store.count == 0u);
    ZrCore_OptimizationRemarks_StoreFree(&store);
}

static void test_unknown_execir_reason_is_not_silently_relabelled(void) {
    SZrExecIrOptimizationRemark sourceRemark;
    SZrOptimizationRemark destination;
    SZrOptimizationRemarkDiagnostic diagnostic;

    memset(&sourceRemark, 0, sizeof(sourceRemark));
    sourceRemark.pass = "future-pass";
    sourceRemark.outcome = ZR_EXEC_IR_REMARK_MISSED;
    sourceRemark.reasonCode = 999u;
    assert(!ZrParser_OptimizationRemark_FromExecIr(
            &sourceRemark, NULL, NULL, &destination, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_REASON);
}

static void test_missing_execir_pass_is_rejected(void) {
    SZrExecIrOptimizationRemark sourceRemark;
    SZrOptimizationRemark destination;
    SZrOptimizationRemarkDiagnostic diagnostic;

    memset(&sourceRemark, 0, sizeof(sourceRemark));
    sourceRemark.outcome = ZR_EXEC_IR_REMARK_SUCCESS;
    assert(!ZrParser_OptimizationRemark_FromExecIr(
            &sourceRemark, NULL, NULL, &destination, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT);
}

static void test_no_change_missed_remark_gets_a_nonempty_explanation(void) {
    SZrExecIrOptimizationRemark sourceRemark;
    SZrParserOptimizationRemarkContext context;
    SZrOptimizationRemark destination;
    SZrOptimizationRemarkDiagnostic diagnostic;

    memset(&sourceRemark, 0, sizeof(sourceRemark));
    sourceRemark.pass = "pass-manager";
    sourceRemark.outcome = ZR_EXEC_IR_REMARK_MISSED;
    sourceRemark.reasonCode = ZR_EXEC_IR_PASS_REASON_NO_CHANGE;
    ZrParser_OptimizationRemarkContext_Init(&context);
    context.hasProfile = ZR_TRUE;
    assert(ZrParser_OptimizationRemark_FromExecIr(
            &sourceRemark, NULL, &context, &destination, &diagnostic));
    assert(destination.reason != ZR_OPTIMIZATION_REMARK_REASON_NONE);
    assert(destination.evidence == ZR_OPTIMIZATION_REMARK_EVIDENCE_PROVEN);
}

static void test_loop_reason_prefix_is_mapped_at_parser_boundary(void) {
    SZrExecIrOptimizationRemark sourceRemark;
    SZrOptimizationRemark destination;
    SZrOptimizationRemarkDiagnostic diagnostic;

    memset(&sourceRemark, 0, sizeof(sourceRemark));
    sourceRemark.pass = "licm";
    sourceRemark.outcome = ZR_EXEC_IR_REMARK_BLOCKED;
    sourceRemark.reasonCode = 100u + 9u; /* EZrExecIrLoopReason::BUDGET */
    assert(ZrParser_OptimizationRemark_FromExecIr(
            &sourceRemark, NULL, NULL, &destination, &diagnostic));
    assert(destination.reason == ZR_OPTIMIZATION_REMARK_REASON_CODE_BUDGET);
}

static void test_no_change_with_measured_counters_remains_explainable(void) {
    SZrExecIrOptimizationRemark sourceRemark;
    SZrParserOptimizationRemarkContext context;
    SZrOptimizationRemark destination;
    SZrOptimizationRemarkDiagnostic diagnostic;

    memset(&sourceRemark, 0, sizeof(sourceRemark));
    sourceRemark.pass = "runtime-site";
    sourceRemark.outcome = ZR_EXEC_IR_REMARK_BLOCKED;
    sourceRemark.reasonCode = ZR_EXEC_IR_PASS_REASON_NO_CHANGE;
    ZrParser_OptimizationRemarkContext_Init(&context);
    context.hasMeasuredCounters = ZR_TRUE;
    context.measuredCounterMask = ZR_OPTIMIZATION_REMARK_COUNTER_SOFTWARE_IC;
    context.softwareIcMisses = 2u;
    assert(ZrParser_OptimizationRemark_FromExecIr(
            &sourceRemark, NULL, &context, &destination, &diagnostic));
    assert(destination.reason != ZR_OPTIMIZATION_REMARK_REASON_NONE);
    assert(destination.evidence == ZR_OPTIMIZATION_REMARK_EVIDENCE_MEASURED);
}

static void test_success_without_profile_is_not_claimed_as_proven(void) {
    SZrExecIrOptimizationRemark sourceRemark;
    SZrOptimizationRemark destination;
    SZrOptimizationRemarkDiagnostic diagnostic;

    memset(&sourceRemark, 0, sizeof(sourceRemark));
    sourceRemark.pass = "unprofiled-pass";
    sourceRemark.outcome = ZR_EXEC_IR_REMARK_SUCCESS;
    sourceRemark.reasonCode = ZR_EXEC_IR_PASS_REASON_NONE;
    assert(ZrParser_OptimizationRemark_FromExecIr(
                &sourceRemark, NULL, NULL, &destination, &diagnostic));
    assert(destination.evidence == ZR_OPTIMIZATION_REMARK_EVIDENCE_UNAVAILABLE);
}

static void test_core_rejects_malformed_store_query_and_page(void) {
    SZrOptimizationRemarkStore store;
    SZrOptimizationRemarkStore malformedStore;
    SZrOptimizationRemark remark;
    SZrOptimizationRemarkQuery query;
    SZrOptimizationRemarkPage page = {0};
    SZrOptimizationRemarkDiagnostic diagnostic;
    TZrChar json[256];

    assert(ZR_OPTIMIZATION_REMARK_REASON_MASK(
                   (EZrOptimizationRemarkReason)32u) == 0u);
    assert(ZR_OPTIMIZATION_REMARK_STATUS_MASK(
                   (EZrOptimizationRemarkStatus)32u) == 0u);
    assert(ZR_OPTIMIZATION_REMARK_EVIDENCE_MASK(
                   (EZrOptimizationRemarkEvidence)32u) == 0u);

    remark = make_remark(17u, 6u, 701u, 4u,
                         ZR_OPTIMIZATION_REMARK_SUCCESS,
                         ZR_OPTIMIZATION_REMARK_REASON_NONE,
                         ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC,
                         "query-edge", 1u);
    ZrCore_OptimizationRemarks_StoreInit(&store);
    assert(ZrCore_OptimizationRemarks_Append(&store, &remark, &diagnostic));

    ZrCore_OptimizationRemarkQuery_Init(&query);
    query.evidenceMask = (TZrUInt32)1u <<
                         (TZrUInt32)ZR_OPTIMIZATION_REMARK_EVIDENCE_COUNT;
    assert(!ZrCore_OptimizationRemarks_Query(
            &store, &query, &page, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY);
    ZrCore_OptimizationRemarks_PageFree(&page);

    /* A caller cannot bypass the schema by mutating a public store row: the
     * query path validates rows before using their enum values. */
    store.items[0].reason = (EZrOptimizationRemarkReason)
                            ZR_OPTIMIZATION_REMARK_REASON_COUNT;
    ZrCore_OptimizationRemarkQuery_Init(&query);
    assert(!ZrCore_OptimizationRemarks_Query(
            &store, &query, &page, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_REASON);
    store.items[0] = remark;
    ZrCore_OptimizationRemarks_PageFree(&page);

    ZrCore_OptimizationRemarkQuery_Init(&query);
    query.hasPass = ZR_TRUE;
    (void)memset(query.pass, 'x', sizeof(query.pass));
    assert(!ZrCore_OptimizationRemarks_Query(
            &store, &query, &page, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_QUERY);
    ZrCore_OptimizationRemarks_PageFree(&page);

    ZrCore_OptimizationRemarks_StoreInit(&malformedStore);
    malformedStore.count = 1u;
    malformedStore.capacity = 1u;
    malformedStore.items = ZR_NULL;
    ZrCore_OptimizationRemarkQuery_Init(&query);
    assert(!ZrCore_OptimizationRemarks_Query(
            &malformedStore, &query, &page, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT);
    assert(!ZrCore_OptimizationRemarks_Append(
            &malformedStore, &remark, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT);
    assert(!ZrCore_OptimizationRemarks_InvalidateSourceVersion(
            &malformedStore, 17u, 6u, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_INVALID_ARGUMENT);
    ZrCore_OptimizationRemarks_StoreFree(&malformedStore);

    page.items = &remark;
    page.count = 1u;
    page.totalMatches = 1u;
    remark.schemaVersion = ZR_OPTIMIZATION_REMARK_SCHEMA_VERSION + 1u;
    assert(!ZrCore_OptimizationRemarks_PageWriteJson(
            &page, json, sizeof(json), NULL, &diagnostic));
    assert(diagnostic.code == ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_SCHEMA);
    page.items = ZR_NULL; /* stack-backed probe, not an owned page */
    ZrCore_OptimizationRemarks_PageFree(&page);
    ZrCore_OptimizationRemarks_StoreFree(&store);
}

int main(void) {
    test_proven_records_query_by_source_version_and_reason();
    test_estimated_and_measured_evidence_are_distinct();
    test_required_reason_codes_are_stable_and_source_located();
    test_reason_codes_are_registered_as_informational_diagnostics();
    test_invalid_records_report_specific_reason();
    test_bounded_store_reports_truncation_and_invalidation();
    test_execir_sink_projection_preserves_canonical_source_identity();
    test_json_projection_is_deterministic_and_never_claims_unknown_measurement();
    test_import_is_transactional_when_a_later_record_is_invalid();
    test_import_rejects_malformed_sink_without_reading_past_capacity();
    test_unknown_execir_reason_is_not_silently_relabelled();
    test_missing_execir_pass_is_rejected();
    test_no_change_missed_remark_gets_a_nonempty_explanation();
    test_loop_reason_prefix_is_mapped_at_parser_boundary();
    test_no_change_with_measured_counters_remains_explainable();
    test_success_without_profile_is_not_claimed_as_proven();
    test_core_rejects_malformed_store_query_and_page();
    return 0;
}
