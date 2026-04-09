#include "benchmark_case.h"
#include "benchmark_support.h"

static ZrBenchInt zr_bench_case_call_chain_polymorphic_run(int scale) {
    return zr_bench_run_call_chain_polymorphic(scale);
}

const ZrBenchCaseDescriptor zr_bench_case_descriptor_call_chain_polymorphic = {
        "call_chain_polymorphic",
        "BENCH_CALL_CHAIN_POLYMORPHIC_PASS",
        zr_bench_case_call_chain_polymorphic_run
};
