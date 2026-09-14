#include "backend_aot_ir_coverage.h"

#include <string.h>

static void backend_aot_ir_coverage_clear_diagnostic(
        SZrBackendAotIrCoverageDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        (void)memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_BACKEND_AOT_IR_OK;
    }
}

static TZrBool backend_aot_ir_coverage_class_valid(
        EZrBackendAotIrCoverageClass coverageClass) {
    return (TZrBool)(coverageClass >= ZR_BACKEND_AOT_IR_COVERAGE_NATIVE &&
                     coverageClass < ZR_BACKEND_AOT_IR_COVERAGE_CLASS_COUNT);
}

static TZrBool backend_aot_ir_coverage_add_checked(TZrUInt64 *value,
                                                   TZrUInt64 amount) {
    if (value == ZR_NULL || amount > UINT64_MAX - *value) {
        return ZR_FALSE;
    }
    *value += amount;
    return ZR_TRUE;
}

void backend_aot_ir_coverage_init(SZrBackendAotIrCoverage *coverage) {
    if (coverage != ZR_NULL) {
        (void)memset(coverage, 0, sizeof(*coverage));
    }
}

TZrBool backend_aot_ir_coverage_set_semantic_sites(
        SZrBackendAotIrCoverage *coverage,
        TZrUInt64 semanticSiteCount,
        SZrBackendAotIrCoverageDiagnostic *diagnostic) {
    backend_aot_ir_coverage_clear_diagnostic(diagnostic);
    if (coverage == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    coverage->semanticSiteCount = semanticSiteCount;
    coverage->nativeSiteCount = 0u;
    coverage->runtimeHelperSiteCount = 0u;
    coverage->interpreterFallbackSiteCount = 0u;
    coverage->unsupportedSiteCount = 0u;
    return ZR_TRUE;
}

TZrBool backend_aot_ir_coverage_add(
        SZrBackendAotIrCoverage *coverage,
        EZrBackendAotIrCoverageClass coverageClass,
        TZrUInt64 count,
        SZrBackendAotIrCoverageDiagnostic *diagnostic) {
    TZrUInt64 *classCount = ZR_NULL;

    backend_aot_ir_coverage_clear_diagnostic(diagnostic);
    if (coverage == ZR_NULL || !backend_aot_ir_coverage_class_valid(coverageClass)) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_COVERAGE_INVALID;
            diagnostic->coverageClass = coverageClass;
        }
        return ZR_FALSE;
    }
    switch (coverageClass) {
        case ZR_BACKEND_AOT_IR_COVERAGE_NATIVE:
            classCount = &coverage->nativeSiteCount;
            break;
        case ZR_BACKEND_AOT_IR_COVERAGE_RUNTIME_HELPER:
            classCount = &coverage->runtimeHelperSiteCount;
            break;
        case ZR_BACKEND_AOT_IR_COVERAGE_INTERPRETER_FALLBACK:
            classCount = &coverage->interpreterFallbackSiteCount;
            break;
        case ZR_BACKEND_AOT_IR_COVERAGE_UNSUPPORTED:
            classCount = &coverage->unsupportedSiteCount;
            break;
        case ZR_BACKEND_AOT_IR_COVERAGE_CLASS_COUNT:
        default:
            break;
    }
    if (!backend_aot_ir_coverage_add_checked(classCount, count)) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_CAPACITY;
            diagnostic->coverageClass = coverageClass;
            diagnostic->expected = UINT64_MAX;
            diagnostic->actual = count;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool backend_aot_ir_coverage_validate(
        const SZrBackendAotIrCoverage *coverage,
        SZrBackendAotIrCoverageDiagnostic *diagnostic) {
    TZrUInt64 total;

    backend_aot_ir_coverage_clear_diagnostic(diagnostic);
    if (coverage == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    if (coverage->semanticSiteCount == 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_COVERAGE_UNAVAILABLE;
            diagnostic->expected = 1u;
            diagnostic->actual = 0u;
        }
        return ZR_FALSE;
    }
    total = coverage->nativeSiteCount;
    if (!backend_aot_ir_coverage_add_checked(&total,
                                             coverage->runtimeHelperSiteCount) ||
        !backend_aot_ir_coverage_add_checked(
                &total, coverage->interpreterFallbackSiteCount) ||
        !backend_aot_ir_coverage_add_checked(&total,
                                             coverage->unsupportedSiteCount)) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_COVERAGE_INVALID;
        }
        return ZR_FALSE;
    }
    if (total != coverage->semanticSiteCount) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_COVERAGE_INVALID;
            diagnostic->expected = coverage->semanticSiteCount;
            diagnostic->actual = total;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool backend_aot_ir_coverage_from_facts(
        const SZrBackendAotIrFacts *facts,
        SZrBackendAotIrCoverage *outCoverage,
        SZrBackendAotIrCoverageDiagnostic *diagnostic) {
    TZrUInt32 uncoveredUnsupported;

    backend_aot_ir_coverage_clear_diagnostic(diagnostic);
    if (outCoverage != ZR_NULL) {
        backend_aot_ir_coverage_init(outCoverage);
    }
    if (facts == ZR_NULL || outCoverage == ZR_NULL ||
        facts->semanticSiteCount != facts->instructionCount ||
        facts->nativeLoweredCount > facts->semanticSiteCount ||
        facts->runtimeBridgeCount > facts->semanticSiteCount -
                                     facts->nativeLoweredCount ||
        facts->interpreterFallbackCount > facts->semanticSiteCount -
                                          facts->nativeLoweredCount -
                                          facts->runtimeBridgeCount ||
        facts->unsupportedCount > facts->semanticSiteCount) {
        if (diagnostic != ZR_NULL) {
            diagnostic->status = ZR_BACKEND_AOT_IR_COVERAGE_INVALID;
        }
        return ZR_FALSE;
    }
    outCoverage->semanticSiteCount = facts->semanticSiteCount;
    outCoverage->nativeSiteCount = facts->nativeLoweredCount;
    outCoverage->runtimeHelperSiteCount = facts->runtimeBridgeCount;
    outCoverage->interpreterFallbackSiteCount =
            facts->interpreterFallbackCount;
    /* An unsupported operation which is explicitly allowed to fall back is
     * annotated in both fields.  Only the portion without a fallback remains
     * uncovered in the mutually-exclusive coverage classes. */
    uncoveredUnsupported = facts->unsupportedCount >
                                   facts->interpreterFallbackCount
                           ? facts->unsupportedCount -
                                     facts->interpreterFallbackCount
                           : 0u;
    outCoverage->unsupportedSiteCount = uncoveredUnsupported;
    return backend_aot_ir_coverage_validate(outCoverage, diagnostic);
}

