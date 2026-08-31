#ifndef ZR_VM_TESTS_PERFORMANCE_PERF_PROCESS_H
#define ZR_VM_TESTS_PERFORMANCE_PERF_PROCESS_H

#include <stddef.h>
#include <stdint.h>

#include "perf_report.h"

int ZrPerfProcess_RunAggregate(const char *workingDirectory,
                               char *const *command,
                               uint32_t repetitions,
                               uint32_t processTimeoutMs,
                               SZrPerfRunSample *sample,
                               char *errorBuffer,
                               size_t errorBufferSize);

#endif
