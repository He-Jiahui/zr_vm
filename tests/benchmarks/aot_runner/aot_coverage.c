#include "aot_coverage.h"

#include <string.h>

static TZrBool zr_aot_coverage_add_u64(TZrUInt64 left,
                                       TZrUInt64 right,
                                       TZrUInt64 *result) {
    if (result == ZR_NULL || left > UINT64_MAX - right) {
        return ZR_FALSE;
    }
    *result = left + right;
    return ZR_TRUE;
}

static TZrBool zr_aot_coverage_class_is_semantic(EZrAotCoverageClass coverageClass) {
    return (TZrBool)(coverageClass == ZR_AOT_COVERAGE_CLASS_NATIVE ||
                     coverageClass == ZR_AOT_COVERAGE_CLASS_NATIVE_HELPER ||
                     coverageClass == ZR_AOT_COVERAGE_CLASS_INTERPRETER);
}

static TZrBool zr_aot_coverage_semantic_sum(const SZrAotCoverageCounts *counts,
                                            TZrUInt64 *sum) {
    TZrUInt64 partial;

    if (counts == ZR_NULL || sum == ZR_NULL ||
        !zr_aot_coverage_add_u64(counts->nativeSites,
                                 counts->nativeHelperSites,
                                 &partial) ||
        !zr_aot_coverage_add_u64(partial,
                                 counts->interpreterSites,
                                 sum)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

void ZrTests_AotCoverage_Init(SZrAotCoverageCounts *counts) {
    if (counts != ZR_NULL) {
        memset(counts, 0, sizeof(*counts));
    }
}

TZrBool ZrTests_AotCoverage_Validate(const SZrAotCoverageCounts *counts) {
    TZrUInt64 semanticSum;
    TZrUInt64 executedSites;

    if (counts == ZR_NULL ||
        counts->sampleRatePermille > ZR_AOT_COVERAGE_MAX_SAMPLE_PERMILLE ||
        !zr_aot_coverage_semantic_sum(counts, &semanticSum)) {
        return ZR_FALSE;
    }
    executedSites = counts->executedSemanticSites != 0u
                            ? counts->executedSemanticSites
                            : semanticSum;
    if (counts->executedSemanticSites != 0u &&
        counts->executedSemanticSites != semanticSum) {
        return ZR_FALSE;
    }
    if (counts->semanticSites != 0u && counts->semanticSites < executedSites) {
        return ZR_FALSE;
    }
    if ((counts->totalTimeNs == 0u && counts->fallbackTimeNs != 0u) ||
        (counts->totalTimeNs != 0u && counts->fallbackTimeNs > counts->totalTimeNs)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrTests_AotCoverage_Compute(const SZrAotCoverageCounts *counts,
                                    SZrAotCoverageReport *report) {
    TZrUInt64 semanticSum;
    TZrUInt64 denominator;
    TZrUInt64 executedSites;

    if (report == ZR_NULL || !ZrTests_AotCoverage_Validate(counts) ||
        !zr_aot_coverage_semantic_sum(counts, &semanticSum)) {
        return ZR_FALSE;
    }

    memset(report, 0, sizeof(*report));
    report->status = ZR_AOT_COVERAGE_STATUS_UNAVAILABLE;
    report->nativeCoverage = -1.0;
    report->nativeExecutionCoverage = -1.0;
    report->nativeStaticCoverage = -1.0;
    report->nativeExecutionStaticCoverage = -1.0;
    report->nativeExecutionCoverage = -1.0;
    report->nativeHelperShare = -1.0;
    report->interpreterShare = -1.0;
    report->fallbackTimeShare = -1.0;
    executedSites = counts->executedSemanticSites != 0u
                            ? counts->executedSemanticSites
                            : semanticSum;
    denominator = counts->semanticSites != 0u ? counts->semanticSites : executedSites;
    report->semanticSites = denominator;
    report->executedSemanticSites = executedSites;
    report->nativeSites = counts->nativeSites;
    report->nativeHelperSites = counts->nativeHelperSites;
    report->interpreterSites = counts->interpreterSites;
    report->deoptCount = counts->deoptCount;
    report->sampleRatePermille = counts->sampleRatePermille;

    /* A producer must identify its sampling rate before a ratio is usable. */
    if (executedSites == 0u || counts->sampleRatePermille == 0u) {
        return ZR_TRUE;
    }

    report->status = ZR_AOT_COVERAGE_STATUS_AVAILABLE;
    report->available = ZR_TRUE;
    report->exact = (TZrBool)(counts->sampleRatePermille ==
                              ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE);
    report->mixedExecution = (TZrBool)((counts->nativeSites != 0u ||
                                        counts->nativeHelperSites != 0u) &&
                                       counts->interpreterSites != 0u);
    report->nativeCoverage = (double)counts->nativeSites / (double)executedSites;
    report->nativeExecutionCoverage =
            (double)(counts->nativeSites + counts->nativeHelperSites) /
            (double)executedSites;
    report->nativeStaticCoverage = (double)counts->nativeSites / (double)denominator;
    report->nativeExecutionStaticCoverage =
            (double)(counts->nativeSites + counts->nativeHelperSites) /
            (double)denominator;
    report->nativeHelperShare = (double)counts->nativeHelperSites / (double)executedSites;
    report->interpreterShare = (double)counts->interpreterSites / (double)executedSites;
    if (counts->totalTimeNs != 0u) {
        report->fallbackTimeShare = (double)counts->fallbackTimeNs /
                                    (double)counts->totalTimeNs;
    }
    return ZR_TRUE;
}

TZrBool ZrTests_AotCoverage_Record(SZrAotCoverageCounts *counts,
                                   EZrAotCoverageClass coverageClass,
                                   TZrUInt64 amount) {
    SZrAotCoverageCounts candidate;
    TZrUInt64 *counter;
    TZrUInt64 nextCounter;
    TZrUInt64 nextSemanticSites;
    TZrUInt64 currentSemanticSum;
    TZrUInt64 nextExecutedSites;

    if (counts == ZR_NULL || !ZrTests_AotCoverage_Validate(counts) ||
        coverageClass < ZR_AOT_COVERAGE_CLASS_NATIVE ||
        coverageClass >= ZR_AOT_COVERAGE_CLASS_COUNT) {
        return ZR_FALSE;
    }
    /* Build a candidate and validate it before publishing.  In particular,
     * an explicit static denominator must not be left inconsistent when a
     * producer records more executed work than it declared. */
    candidate = *counts;
    switch (coverageClass) {
        case ZR_AOT_COVERAGE_CLASS_NATIVE:
            counter = &candidate.nativeSites;
            break;
        case ZR_AOT_COVERAGE_CLASS_NATIVE_HELPER:
            counter = &candidate.nativeHelperSites;
            break;
        case ZR_AOT_COVERAGE_CLASS_INTERPRETER:
            counter = &candidate.interpreterSites;
            break;
        case ZR_AOT_COVERAGE_CLASS_DEOPT:
            counter = &candidate.deoptCount;
            break;
        case ZR_AOT_COVERAGE_CLASS_COUNT:
        default:
            return ZR_FALSE;
    }
    nextCounter = *counter;
    if (!zr_aot_coverage_add_u64(nextCounter, amount, &nextCounter)) {
        return ZR_FALSE;
    }
    nextSemanticSites = candidate.semanticSites;
    nextExecutedSites = candidate.executedSemanticSites;
    if (zr_aot_coverage_class_is_semantic(coverageClass)) {
        if (nextExecutedSites == 0u) {
            if (!zr_aot_coverage_semantic_sum(&candidate, &currentSemanticSum)) {
                return ZR_FALSE;
            }
            nextExecutedSites = currentSemanticSum;
        }
        if (!zr_aot_coverage_add_u64(nextExecutedSites,
                                     amount,
                                     &nextExecutedSites)) {
            return ZR_FALSE;
        }
    }
    *counter = nextCounter;
    candidate.semanticSites = nextSemanticSites;
    candidate.executedSemanticSites = nextExecutedSites;
    if (!ZrTests_AotCoverage_Validate(&candidate)) {
        return ZR_FALSE;
    }
    *counts = candidate;
    return ZR_TRUE;
}

TZrBool ZrTests_AotCoverage_Merge(SZrAotCoverageCounts *destination,
                                  const SZrAotCoverageCounts *source) {
    SZrAotCoverageCounts merged;
    TZrUInt64 destinationSum;
    TZrUInt64 sourceSum;
    TZrUInt64 destinationDenominator;
    TZrUInt64 sourceDenominator;
    TZrUInt64 destinationExecuted;
    TZrUInt64 sourceExecuted;
    TZrUInt64 mergedExecuted;
    TZrUInt64 mergedDenominator;
    TZrUInt32 mergedSampleRate;

    if (destination == ZR_NULL ||
        !ZrTests_AotCoverage_Validate(destination) ||
        !ZrTests_AotCoverage_Validate(source) ||
        !zr_aot_coverage_semantic_sum(destination, &destinationSum) ||
        !zr_aot_coverage_semantic_sum(source, &sourceSum)) {
        return ZR_FALSE;
    }
    destinationDenominator = destination->semanticSites != 0u
                                     ? destination->semanticSites
                                     : destinationSum;
    sourceDenominator = source->semanticSites != 0u ? source->semanticSites : sourceSum;
    destinationExecuted = destination->executedSemanticSites != 0u
                                  ? destination->executedSemanticSites
                                  : destinationSum;
    sourceExecuted = source->executedSemanticSites != 0u
                             ? source->executedSemanticSites
                             : sourceSum;
    merged = *destination;
    if (!zr_aot_coverage_add_u64(destinationDenominator,
                                 sourceDenominator,
                                 &mergedDenominator) ||
        !zr_aot_coverage_add_u64(destinationExecuted,
                                 sourceExecuted,
                                 &mergedExecuted) ||
        !zr_aot_coverage_add_u64(merged.nativeSites,
                                 source->nativeSites,
                                 &merged.nativeSites) ||
        !zr_aot_coverage_add_u64(merged.nativeHelperSites,
                                 source->nativeHelperSites,
                                 &merged.nativeHelperSites) ||
        !zr_aot_coverage_add_u64(merged.interpreterSites,
                                 source->interpreterSites,
                                 &merged.interpreterSites) ||
        !zr_aot_coverage_add_u64(merged.deoptCount,
                                 source->deoptCount,
                                 &merged.deoptCount) ||
        !zr_aot_coverage_add_u64(merged.fallbackTimeNs,
                                 source->fallbackTimeNs,
                                 &merged.fallbackTimeNs) ||
        !zr_aot_coverage_add_u64(merged.totalTimeNs,
                                 source->totalTimeNs,
                                 &merged.totalTimeNs)) {
        return ZR_FALSE;
    }
    mergedSampleRate = merged.sampleRatePermille;
    if (mergedSampleRate == 0u) {
        mergedSampleRate = source->sampleRatePermille;
    } else if (source->sampleRatePermille == 0u && sourceDenominator != 0u) {
        mergedSampleRate = 0u;
    } else if (source->sampleRatePermille != 0u &&
               mergedSampleRate != source->sampleRatePermille) {
        /* A mixed-rate aggregate is intentionally not presented as exact. */
        mergedSampleRate = 0u;
    }
    merged.sampleRatePermille = mergedSampleRate;
    merged.semanticSites = mergedDenominator;
    merged.executedSemanticSites = mergedExecuted;
    if (!ZrTests_AotCoverage_Validate(&merged)) {
        return ZR_FALSE;
    }
    *destination = merged;
    return ZR_TRUE;
}

const TZrChar *ZrTests_AotCoverage_StatusName(EZrAotCoverageStatus status) {
    switch (status) {
        case ZR_AOT_COVERAGE_STATUS_AVAILABLE:
            return "AVAILABLE";
        case ZR_AOT_COVERAGE_STATUS_UNAVAILABLE:
            return "UNAVAILABLE";
        case ZR_AOT_COVERAGE_STATUS_INVALID:
        case ZR_AOT_COVERAGE_STATUS_COUNT:
        default:
            return "INVALID";
    }
}

const TZrChar *ZrTests_AotCoverage_ClassName(EZrAotCoverageClass coverageClass) {
    switch (coverageClass) {
        case ZR_AOT_COVERAGE_CLASS_NATIVE:
            return "native";
        case ZR_AOT_COVERAGE_CLASS_NATIVE_HELPER:
            return "native-helper";
        case ZR_AOT_COVERAGE_CLASS_INTERPRETER:
            return "interpreter-fallback";
        case ZR_AOT_COVERAGE_CLASS_DEOPT:
            return "deopt";
        case ZR_AOT_COVERAGE_CLASS_COUNT:
        default:
            return "invalid";
    }
}
