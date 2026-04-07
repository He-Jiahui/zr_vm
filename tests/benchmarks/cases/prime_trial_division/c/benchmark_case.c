#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_prime_trial_division_run(int scale) {
    return zr_bench_run_prime_trial_division(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_prime_trial_division = {
        "prime_trial_division",
        "BENCH_PRIME_TRIAL_DIVISION_PASS",
        zr_bench_case_prime_trial_division_run
};
