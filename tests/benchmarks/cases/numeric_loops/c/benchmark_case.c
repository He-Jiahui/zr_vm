#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_numeric_loops_run(int scale) {
    return zr_bench_run_numeric_loops(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_numeric_loops = {
        "numeric_loops",
        "BENCH_NUMERIC_LOOPS_PASS",
        zr_bench_case_numeric_loops_run
};
