#ifndef ZR_VM_TESTS_BENCHMARK_CASE_H
#define ZR_VM_TESTS_BENCHMARK_CASE_H

#include "benchmark_support.h"

typedef struct ZrBenchCaseDescriptor {
    const char *caseName;
    const char *passBanner;
    ZrBenchInt (*run)(int scale);
} ZrBenchCaseDescriptor;

#endif