static TZrUInt32 backend_aot_ir_coverage_ratio(TZrUInt64 count,
                                               TZrUInt64 denominator) {
    TZrUInt32 result = 0u;
    TZrUInt64 remainder = 0u;

    /* Repeated modular addition avoids count * 1000 overflow when a caller
     * uses a near-maximum 64-bit semantic-site count. */
    for (TZrUInt32 step = 0u; step < 1000u; ++step) {
        if (remainder >= denominator - count) {
            remainder -= denominator - count;
            ++result;
        } else {
            remainder += count;
        }
    }
    return result;
}

TZrBool backend_aot_ir_coverage_ratio_per_mille(
        const SZrBackendAotIrCoverage *coverage,
        TZrUInt32 *outNative,
        TZrUInt32 *outRuntimeHelper,
        TZrUInt32 *outInterpreterFallback,
        SZrBackendAotIrCoverageDiagnostic *diagnostic) {
    backend_aot_ir_coverage_clear_diagnostic(diagnostic);
    if (outNative == ZR_NULL || outRuntimeHelper == ZR_NULL ||
        outInterpreterFallback == ZR_NULL ||
        !backend_aot_ir_coverage_validate(coverage, diagnostic)) {
        if (diagnostic != ZR_NULL &&
            diagnostic->status == ZR_BACKEND_AOT_IR_OK) {
            diagnostic->status = ZR_BACKEND_AOT_IR_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    *outNative = backend_aot_ir_coverage_ratio(
            coverage->nativeSiteCount, coverage->semanticSiteCount);
    *outRuntimeHelper = backend_aot_ir_coverage_ratio(
            coverage->runtimeHelperSiteCount, coverage->semanticSiteCount);
    *outInterpreterFallback = backend_aot_ir_coverage_ratio(
            coverage->interpreterFallbackSiteCount,
            coverage->semanticSiteCount);
    return ZR_TRUE;
}
