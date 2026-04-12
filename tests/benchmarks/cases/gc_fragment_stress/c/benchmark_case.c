#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_gc_fragment_stress_run(int scale) {
    return zr_bench_run_gc_fragment_stress(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_gc_fragment_stress = {
        "gc_fragment_stress",
        "BENCH_GC_FRAGMENT_STRESS_PASS",
        zr_bench_case_gc_fragment_stress_run
};
