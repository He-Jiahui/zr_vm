#ifndef ZR_VM_TESTS_BENCHMARK_CASE_H
#define ZR_VM_TESTS_BENCHMARK_CASE_H

#include "benchmark_support.h"

/** @brief 将 case 名称、成功横幅和 C 算法绑定给原生基线入口。 */
typedef struct ZrBenchCaseDescriptor {
    const char *caseName;
    const char *passBanner;
    ZrBenchInt (*run)(int scale);
} ZrBenchCaseDescriptor;

#endif
