#include "perf_report.h"

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int zr_perf_report_compare_u64(const void *left, const void *right) {
    const uint64_t leftValue = *(const uint64_t *)left;
    const uint64_t rightValue = *(const uint64_t *)right;
    return leftValue < rightValue ? -1 : (leftValue > rightValue ? 1 : 0);
}

static double zr_perf_report_mean_u64(const uint64_t *values, int count) {
    double sum = 0.0;
    int index;
    if (values == NULL || count <= 0) {
        return 0.0;
    }
    for (index = 0; index < count; index++) {
        sum += (double)values[index];
    }
    return sum / (double)count;
}

static double zr_perf_report_median_u64(const uint64_t *values, int count) {
    uint64_t *sortedValues;
    double result;
    if (values == NULL || count <= 0) {
        return 0.0;
    }
    sortedValues = (uint64_t *)malloc((size_t)count * sizeof(*sortedValues));
    if (sortedValues == NULL) {
        return 0.0;
    }
    memcpy(sortedValues, values, (size_t)count * sizeof(*sortedValues));
    qsort(sortedValues, (size_t)count, sizeof(*sortedValues), zr_perf_report_compare_u64);
    result = (count % 2) == 0 ?
                     ((double)sortedValues[(count / 2) - 1] + (double)sortedValues[count / 2]) / 2.0 :
                     (double)sortedValues[count / 2];
    free(sortedValues);
    return result;
}

int ZrPerfReport_ComputeSummary(const SZrPerfRunSample *samples,
                                int count,
                                uint64_t bootstrapSeed,
                                size_t bootstrapResampleCount,
                                SZrPerfSummary *summary) {
    double *wallValues;
    uint64_t *peakValues;
    SZrPerfStatistics statistics;
    int index;
    if (samples == NULL || count <= 0 || count > (int)ZR_PERF_MAX_TOTAL_SAMPLES || summary == NULL ||
        (size_t)count > SIZE_MAX / sizeof(*wallValues) ||
        (size_t)count > SIZE_MAX / sizeof(*peakValues)) {
        return 0;
    }
    wallValues = (double *)malloc((size_t)count * sizeof(*wallValues));
    peakValues = (uint64_t *)malloc((size_t)count * sizeof(*peakValues));
    if (wallValues == NULL || peakValues == NULL) {
        free(wallValues);
        free(peakValues);
        memset(summary, 0, sizeof(*summary));
        return 0;
    }
    summary->minWallMs = samples[0].wallMs;
    summary->maxWallMs = samples[0].wallMs;
    summary->minPeakWorkingSetBytes = samples[0].peakWorkingSetBytes;
    summary->maxPeakWorkingSetBytes = samples[0].peakWorkingSetBytes;
    for (index = 0; index < count; index++) {
        wallValues[index] = samples[index].wallMs;
        peakValues[index] = samples[index].peakWorkingSetBytes;
        if (samples[index].wallMs < summary->minWallMs) summary->minWallMs = samples[index].wallMs;
        if (samples[index].wallMs > summary->maxWallMs) summary->maxWallMs = samples[index].wallMs;
        if (samples[index].peakWorkingSetBytes < summary->minPeakWorkingSetBytes) {
            summary->minPeakWorkingSetBytes = samples[index].peakWorkingSetBytes;
        }
        if (samples[index].peakWorkingSetBytes > summary->maxPeakWorkingSetBytes) {
            summary->maxPeakWorkingSetBytes = samples[index].peakWorkingSetBytes;
        }
    }
    if (!ZrPerfStatistics_Compute(wallValues, (size_t)count, &statistics) ||
        !ZrPerfStatistics_BootstrapMedian95(wallValues,
                                           (size_t)count,
                                           bootstrapSeed,
                                           bootstrapResampleCount,
                                           &summary->bootstrapMedian95)) {
        free(wallValues);
        free(peakValues);
        memset(summary, 0, sizeof(*summary));
        return 0;
    }
    summary->meanWallMs = statistics.mean;
    summary->medianWallMs = statistics.median;
    summary->stddevWallMs = statistics.sampleStddev;
    summary->madWallMs = statistics.mad;
    summary->coefficientOfVariation = statistics.coefficientOfVariation;
    summary->meanPeakWorkingSetBytes = zr_perf_report_mean_u64(peakValues, count);
    summary->medianPeakWorkingSetBytes = zr_perf_report_median_u64(peakValues, count);
    free(wallValues);
    free(peakValues);
    return 1;
}

