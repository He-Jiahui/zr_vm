#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_object_field_hot_run(int scale) {
    return zr_bench_run_object_field_hot(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_object_field_hot = {
        "object_field_hot",
        "BENCH_OBJECT_FIELD_HOT_PASS",
        zr_bench_case_object_field_hot_run
};
