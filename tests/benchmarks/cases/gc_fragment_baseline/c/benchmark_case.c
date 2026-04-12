#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_gc_fragment_baseline_run(int scale) {
    return zr_bench_run_gc_fragment_baseline(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_gc_fragment_baseline = {
        "gc_fragment_baseline",
        "BENCH_GC_FRAGMENT_BASELINE_PASS",
        zr_bench_case_gc_fragment_baseline_run
};
