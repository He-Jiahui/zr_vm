#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_container_pipeline_run(int scale) {
    return zr_bench_run_container_pipeline(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_container_pipeline = {
        "container_pipeline",
        "BENCH_CONTAINER_PIPELINE_PASS",
        zr_bench_case_container_pipeline_run
};
