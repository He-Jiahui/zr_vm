#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_matrix_add_2d_run(int scale) {
    return zr_bench_run_matrix_add_2d(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_matrix_add_2d = {
        "matrix_add_2d",
        "BENCH_MATRIX_ADD_2D_PASS",
        zr_bench_case_matrix_add_2d_run
};
