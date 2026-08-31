#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "persistent_protocol.h"
#include "perf_process.h"
#include "perf_report.h"
#include "perf_statistics.h"

#define ZR_PERF_MAX_EXTRA_SAMPLES 10
#define ZR_PERF_MAXIMUM_CV 0.05
#define ZR_PERF_BOOTSTRAP_RESAMPLES 10000U
#define ZR_PERF_DEFAULT_BOOTSTRAP_SEED UINT64_C(10322096095657499217)
#define ZR_PERF_DEFAULT_PROCESS_TIMEOUT_MS 600000

static void zr_perf_print_usage(const char *executable) {
    fprintf(stderr,
            "Usage:\n"
            "  %s --name <case> --iterations <n> --warmup <n> --json-out <path> [--working-directory <dir>] "
            "--measurement-scope <scope> --prepare-scope <scope> --runtime-reused true|false "
            "--compiler-reused true|false --jit-state-reused true|false "
            "[--min-sample-ms <n>] [--max-extra-samples <0..10>] [--bootstrap-seed <uint64>] [--profile] "
            "[--process-timeout-ms <n>] "
            "[--persistent --checksum-contract <contract> --expected-checksum <decimal> "
            "--ready-timeout-ms <n> --request-timeout-ms <n> --stop-timeout-ms <n>] "
            "-- <command> [args...]\n",
            executable);
}

static int zr_perf_parse_u64(const char *text, uint64_t *outValue) {
    uint64_t parsed = 0U;
    const unsigned char *cursor = (const unsigned char *)text;

    if (cursor == NULL || *cursor == '\0' || outValue == NULL) {
        return 0;
    }
    while (*cursor >= '0' && *cursor <= '9') {
        const uint64_t digit = (uint64_t)(*cursor - '0');
        if (parsed > (UINT64_MAX - digit) / 10U) {
            return 0;
        }
        parsed = parsed * 10U + digit;
        cursor++;
    }
    if (*cursor != '\0') {
        return 0;
    }
    *outValue = parsed;
    return 1;
}

static int zr_perf_parse_bool(const char *text, int *outValue) {
    if (text == NULL || outValue == NULL) {
        return 0;
    }
    if (strcmp(text, "true") == 0) {
        *outValue = 1;
        return 1;
    }
    if (strcmp(text, "false") == 0) {
        *outValue = 0;
        return 1;
    }
    return 0;
}

static int zr_perf_parse_positive_int(const char *text, int *outValue) {
    long parsed;
    char *end = NULL;

    if (text == NULL || outValue == NULL || text[0] == '\0') {
        return 0;
    }

    parsed = strtol(text, &end, 10);
    if (end == text || end == NULL || *end != '\0' || parsed <= 0 || parsed > 1000000L) {
        return 0;
    }

    *outValue = (int)parsed;
    return 1;
}

static int zr_perf_parse_non_negative_int(const char *text, int *outValue) {
    long parsed;
    char *end = NULL;

    if (text == NULL || outValue == NULL || text[0] == '\0') {
        return 0;
    }

    parsed = strtol(text, &end, 10);
    if (end == text || end == NULL || *end != '\0' || parsed < 0 || parsed > 1000000L) {
        return 0;
    }

    *outValue = (int)parsed;
    return 1;
}

static int zr_perf_evaluate_stability(const SZrPerfRunSample *samples,
                                      int sampleCount,
                                      int initialSampleCount,
                                      int maxExtraSampleCount,
                                      EZrPerfStability *stability) {
    SZrPerfStatistics statistics;
    double *wallValues;
    int index;

    if (samples == NULL || sampleCount <= 0 || stability == NULL ||
        (size_t)sampleCount > SIZE_MAX / sizeof(*wallValues)) {
        return 0;
    }
    wallValues = (double *)malloc((size_t)sampleCount * sizeof(*wallValues));
    if (wallValues == NULL) {
        return 0;
    }
    for (index = 0; index < sampleCount; index++) {
        wallValues[index] = samples[index].wallMs;
    }
    if (!ZrPerfStatistics_Compute(wallValues, (size_t)sampleCount, &statistics)) {
        free(wallValues);
        return 0;
    }
    free(wallValues);
    *stability = ZrPerfStatistics_Classify(&statistics,
                                           (size_t)sampleCount,
                                           (size_t)initialSampleCount,
                                           (size_t)maxExtraSampleCount,
                                           ZR_PERF_MAXIMUM_CV);
    return *stability != ZR_PERF_STABILITY_INVALID;
}

