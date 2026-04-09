#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_branch_jump_dense_run(int scale) {
    return zr_bench_run_branch_jump_dense(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_branch_jump_dense = {
        "branch_jump_dense",
        "BENCH_BRANCH_JUMP_DENSE_PASS",
        zr_bench_case_branch_jump_dense_run
};
