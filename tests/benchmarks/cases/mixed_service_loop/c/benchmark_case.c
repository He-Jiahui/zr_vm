#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_mixed_service_loop_run(int scale) {
    return zr_bench_run_mixed_service_loop(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_mixed_service_loop = {
        "mixed_service_loop",
        "BENCH_MIXED_SERVICE_LOOP_PASS",
        zr_bench_case_mixed_service_loop_run
};
