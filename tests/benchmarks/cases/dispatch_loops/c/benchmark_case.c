#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_dispatch_loops_run(int scale) {
    return zr_bench_run_dispatch_loops(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_dispatch_loops = {
        "dispatch_loops",
        "BENCH_DISPATCH_LOOPS_PASS",
        zr_bench_case_dispatch_loops_run
};