int main(int argc, char **argv) {
    const char *caseName = NULL;
    const char *jsonPath = NULL;
    const char *workingDirectory = "";
    const char *measurementScope = NULL;
    const char *prepareScope = NULL;
    int runtimeReused = -1;
    int compilerReused = -1;
    int jitStateReused = -1;
    int iterations = 0;
    int warmup = 0;
    int commandIndex = -1;
    int persistentMode = 0;
    const char *checksumContract = NULL;
    const char *expectedChecksum = NULL;
    int readyTimeoutMs = 0;
    int requestTimeoutMs = 0;
    int stopTimeoutMs = 0;
    int minimumSampleMs = 0;
    int maxExtraSamples = 0;
    int profileMode = 0;
    int processTimeoutMs = ZR_PERF_DEFAULT_PROCESS_TIMEOUT_MS;
    int processTimeoutExplicit = 0;
    uint64_t bootstrapSeed = ZR_PERF_DEFAULT_BOOTSTRAP_SEED;
    uint32_t repetitions = 1U;
    double calibrationAggregateWallMs = 0.0;
    int sampleCount = 0;
    EZrPerfStability stability = ZR_PERF_STABILITY_INVALID;
    int index;
    SZrPerfRunSample *samples = NULL;
    SZrPerfPersistentSessionInfo persistentSession;
    SZrPerfSummary summary;
    SZrPerfMeasurementMetadata metadata;
    char errorBuffer[512];

    memset(&summary, 0, sizeof(summary));
    memset(&persistentSession, 0, sizeof(persistentSession));
    memset(&metadata, 0, sizeof(metadata));
    memset(errorBuffer, 0, sizeof(errorBuffer));

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--") == 0) {
            commandIndex = index + 1;
            break;
        }
        if (strcmp(argv[index], "--name") == 0 && index + 1 < argc) {
            caseName = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--iterations") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_positive_int(argv[++index], &iterations)) {
                fprintf(stderr, "Invalid --iterations value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--warmup") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_non_negative_int(argv[++index], &warmup)) {
                fprintf(stderr, "Invalid --warmup value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--json-out") == 0 && index + 1 < argc) {
            jsonPath = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--working-directory") == 0 && index + 1 < argc) {
            workingDirectory = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--measurement-scope") == 0 && index + 1 < argc) {
            measurementScope = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--prepare-scope") == 0 && index + 1 < argc) {
            prepareScope = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--runtime-reused") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_bool(argv[++index], &runtimeReused)) {
                fprintf(stderr, "Invalid --runtime-reused value; expected true or false.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--compiler-reused") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_bool(argv[++index], &compilerReused)) {
                fprintf(stderr, "Invalid --compiler-reused value; expected true or false.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--jit-state-reused") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_bool(argv[++index], &jitStateReused)) {
                fprintf(stderr, "Invalid --jit-state-reused value; expected true or false.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--persistent") == 0) {
            persistentMode = 1;
            continue;
        }
        if (strcmp(argv[index], "--min-sample-ms") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_positive_int(argv[++index], &minimumSampleMs)) {
                fprintf(stderr, "Invalid --min-sample-ms value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--max-extra-samples") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_non_negative_int(argv[++index], &maxExtraSamples) ||
                maxExtraSamples > ZR_PERF_MAX_EXTRA_SAMPLES) {
                fprintf(stderr, "Invalid --max-extra-samples value; expected 0..10.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--bootstrap-seed") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_u64(argv[++index], &bootstrapSeed)) {
                fprintf(stderr, "Invalid --bootstrap-seed value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--profile") == 0) {
            profileMode = 1;
            continue;
        }
        if (strcmp(argv[index], "--process-timeout-ms") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_positive_int(argv[++index], &processTimeoutMs)) {
                fprintf(stderr, "Invalid --process-timeout-ms value.\n");
                return 1;
            }
            processTimeoutExplicit = 1;
            continue;
        }
        if (strcmp(argv[index], "--checksum-contract") == 0 && index + 1 < argc) {
            checksumContract = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--expected-checksum") == 0 && index + 1 < argc) {
            expectedChecksum = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--ready-timeout-ms") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_positive_int(argv[++index], &readyTimeoutMs)) {
                fprintf(stderr, "Invalid --ready-timeout-ms value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--request-timeout-ms") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_positive_int(argv[++index], &requestTimeoutMs)) {
                fprintf(stderr, "Invalid --request-timeout-ms value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[index], "--stop-timeout-ms") == 0 && index + 1 < argc) {
            if (!zr_perf_parse_positive_int(argv[++index], &stopTimeoutMs)) {
                fprintf(stderr, "Invalid --stop-timeout-ms value.\n");
                return 1;
            }
            continue;
        }

        zr_perf_print_usage(argv[0]);
        fprintf(stderr, "Unknown or incomplete option: %s\n", argv[index]);
        return 1;
    }

    if (persistentMode && processTimeoutExplicit) {
        fprintf(stderr, "--process-timeout-ms is a process-only option.\n");
        return 1;
    }

    if (persistentMode &&
        (measurementScope == NULL || checksumContract == NULL || checksumContract[0] == '\0' || expectedChecksum == NULL ||
         readyTimeoutMs <= 0 || requestTimeoutMs <= 0 || stopTimeoutMs <= 0 ||
         strcmp(measurementScope, "persistent_runtime") != 0 || runtimeReused != 1)) {
        fprintf(stderr,
                "Persistent mode requires persistent_runtime scope, runtime reuse, contract, checksum, and deadlines.\n");
        return 1;
    }

    if (caseName == NULL || jsonPath == NULL || measurementScope == NULL || measurementScope[0] == '\0' ||
        prepareScope == NULL || prepareScope[0] == '\0' || runtimeReused < 0 || compilerReused < 0 ||
        jitStateReused < 0 || iterations <= 0 || warmup < 0 || commandIndex <= 0 || commandIndex >= argc) {
        zr_perf_print_usage(argv[0]);
        return 1;
    }

    if (!persistentMode &&
        (strcmp(measurementScope, "persistent_runtime") == 0 || runtimeReused || compilerReused || jitStateReused ||
         checksumContract != NULL || expectedChecksum != NULL || readyTimeoutMs != 0 || requestTimeoutMs != 0 ||
         stopTimeoutMs != 0)) {
        fprintf(stderr, "Process mode cannot claim persistent reuse or accept persistent-only options.\n");
        return 1;
    }

    if (profileMode && (iterations != 1 || maxExtraSamples != 0 || minimumSampleMs != 0)) {
        fprintf(stderr, "Profile mode requires exactly one sample, no calibration, and no extra samples.\n");
        return 1;
    }
    if (iterations > (int)ZR_PERF_MAX_TOTAL_SAMPLES ||
        maxExtraSamples > (int)ZR_PERF_MAX_TOTAL_SAMPLES - iterations) {
        fprintf(stderr, "Performance sample count must not exceed 20.\n");
        return 1;
    }

    samples = (SZrPerfRunSample *)calloc((size_t)(iterations + maxExtraSamples), sizeof(*samples));
    if (samples == NULL) {
        fprintf(stderr, "Failed to allocate performance samples.\n");
        return 1;
    }

    if (persistentMode) {
        SZrPerfPersistentOptions persistentOptions;
        SZrPerfPersistentSession session;
        int requestIndex = 1;
        memset(&session, 0, sizeof(session));
        memset(&persistentOptions, 0, sizeof(persistentOptions));
        persistentOptions.workingDirectory = workingDirectory;
        persistentOptions.command = &argv[commandIndex];
        persistentOptions.checksumContract = checksumContract;
        persistentOptions.expectedChecksum = expectedChecksum;
        persistentOptions.readyTimeoutMs = (uint32_t)readyTimeoutMs;
        persistentOptions.requestTimeoutMs = (uint32_t)requestTimeoutMs;
        persistentOptions.stopTimeoutMs = (uint32_t)stopTimeoutMs;
        if (!ZrPerfPersistentSession_Start(&session, &persistentOptions, errorBuffer, sizeof(errorBuffer))) {
            fprintf(stderr, "Persistent run failed for %s: %s\n", caseName,
                    errorBuffer[0] != '\0' ? errorBuffer : "unknown error");
            free(samples);
            return 1;
        }
        if (minimumSampleMs > 0) {
            while (1) {
                SZrPerfPersistentSample calibrationSample;
                if (!ZrPerfPersistentSession_Request(&session,
                                                     1,
                                                     requestIndex++,
                                                     repetitions,
                                                     &calibrationSample,
                                                     errorBuffer,
                                                     sizeof(errorBuffer))) {
                    fprintf(stderr, "Persistent calibration failed for %s: %s\n", caseName, errorBuffer);
                    ZrPerfPersistentSession_Abort(&session);
                    free(samples);
                    return 1;
                }
                calibrationAggregateWallMs = calibrationSample.wallMs;
                if (calibrationAggregateWallMs >= (double)minimumSampleMs) {
                    break;
                }
                if (repetitions > ZR_PERF_MAX_REPETITIONS / 2U) {
                    fprintf(stderr, "Persistent calibration exceeded maximum repetitions.\n");
                    ZrPerfPersistentSession_Abort(&session);
                    free(samples);
                    return 1;
                }
                repetitions *= 2U;
            }
        }
        for (index = 0; index < warmup; index++) {
            if (!ZrPerfPersistentSession_Request(&session,
                                                 1,
                                                 requestIndex++,
                                                 repetitions,
                                                 NULL,
                                                 errorBuffer,
                                                 sizeof(errorBuffer))) {
                fprintf(stderr, "Persistent warmup failed for %s: %s\n", caseName, errorBuffer);
                ZrPerfPersistentSession_Abort(&session);
                free(samples);
                return 1;
            }
        }
        while (sampleCount < iterations) {
            SZrPerfPersistentSample persistentSample;
            if (!ZrPerfPersistentSession_Request(&session,
                                                 0,
                                                 requestIndex++,
                                                 repetitions,
                                                 &persistentSample,
                                                 errorBuffer,
                                                 sizeof(errorBuffer))) {
                fprintf(stderr, "Persistent measured run failed for %s: %s\n", caseName, errorBuffer);
                ZrPerfPersistentSession_Abort(&session);
                free(samples);
                return 1;
            }
            samples[sampleCount].aggregateWallMs = persistentSample.wallMs;
            samples[sampleCount].wallMs = persistentSample.wallMs / (double)repetitions;
            samples[sampleCount].processId = persistentSample.processId;
            sampleCount++;
        }
        if (!profileMode && !zr_perf_evaluate_stability(samples,
                                                        sampleCount,
                                                        iterations,
                                                        maxExtraSamples,
                                                        &stability)) {
            fprintf(stderr, "Failed to evaluate persistent sample stability.\n");
            ZrPerfPersistentSession_Abort(&session);
            free(samples);
            return 1;
        }
        while (!profileMode && stability == ZR_PERF_STABILITY_NEEDS_MORE) {
            SZrPerfPersistentSample persistentSample;
            if (!ZrPerfPersistentSession_Request(&session,
                                                 0,
                                                 requestIndex++,
                                                 repetitions,
                                                 &persistentSample,
                                                 errorBuffer,
                                                 sizeof(errorBuffer))) {
                fprintf(stderr, "Persistent extra sample failed for %s: %s\n", caseName, errorBuffer);
                ZrPerfPersistentSession_Abort(&session);
                free(samples);
                return 1;
            }
            samples[sampleCount].aggregateWallMs = persistentSample.wallMs;
            samples[sampleCount].wallMs = persistentSample.wallMs / (double)repetitions;
            samples[sampleCount].processId = persistentSample.processId;
            sampleCount++;
            if (!zr_perf_evaluate_stability(samples,
                                            sampleCount,
                                            iterations,
                                            maxExtraSamples,
                                            &stability)) {
                fprintf(stderr, "Failed to evaluate persistent sample stability.\n");
                ZrPerfPersistentSession_Abort(&session);
                free(samples);
                return 1;
            }
        }
        if (!ZrPerfPersistentSession_Finish(&session,
                                            &persistentSession,
                                            errorBuffer,
                                            sizeof(errorBuffer))) {
            fprintf(stderr, "Persistent STOP failed for %s: %s\n", caseName, errorBuffer);
            free(samples);
            return 1;
        }
    } else {
        if (minimumSampleMs > 0) {
            while (1) {
                SZrPerfRunSample calibrationSample;
                if (!ZrPerfProcess_RunAggregate(workingDirectory,
                                           &argv[commandIndex],
                                           repetitions,
                                           (uint32_t)processTimeoutMs,
                                           &calibrationSample,
                                           errorBuffer,
                                           sizeof(errorBuffer))) {
                    fprintf(stderr, "Calibration run failed for %s: %s\n", caseName, errorBuffer);
                    free(samples);
                    return 1;
                }
                if (calibrationSample.exitCode != 0) {
                    fprintf(stderr, "Calibration run failed for %s with exit code %d.\n",
                            caseName,
                            calibrationSample.exitCode);
                    free(samples);
                    return 1;
                }
                calibrationAggregateWallMs = calibrationSample.aggregateWallMs;
                if (calibrationAggregateWallMs >= (double)minimumSampleMs) {
                    break;
                }
                if (repetitions > ZR_PERF_MAX_REPETITIONS / 2U) {
                    fprintf(stderr, "Process calibration exceeded maximum repetitions.\n");
                    free(samples);
                    return 1;
                }
                repetitions *= 2U;
            }
        }
        for (index = 0; index < warmup; index++) {
            SZrPerfRunSample warmupSample;
            if (!ZrPerfProcess_RunAggregate(workingDirectory, &argv[commandIndex], repetitions,
                                            (uint32_t)processTimeoutMs, &warmupSample,
                                            errorBuffer, sizeof(errorBuffer))) {
                fprintf(stderr, "Warmup run failed for %s: %s\n", caseName, errorBuffer[0] != '\0' ? errorBuffer : "unknown error");
                free(samples);
                return 1;
            }
            if (warmupSample.exitCode != 0) {
                fprintf(stderr, "Warmup run failed for %s with exit code %d.\n", caseName, warmupSample.exitCode);
                free(samples);
                return 1;
            }
        }

        while (sampleCount < iterations) {
            if (!ZrPerfProcess_RunAggregate(workingDirectory, &argv[commandIndex], repetitions,
                                            (uint32_t)processTimeoutMs, &samples[sampleCount],
                                            errorBuffer, sizeof(errorBuffer))) {
                fprintf(stderr, "Measured run failed for %s: %s\n", caseName, errorBuffer[0] != '\0' ? errorBuffer : "unknown error");
                free(samples);
                return 1;
            }
            if (samples[sampleCount].exitCode != 0) {
                fprintf(stderr, "Measured run %d failed for %s with exit code %d.\n",
                        sampleCount + 1,
                        caseName,
                        samples[sampleCount].exitCode);
                free(samples);
                return 1;
            }
            sampleCount++;
        }
        if (!profileMode && !zr_perf_evaluate_stability(samples,
                                                        sampleCount,
                                                        iterations,
                                                        maxExtraSamples,
                                                        &stability)) {
            fprintf(stderr, "Failed to evaluate process sample stability.\n");
            free(samples);
            return 1;
        }
        while (!profileMode && stability == ZR_PERF_STABILITY_NEEDS_MORE) {
            if (!ZrPerfProcess_RunAggregate(workingDirectory,
                                       &argv[commandIndex],
                                       repetitions,
                                       (uint32_t)processTimeoutMs,
                                       &samples[sampleCount],
                                       errorBuffer,
                                       sizeof(errorBuffer)) ||
                samples[sampleCount].exitCode != 0) {
                fprintf(stderr, "Extra measured run failed for %s: %s\n", caseName,
                        errorBuffer[0] != '\0' ? errorBuffer : "child exited nonzero");
                free(samples);
                return 1;
            }
            sampleCount++;
            if (!zr_perf_evaluate_stability(samples,
                                            sampleCount,
                                            iterations,
                                            maxExtraSamples,
                                            &stability)) {
                fprintf(stderr, "Failed to evaluate process sample stability.\n");
                free(samples);
                return 1;
            }
        }
    }

    if (profileMode) {
        stability = ZR_PERF_STABILITY_UNSTABLE;
    }
    if (!ZrPerfReport_ComputeSummary(samples,
                                     sampleCount,
                                     bootstrapSeed,
                                     ZR_PERF_BOOTSTRAP_RESAMPLES,
                                     &summary)) {
        fprintf(stderr, "Failed to compute performance statistics.\n");
        free(samples);
        return 1;
    }
    metadata.initialSampleCount = iterations;
    metadata.sampleCount = sampleCount;
    metadata.extraSampleCount = sampleCount - iterations;
    metadata.repetitions = repetitions;
    metadata.calibrationEnabled = minimumSampleMs > 0;
    metadata.minimumSampleMs = (double)minimumSampleMs;
    metadata.calibrationAggregateWallMs = calibrationAggregateWallMs;
    metadata.comparable = !profileMode;
    metadata.gateEligible = !profileMode && sampleCount >= (int)ZR_PERF_MIN_GATE_SAMPLES &&
                            stability == ZR_PERF_STABILITY_STABLE;
    metadata.stability = profileMode
                                 ? "NOT_COMPARABLE"
                                 : (stability == ZR_PERF_STABILITY_STABLE ? "STABLE" : "UNSTABLE");
    metadata.bootstrapSeed = bootstrapSeed;
    metadata.bootstrapResampleCount = ZR_PERF_BOOTSTRAP_RESAMPLES;
    if (!ZrPerfReport_WriteJson(jsonPath,
                                   caseName,
                                   workingDirectory,
                                   measurementScope,
                                   prepareScope,
                                   runtimeReused,
                                   compilerReused,
                                   jitStateReused,
                                   &argv[commandIndex],
                                   &metadata,
                                   warmup,
                                   samples,
                                   &summary,
                                   persistentMode,
                                   &persistentSession,
                                   checksumContract,
                                   expectedChecksum)) {
        fprintf(stderr, "Failed to write JSON report: %s\n", jsonPath);
        free(samples);
        return 1;
    }

    printf("PERF_SUMMARY case=%s iterations=%d warmup=%d mean_wall_ms=%.3f median_wall_ms=%.3f min_wall_ms=%.3f "
           "max_wall_ms=%.3f stddev_wall_ms=%.3f",
           caseName,
           sampleCount,
           warmup,
           summary.meanWallMs,
           summary.medianWallMs,
           summary.minWallMs,
           summary.maxWallMs,
           summary.stddevWallMs);
    if (persistentMode) {
        printf(" session_pid=%" PRIu64 " session_peak_bytes=%" PRIu64 " session_peak_mib=%.3f\n",
               persistentSession.processId,
               persistentSession.peakWorkingSetBytes,
               (double)persistentSession.peakWorkingSetBytes / (1024.0 * 1024.0));
    } else {
        printf(" mean_peak_bytes=%.0f median_peak_bytes=%.0f min_peak_bytes=%" PRIu64
               " max_peak_bytes=%" PRIu64 " mean_peak_mib=%.3f max_peak_mib=%.3f\n",
               summary.meanPeakWorkingSetBytes,
               summary.medianPeakWorkingSetBytes,
               summary.minPeakWorkingSetBytes,
               summary.maxPeakWorkingSetBytes,
               summary.meanPeakWorkingSetBytes / (1024.0 * 1024.0),
               (double)summary.maxPeakWorkingSetBytes / (1024.0 * 1024.0));
    }

    free(samples);
    return 0;
}
