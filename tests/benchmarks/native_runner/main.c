#include <inttypes.h>
#include <stdio.h>
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

static void zr_bench_print_usage(const char *executable) {
    fprintf(stderr,
            "Usage:\n"
            "  %s --case <name> --tier <smoke|core|stress>\n",
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
            &zr_bench_case_descriptor_map_object_access
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

    scale = zr_bench_scale_from_tier(tier);
    if (scale == 0) {
        fprintf(stderr, "Unsupported tier: %s\n", tier);
        return 1;
    }

    printf("%s\n", descriptor->passBanner);
    printf("%" PRId64 "\n", descriptor->run(scale));
    return 0;
}
