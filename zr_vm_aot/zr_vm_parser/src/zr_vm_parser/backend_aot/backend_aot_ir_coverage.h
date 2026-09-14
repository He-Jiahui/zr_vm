#ifndef ZR_VM_PARSER_BACKEND_AOT_IR_COVERAGE_H
#define ZR_VM_PARSER_BACKEND_AOT_IR_COVERAGE_H

#include "backend_aot_ir_adapter.h"

typedef enum EZrBackendAotIrCoverageClass {
    ZR_BACKEND_AOT_IR_COVERAGE_NATIVE = 0,
    ZR_BACKEND_AOT_IR_COVERAGE_RUNTIME_HELPER,
    ZR_BACKEND_AOT_IR_COVERAGE_INTERPRETER_FALLBACK,
    ZR_BACKEND_AOT_IR_COVERAGE_UNSUPPORTED,
    ZR_BACKEND_AOT_IR_COVERAGE_CLASS_COUNT
} EZrBackendAotIrCoverageClass;

typedef struct SZrBackendAotIrCoverage {
    TZrUInt64 semanticSiteCount;
    TZrUInt64 nativeSiteCount;
    TZrUInt64 runtimeHelperSiteCount;
    TZrUInt64 interpreterFallbackSiteCount;
    TZrUInt64 unsupportedSiteCount;
} SZrBackendAotIrCoverage;

typedef struct SZrBackendAotIrCoverageDiagnostic {
    EZrBackendAotIrStatus status;
    EZrBackendAotIrCoverageClass coverageClass;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrBackendAotIrCoverageDiagnostic;

ZR_PARSER_API void backend_aot_ir_coverage_init(
        SZrBackendAotIrCoverage *coverage);

/* Setting the denominator starts a fresh observation window. */
ZR_PARSER_API TZrBool backend_aot_ir_coverage_set_semantic_sites(
        SZrBackendAotIrCoverage *coverage,
        TZrUInt64 semanticSiteCount,
        SZrBackendAotIrCoverageDiagnostic *diagnostic);

ZR_PARSER_API TZrBool backend_aot_ir_coverage_add(
        SZrBackendAotIrCoverage *coverage,
        EZrBackendAotIrCoverageClass coverageClass,
        TZrUInt64 count,
        SZrBackendAotIrCoverageDiagnostic *diagnostic);

ZR_PARSER_API TZrBool backend_aot_ir_coverage_validate(
        const SZrBackendAotIrCoverage *coverage,
        SZrBackendAotIrCoverageDiagnostic *diagnostic);

/* Converts static lowering facts to a coverage denominator.  Every AOTIR
 * instruction contributes one semantic site, including bridges and
 * unsupported operations; fusion or inlining must not shrink this count. */
ZR_PARSER_API TZrBool backend_aot_ir_coverage_from_facts(
        const SZrBackendAotIrFacts *facts,
        SZrBackendAotIrCoverage *outCoverage,
        SZrBackendAotIrCoverageDiagnostic *diagnostic);

/* Returns per-mille values (0..1000).  A zero denominator is unavailable,
 * never 100%, and leaves the output values unchanged. */
ZR_PARSER_API TZrBool backend_aot_ir_coverage_ratio_per_mille(
        const SZrBackendAotIrCoverage *coverage,
        TZrUInt32 *outNative,
        TZrUInt32 *outRuntimeHelper,
        TZrUInt32 *outInterpreterFallback,
        SZrBackendAotIrCoverageDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_BACKEND_AOT_IR_COVERAGE_H */
