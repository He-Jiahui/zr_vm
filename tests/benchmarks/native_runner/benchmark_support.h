#ifndef ZR_VM_TESTS_BENCHMARK_SUPPORT_H
#define ZR_VM_TESTS_BENCHMARK_SUPPORT_H

#include <stdint.h>

typedef int64_t ZrBenchInt;

#define ZR_BENCH_MOD 1000000007LL

int zr_bench_scale_from_tier(const char *tier);

ZrBenchInt zr_bench_run_numeric_loops(int scale);
ZrBenchInt zr_bench_run_dispatch_loops(int scale);
ZrBenchInt zr_bench_run_container_pipeline(int scale);
ZrBenchInt zr_bench_run_sort_array(int scale);
ZrBenchInt zr_bench_run_prime_trial_division(int scale);
ZrBenchInt zr_bench_run_matrix_add_2d(int scale);
ZrBenchInt zr_bench_run_string_build(int scale);
ZrBenchInt zr_bench_run_map_object_access(int scale);
ZrBenchInt zr_bench_run_fib_recursive(int scale);
ZrBenchInt zr_bench_run_call_chain_polymorphic(int scale);
ZrBenchInt zr_bench_run_object_field_hot(int scale);
ZrBenchInt zr_bench_run_array_index_dense(int scale);
ZrBenchInt zr_bench_run_branch_jump_dense(int scale);
ZrBenchInt zr_bench_run_mixed_service_loop(int scale);

#endif
