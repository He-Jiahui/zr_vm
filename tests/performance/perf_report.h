#ifndef ZR_VM_PERF_REPORT_H
#define ZR_VM_PERF_REPORT_H

#include <stdint.h>
#include <stdio.h>

#include "persistent_protocol.h"
#include "perf_statistics.h"
#include "zr_vm_common/zr_common_conf.h"

typedef struct SZrPerfRunSample {
    double wallMs;
    double aggregateWallMs;
    uint64_t peakWorkingSetBytes;
    uint64_t processId;
    int exitCode;
} SZrPerfRunSample;

typedef struct SZrPerfSummary {
    double meanWallMs;
    double medianWallMs;
    double minWallMs;
    double maxWallMs;
    double stddevWallMs;
    double madWallMs;
    double coefficientOfVariation;
    SZrPerfBootstrapInterval bootstrapMedian95;
    double meanPeakWorkingSetBytes;
    double medianPeakWorkingSetBytes;
    uint64_t minPeakWorkingSetBytes;
    uint64_t maxPeakWorkingSetBytes;
} SZrPerfSummary;

typedef struct SZrPerfMeasurementMetadata {
    int initialSampleCount;
    int sampleCount;
    int extraSampleCount;
    uint32_t repetitions;
    int calibrationEnabled;
    double minimumSampleMs;
    double calibrationAggregateWallMs;
    int comparable;
    int gateEligible;
    const char *stability;
    uint64_t bootstrapSeed;
    size_t bootstrapResampleCount;
} SZrPerfMeasurementMetadata;

/*
 * Standalone AOT phase report.  The normal process/persistent report above is
 * intentionally unchanged; this value-only extension lets the AOT runner
 * publish compile/link/load/startup/run costs without folding them into the
 * steady-state sample.  A negative phase value means unavailable, while zero
 * remains a valid measured duration.
 */
#define ZR_PERF_AOT_REPORT_TEXT_CAPACITY 128U

typedef enum EZrPerfAotReportStatus {
    ZR_PERF_AOT_REPORT_INVALID = 0,
    ZR_PERF_AOT_REPORT_RAN,
    ZR_PERF_AOT_REPORT_FALLBACK,
    ZR_PERF_AOT_REPORT_UNAVAILABLE,
    ZR_PERF_AOT_REPORT_FAILED,
    ZR_PERF_AOT_REPORT_STATUS_COUNT
} EZrPerfAotReportStatus;

typedef struct SZrPerfAotPhaseReport {
    EZrPerfAotReportStatus status;
    int processExitCode;
    TZrChar requestedBackend[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];
    TZrChar actualBackend[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];
    TZrChar entryToken[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];
    TZrChar artifactHash[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];
    TZrChar toolchain[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];
    TZrChar failureReason[ZR_PERF_AOT_REPORT_TEXT_CAPACITY];

    double compileMs;
    double linkMs;
    double loadMs;
    double startupMs;
    double runMs;

    TZrBool hasRssBytes;
    TZrUInt64 rssBytes;
    TZrBool hasCodeSizeBytes;
    TZrUInt64 codeSizeBytes;

    TZrBool coverageAvailable;
    TZrUInt64 semanticSites;
    TZrUInt64 executedSemanticSites;
    TZrUInt64 nativeSites;
    TZrUInt64 nativeHelperSites;
    TZrUInt64 interpreterSites;
    TZrUInt64 fallbackCount;
    TZrUInt64 deoptCount;
    double nativeCoverage;
} SZrPerfAotPhaseReport;

const TZrChar *ZrPerfReport_AotStatusName(EZrPerfAotReportStatus status);
int ZrPerfReport_ValidateAotPhase(const SZrPerfAotPhaseReport *report);
int ZrPerfReport_WriteAotJson(const char *jsonPath,
                              const SZrPerfAotPhaseReport *report);

int ZrPerfReport_ComputeSummary(const SZrPerfRunSample *samples,
                                int count,
                                uint64_t bootstrapSeed,
                                size_t bootstrapResampleCount,
                                SZrPerfSummary *summary);

int ZrPerfReport_WriteJson(const char *jsonPath,
                           const char *caseName,
                           const char *workingDirectory,
                           const char *measurementScope,
                           const char *prepareScope,
                           int runtimeReused,
                           int compilerReused,
                           int jitStateReused,
                           char *const *command,
                           const SZrPerfMeasurementMetadata *metadata,
                           int warmup,
                           const SZrPerfRunSample *samples,
                           const SZrPerfSummary *summary,
                           int persistentMode,
                           const SZrPerfPersistentSessionInfo *persistentSession,
                           const char *checksumContract,
                           const char *expectedChecksum);

#endif
