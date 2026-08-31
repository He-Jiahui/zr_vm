#ifndef ZR_VM_PERF_REPORT_H
#define ZR_VM_PERF_REPORT_H

#include <stdint.h>
#include <stdio.h>

#include "persistent_protocol.h"
#include "perf_statistics.h"

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
