#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_fib_recursive_run(int scale) {
    return zr_bench_run_fib_recursive(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_fib_recursive = {
        "fib_recursive",
        "BENCH_FIB_RECURSIVE_PASS",
        zr_bench_case_fib_recursive_run
};
