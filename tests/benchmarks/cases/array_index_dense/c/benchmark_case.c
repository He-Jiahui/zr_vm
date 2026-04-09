#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_array_index_dense_run(int scale) {
    return zr_bench_run_array_index_dense(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_array_index_dense = {
        "array_index_dense",
        "BENCH_ARRAY_INDEX_DENSE_PASS",
        zr_bench_case_array_index_dense_run
};
