#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_sort_array_run(int scale) {
    return zr_bench_run_sort_array(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_sort_array = {
        "sort_array",
        "BENCH_SORT_ARRAY_PASS",
        zr_bench_case_sort_array_run
};