static void zr_perf_report_json_escaped(FILE *file, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    fputc('"', file);
    while (cursor != NULL && *cursor != '\0') {
        switch (*cursor) {
            case '\\': fputs("\\\\", file); break;
            case '"': fputs("\\\"", file); break;
            case '\n': fputs("\\n", file); break;
            case '\r': fputs("\\r", file); break;
            case '\t': fputs("\\t", file); break;
            default:
                if (*cursor < 0x20) fprintf(file, "\\u%04x", (unsigned int)*cursor);
                else fputc((int)*cursor, file);
                break;
        }
        cursor++;
    }
    fputc('"', file);
}

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
                           const char *expectedChecksum) {
    FILE *file;
    int index;
    if (jsonPath == NULL || caseName == NULL || measurementScope == NULL || prepareScope == NULL ||
        command == NULL || metadata == NULL || samples == NULL || summary == NULL || metadata->sampleCount <= 0 ||
        metadata->sampleCount > (int)ZR_PERF_MAX_TOTAL_SAMPLES || metadata->initialSampleCount <= 0 ||
        metadata->initialSampleCount > (int)ZR_PERF_MAX_TOTAL_SAMPLES || metadata->extraSampleCount < 0 ||
        metadata->extraSampleCount > (int)ZR_PERF_MAX_TOTAL_SAMPLES - metadata->initialSampleCount ||
        metadata->sampleCount != metadata->initialSampleCount + metadata->extraSampleCount ||
        metadata->repetitions == 0U || metadata->stability == NULL ||
        (persistentMode && persistentSession == NULL)) {
        return 0;
    }
    if (persistentMode) {
        for (index = 0; index < metadata->sampleCount; index++) {
            if (samples[index].processId != persistentSession->processId) return 0;
        }
    }
    file = fopen(jsonPath, "wb");
    if (file == NULL) return 0;
    fputs("{\n  \"name\": ", file); zr_perf_report_json_escaped(file, caseName);
    fputs(",\n  \"working_directory\": ", file);
    zr_perf_report_json_escaped(file, workingDirectory != NULL ? workingDirectory : "");
    fprintf(file,
            ",\n  \"iterations\": %d,\n  \"sample_count\": %d,\n  \"extra_sample_count\": %d,"
            "\n  \"repetitions\": %" PRIu32 ",\n  \"warmup\": %d,\n  \"measurement_scope\": ",
            metadata->initialSampleCount,
            metadata->sampleCount,
            metadata->extraSampleCount,
            metadata->repetitions,
            warmup);
    zr_perf_report_json_escaped(file, measurementScope);
    fputs(",\n  \"prepare_scope\": ", file); zr_perf_report_json_escaped(file, prepareScope);
    fprintf(file, ",\n  \"runtime_reused\": %s,\n  \"compiler_reused\": %s,\n  \"jit_state_reused\": %s,\n",
            runtimeReused ? "true" : "false", compilerReused ? "true" : "false", jitStateReused ? "true" : "false");
    if (metadata->calibrationEnabled) {
        fprintf(file,
                "  \"calibration\": {\"enabled\": true, \"min_sample_ms\": %.3f, "
                "\"aggregate_wall_ms\": %.3f, \"repetitions\": %" PRIu32 "},\n",
                metadata->minimumSampleMs,
                metadata->calibrationAggregateWallMs,
                metadata->repetitions);
    } else {
        fputs("  \"calibration\": {\"enabled\": false, \"min_sample_ms\": null, "
              "\"aggregate_wall_ms\": null, \"repetitions\": 1},\n",
              file);
    }
    fprintf(file,
            "  \"stability\": \"%s\",\n  \"comparable\": %s,\n  \"gate_eligible\": %s,\n",
            metadata->stability,
            metadata->comparable ? "true" : "false",
            metadata->gateEligible ? "true" : "false");
    if (persistentMode) {
        fprintf(file, "  \"persistent_session\": {\"pid\": %" PRIu64 ", \"same_pid\": true, \"checksum_contract\": ", persistentSession->processId);
        zr_perf_report_json_escaped(file, checksumContract); fputs(", \"expected_checksum\": ", file);
        zr_perf_report_json_escaped(file, expectedChecksum);
        fprintf(file, ", \"peak_working_set_bytes\": %" PRIu64 ", \"exit_code\": %d},\n", persistentSession->peakWorkingSetBytes, persistentSession->exitCode);
    } else fputs("  \"persistent_session\": null,\n", file);
    fputs("  \"command\": [", file);
    for (index = 0; command[index] != NULL; index++) { if (index > 0) fputs(", ", file); zr_perf_report_json_escaped(file, command[index]); }
    fputs("],\n  \"runs\": [\n", file);
    for (index = 0; index < metadata->sampleCount; index++) {
        if (persistentMode) fprintf(file, "    {\"schema_version\": 3, \"index\": %d, \"repetitions\": %" PRIu32 ", \"aggregate_wall_ms\": %.3f, \"wall_ms\": %.3f, \"pid\": %" PRIu64 ", \"peak_working_set_bytes\": null}%s\n", index + 1, metadata->repetitions, samples[index].aggregateWallMs, samples[index].wallMs, samples[index].processId, (index + 1) == metadata->sampleCount ? "" : ",");
        else fprintf(file, "    {\"schema_version\": 3, \"index\": %d, \"repetitions\": %" PRIu32 ", \"aggregate_wall_ms\": %.3f, \"wall_ms\": %.3f, \"peak_working_set_bytes\": %" PRIu64 "}%s\n", index + 1, metadata->repetitions, samples[index].aggregateWallMs, samples[index].wallMs, samples[index].peakWorkingSetBytes, (index + 1) == metadata->sampleCount ? "" : ",");
    }
    fputs("  ],\n  \"summary\": {\n", file);
    fprintf(file, "    \"mean_wall_ms\": %.3f,\n    \"median_wall_ms\": %.3f,\n    \"min_wall_ms\": %.3f,\n    \"max_wall_ms\": %.3f,\n    \"stddev_wall_ms\": %.3f,\n    \"mad_wall_ms\": %.3f,\n    \"coefficient_of_variation\": %.9f,\n", summary->meanWallMs, summary->medianWallMs, summary->minWallMs, summary->maxWallMs, summary->stddevWallMs, summary->madWallMs, summary->coefficientOfVariation);
    fprintf(file,
            "    \"bootstrap\": {\"seed\": \"%" PRIu64 "\", \"statistic\": \"median\", "
            "\"resamples\": %zu, \"low\": %.3f, \"high\": %.3f},\n",
            metadata->bootstrapSeed,
            metadata->bootstrapResampleCount,
            summary->bootstrapMedian95.low,
            summary->bootstrapMedian95.high);
    if (persistentMode) fputs("    \"mean_peak_working_set_bytes\": null,\n    \"median_peak_working_set_bytes\": null,\n    \"min_peak_working_set_bytes\": null,\n    \"max_peak_working_set_bytes\": null\n", file);
    else fprintf(file, "    \"mean_peak_working_set_bytes\": %.0f,\n    \"median_peak_working_set_bytes\": %.0f,\n    \"min_peak_working_set_bytes\": %" PRIu64 ",\n    \"max_peak_working_set_bytes\": %" PRIu64 "\n", summary->meanPeakWorkingSetBytes, summary->medianPeakWorkingSetBytes, summary->minPeakWorkingSetBytes, summary->maxPeakWorkingSetBytes);
    fputs("  }\n}\n", file);
    if (ferror(file) || fflush(file) != 0) {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}
