#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "benchmark_case.h"

extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_numeric_loops;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_dispatch_loops;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_container_pipeline;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_sort_array;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_prime_trial_division;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_matrix_add_2d;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_string_build;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_map_object_access;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_fib_recursive;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_call_chain_polymorphic;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_object_field_hot;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_array_index_dense;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_branch_jump_dense;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_mixed_service_loop;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_gc_fragment_baseline;
extern const ZrBenchCaseDescriptor zr_bench_case_descriptor_gc_fragment_stress;

static void zr_bench_print_usage(const char *executable) {
    fprintf(stderr,
            "Usage:\n"
            "  %s --case <name> --tier <smoke|core|stress|profile> [--scale <positive-int>]\n",
            executable);
}

static const ZrBenchCaseDescriptor *zr_bench_find_case(const char *caseName) {
    static const ZrBenchCaseDescriptor *const cases[] = {
            &zr_bench_case_descriptor_numeric_loops,
            &zr_bench_case_descriptor_dispatch_loops,
            &zr_bench_case_descriptor_container_pipeline,
            &zr_bench_case_descriptor_sort_array,
            &zr_bench_case_descriptor_prime_trial_division,
            &zr_bench_case_descriptor_matrix_add_2d,
            &zr_bench_case_descriptor_string_build,
            &zr_bench_case_descriptor_map_object_access,
            &zr_bench_case_descriptor_fib_recursive,
            &zr_bench_case_descriptor_call_chain_polymorphic,
            &zr_bench_case_descriptor_object_field_hot,
            &zr_bench_case_descriptor_array_index_dense,
            &zr_bench_case_descriptor_branch_jump_dense,
            &zr_bench_case_descriptor_mixed_service_loop,
            &zr_bench_case_descriptor_gc_fragment_baseline,
            &zr_bench_case_descriptor_gc_fragment_stress
    };
    int index;

    for (index = 0; index < (int)(sizeof(cases) / sizeof(cases[0])); index++) {
        if (strcmp(cases[index]->caseName, caseName) == 0) {
            return cases[index];
        }
    }

    return NULL;
}

int main(int argc, char **argv) {
    const char *caseName = NULL;
    const char *tier = NULL;
    const ZrBenchCaseDescriptor *descriptor = NULL;
    int scale = 0;
    int explicitScale = 0;
    int index;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--case") == 0 && index + 1 < argc) {
            caseName = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--tier") == 0 && index + 1 < argc) {
            tier = argv[++index];
            continue;
        }
        if (strcmp(argv[index], "--scale") == 0 && index + 1 < argc) {
            explicitScale = atoi(argv[++index]);
            continue;
        }

        zr_bench_print_usage(argv[0]);
        fprintf(stderr, "Unknown or incomplete option: %s\n", argv[index]);
        return 1;
    }

    if (caseName == NULL || tier == NULL) {
        zr_bench_print_usage(argv[0]);
        return 1;
    }

    descriptor = zr_bench_find_case(caseName);
    if (descriptor == NULL) {
        fprintf(stderr, "Unknown benchmark case: %s\n", caseName);
        return 1;
    }

    if (explicitScale > 0) {
        scale = explicitScale;
    } else {
        scale = zr_bench_scale_from_tier(tier);
    }
    if (scale == 0) {
        fprintf(stderr, "Unsupported tier or scale\n");
        return 1;
    }

    printf("%s\n", descriptor->passBanner);
    printf("%" PRId64 "\n", descriptor->run(scale));
    return 0;
}
