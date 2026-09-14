#ifndef ZR_VM_TESTS_BENCHMARKS_AOT_COVERAGE_H
#define ZR_VM_TESTS_BENCHMARKS_AOT_COVERAGE_H

/*
 * Runtime coverage contract for the standalone AOT benchmark runner.
 *
 * Counts are semantic-site observations, not emitted-instruction counts.  A
 * native helper and an interpreter fallback therefore remain separate classes
 * in the denominator.  The contract deliberately has no allocator or VM
 * dependency so generated AOT entries can report it without changing their
 * ownership model.
 */

#include "zr_vm_common/zr_common_conf.h"

#define ZR_AOT_COVERAGE_FULL_SAMPLE_PERMILLE 1000u
#define ZR_AOT_COVERAGE_MAX_SAMPLE_PERMILLE 1000u

typedef enum EZrAotCoverageStatus {
    ZR_AOT_COVERAGE_STATUS_INVALID = 0,
    ZR_AOT_COVERAGE_STATUS_AVAILABLE,
    ZR_AOT_COVERAGE_STATUS_UNAVAILABLE,
    ZR_AOT_COVERAGE_STATUS_COUNT
} EZrAotCoverageStatus;

typedef enum EZrAotCoverageClass {
    ZR_AOT_COVERAGE_CLASS_NATIVE = 0,
    ZR_AOT_COVERAGE_CLASS_NATIVE_HELPER,
    ZR_AOT_COVERAGE_CLASS_INTERPRETER,
    ZR_AOT_COVERAGE_CLASS_DEOPT,
    ZR_AOT_COVERAGE_CLASS_COUNT
} EZrAotCoverageClass;

/*
 * The first four fields are the stable minimum contract from 07.04.  The
 * remaining fields make the semantic denominator, sampling, and time share
 * explicit without changing the meaning of those fields.
 */
typedef struct SZrAotCoverageCounts {
    TZrUInt64 nativeSites;
    TZrUInt64 nativeHelperSites;
    TZrUInt64 interpreterSites;
    TZrUInt64 deoptCount;

    /* Zero derives the denominator from the three semantic classes. */
    TZrUInt64 semanticSites;
    /* Runtime hits; zero derives from native/helper/interpreter counters. */
    TZrUInt64 executedSemanticSites;
    /* Zero means that the producer did not provide sampling metadata. */
    TZrUInt32 sampleRatePermille;
    /* Optional wall-time attribution for fallback reporting. */
    TZrUInt64 fallbackTimeNs;
    TZrUInt64 totalTimeNs;
} SZrAotCoverageCounts;

typedef struct SZrAotCoverageReport {
    EZrAotCoverageStatus status;
    TZrBool available;
    TZrBool exact;
    TZrBool mixedExecution;
    TZrUInt64 semanticSites;
    TZrUInt64 executedSemanticSites;
    TZrUInt64 nativeSites;
    TZrUInt64 nativeHelperSites;
    TZrUInt64 interpreterSites;
    TZrUInt64 deoptCount;
    TZrUInt32 sampleRatePermille;
    double nativeCoverage;
    /* Native execution includes emitted native code and native helpers. */
    double nativeExecutionCoverage;
    /* Ratios against the static denominator, when it is larger than hits. */
    double nativeStaticCoverage;
    double nativeExecutionStaticCoverage;
    double nativeHelperShare;
    double interpreterShare;
    double fallbackTimeShare;
} SZrAotCoverageReport;

void ZrTests_AotCoverage_Init(SZrAotCoverageCounts *counts);

/* Structural validation; zero observations are valid but have no ratio. */
TZrBool ZrTests_AotCoverage_Validate(const SZrAotCoverageCounts *counts);

/* Compute ratios without ever turning an empty denominator into 100 percent. */
TZrBool ZrTests_AotCoverage_Compute(const SZrAotCoverageCounts *counts,
                                    SZrAotCoverageReport *report);

TZrBool ZrTests_AotCoverage_Record(SZrAotCoverageCounts *counts,
                                   EZrAotCoverageClass coverageClass,
                                   TZrUInt64 amount);

/* Merge independent samples while preserving semantic-site accounting. */
TZrBool ZrTests_AotCoverage_Merge(SZrAotCoverageCounts *destination,
                                  const SZrAotCoverageCounts *source);

const TZrChar *ZrTests_AotCoverage_StatusName(EZrAotCoverageStatus status);
const TZrChar *ZrTests_AotCoverage_ClassName(EZrAotCoverageClass coverageClass);

#endif /* ZR_VM_TESTS_BENCHMARKS_AOT_COVERAGE_H */
