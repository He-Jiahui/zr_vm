#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_map_object_access_run(int scale) {
    return zr_bench_run_map_object_access(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_map_object_access = {
        "map_object_access",
        "BENCH_MAP_OBJECT_ACCESS_PASS",
        zr_bench_case_map_object_access_run
};
