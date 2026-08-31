#include "perf_report.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void zr_test_fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
}

static void zr_test_summary_rejects_too_many_samples(void) {
    SZrPerfRunSample samples[ZR_PERF_MAX_TOTAL_SAMPLES + 1U] = {{0}};
    SZrPerfSummary summary = {0};

    if (ZrPerfReport_ComputeSummary(samples,
                                    (int)(ZR_PERF_MAX_TOTAL_SAMPLES + 1U),
                                    UINT64_C(1),
                                    1U,
                                    &summary)) {
        zr_test_fail("summary accepted more than 20 total samples");
    }
}

static void zr_test_report_rejects_inconsistent_sample_counts(void) {
    SZrPerfRunSample samples[2] = {{0}};
    SZrPerfSummary summary = {0};
    SZrPerfMeasurementMetadata metadata = {0};
    char *command[] = {(char *)"fixture", NULL};
    const char *path = "inconsistent-report-should-not-exist.json";

    (void)remove(path);
    metadata.initialSampleCount = 1;
    metadata.sampleCount = 2;
    metadata.extraSampleCount = 0;
    metadata.repetitions = 1U;
    metadata.stability = "STABLE";
    if (ZrPerfReport_WriteJson(path,
                               "fixture", "", "process_end_to_end", "process_start",
                               0, 0, 0, command, &metadata, 0, samples, &summary,
                               0, NULL, NULL, NULL)) {
        zr_test_fail("report accepted inconsistent initial/extra/total sample counts");
    }
}

static void zr_test_report_detects_output_failure(void) {
#if defined(_WIN32)
    puts("perf report /dev/full check SKIP: unavailable on Windows");
#else
    SZrPerfRunSample sample = {1.0, 1.0, 0U, 0U, 0};
    SZrPerfSummary summary = {0};
    SZrPerfMeasurementMetadata metadata = {0};
    char *command[] = {(char *)"fixture", NULL};

    metadata.initialSampleCount = 1;
    metadata.sampleCount = 1;
    metadata.extraSampleCount = 0;
    metadata.repetitions = 1U;
    metadata.stability = "STABLE";
    metadata.bootstrapResampleCount = 1U;
    if (ZrPerfReport_WriteJson("/dev/full",
                               "fixture", "", "process_end_to_end", "process_start",
                               0, 0, 0, command, &metadata, 0, &sample, &summary,
                               0, NULL, NULL, NULL)) {
        zr_test_fail("report ignored a buffered output failure");
    }
#endif
}

int main(void) {
    zr_test_summary_rejects_too_many_samples();
    zr_test_report_rejects_inconsistent_sample_counts();
    zr_test_report_detects_output_failure();
    puts("perf report PASS");
    return 0;
}
