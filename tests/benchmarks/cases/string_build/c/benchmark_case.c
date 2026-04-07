#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_string_build_run(int scale) {
    return zr_bench_run_string_build(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_string_build = {
        "string_build",
        "BENCH_STRING_BUILD_PASS",
        zr_bench_case_string_build_run
};
