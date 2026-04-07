#include <stdio.h>
#include <string.h>

#include "test_support.h"

static int benchmark_registry_expect_file(const char *path, const char *label) {
    if (ZrTests_File_Exists(path)) {
        return 0;
    }

    printf("Missing %s: %s\n", label, path);
    return 1;
}

static int benchmark_registry_expect_case(const char *caseName) {
    char path[ZR_TESTS_PATH_MAX];

    if (!snprintf(path,
                  sizeof(path),
                  "%s/../tests/benchmarks/cases/%s",
                  ZR_VM_TESTS_SOURCE_DIR,
                  caseName)) {
        printf("Failed to format benchmark case path for %s\n", caseName);
        return 1;
    }

    if (!ZrTests_File_Exists(path)) {
        printf("Missing benchmark case directory marker: %s\n", path);
        return 1;
    }

    return 0;
}

static int benchmark_registry_expect_case_layout(const char *caseName) {
    static const char *const requiredRelativeFiles[] = {
            "zr/src/main.zr",
            "python/main.py",
            "node/main.js",
            "c/benchmark_case.c",
            "rust/mod.rs",
            "dotnet/benchmark_case.cs"
    };
    TZrSize index;
    int failures = 0;

    for (index = 0; index < sizeof(requiredRelativeFiles) / sizeof(requiredRelativeFiles[0]); index++) {
        char path[ZR_TESTS_PATH_MAX];
        int written = snprintf(path,
                               sizeof(path),
                               "%s/../tests/benchmarks/cases/%s/%s",
                               ZR_VM_TESTS_SOURCE_DIR,
                               caseName,
                               requiredRelativeFiles[index]);
        if (written <= 0 || (TZrSize) written >= sizeof(path)) {
            printf("Failed to format benchmark implementation path for %s -> %s\n",
                   caseName,
                   requiredRelativeFiles[index]);
            failures++;
            continue;
        }

        failures += benchmark_registry_expect_file(path, requiredRelativeFiles[index]);
    }

    {
        char projectPath[ZR_TESTS_PATH_MAX];
        int written = snprintf(projectPath,
                               sizeof(projectPath),
                               "%s/../tests/benchmarks/cases/%s/zr/benchmark_%s.zrp",
                               ZR_VM_TESTS_SOURCE_DIR,
                               caseName,
                               caseName);
        if (written <= 0 || (TZrSize) written >= sizeof(projectPath)) {
            printf("Failed to format benchmark project path for %s\n", caseName);
            failures++;
        } else {
            failures += benchmark_registry_expect_file(projectPath, "zr project file");
        }
    }

    return failures;
}

int main(void) {
    static const char *const benchmarkCases[] = {
            "numeric_loops",
            "dispatch_loops",
            "container_pipeline",
            "sort_array",
            "prime_trial_division",
            "matrix_add_2d",
            "string_build",
            "map_object_access"
    };
    char registryPath[ZR_TESTS_PATH_MAX];
    char readmePath[ZR_TESTS_PATH_MAX];
    char nativeRunnerPath[ZR_TESTS_PATH_MAX];
    char rustRunnerManifestPath[ZR_TESTS_PATH_MAX];
    char rustRunnerMainPath[ZR_TESTS_PATH_MAX];
    char dotnetRunnerProjectPath[ZR_TESTS_PATH_MAX];
    char dotnetRunnerMainPath[ZR_TESTS_PATH_MAX];
    int failures = 0;
    TZrSize index;

    printf("==========\n");
    printf("Benchmark Registry Structure Test\n");
    printf("==========\n");

    if (!snprintf(registryPath,
                  sizeof(registryPath),
                  "%s/../tests/benchmarks/registry.cmake",
                  ZR_VM_TESTS_SOURCE_DIR) ||
        !snprintf(readmePath,
                  sizeof(readmePath),
                  "%s/../tests/benchmarks/README.md",
                  ZR_VM_TESTS_SOURCE_DIR) ||
        !snprintf(nativeRunnerPath,
                  sizeof(nativeRunnerPath),
                  "%s/../tests/benchmarks/native_runner/main.c",
                  ZR_VM_TESTS_SOURCE_DIR) ||
        !snprintf(rustRunnerManifestPath,
                  sizeof(rustRunnerManifestPath),
                  "%s/../tests/benchmarks/rust_runner/Cargo.toml",
                  ZR_VM_TESTS_SOURCE_DIR) ||
        !snprintf(rustRunnerMainPath,
                  sizeof(rustRunnerMainPath),
                  "%s/../tests/benchmarks/rust_runner/src/main.rs",
                  ZR_VM_TESTS_SOURCE_DIR) ||
        !snprintf(dotnetRunnerProjectPath,
                  sizeof(dotnetRunnerProjectPath),
                  "%s/../tests/benchmarks/dotnet_runner/BenchmarkRunner.csproj",
                  ZR_VM_TESTS_SOURCE_DIR) ||
        !snprintf(dotnetRunnerMainPath,
                  sizeof(dotnetRunnerMainPath),
                  "%s/../tests/benchmarks/dotnet_runner/Program.cs",
                  ZR_VM_TESTS_SOURCE_DIR)) {
        printf("Failed to format benchmark registry paths\n");
        return 1;
    }

    failures += benchmark_registry_expect_file(registryPath, "benchmark registry");
    failures += benchmark_registry_expect_file(readmePath, "benchmark README");
    failures += benchmark_registry_expect_file(nativeRunnerPath, "native benchmark runner");
    failures += benchmark_registry_expect_file(rustRunnerManifestPath, "Rust benchmark runner manifest");
    failures += benchmark_registry_expect_file(rustRunnerMainPath, "Rust benchmark runner");
    failures += benchmark_registry_expect_file(dotnetRunnerProjectPath, ".NET benchmark runner project");
    failures += benchmark_registry_expect_file(dotnetRunnerMainPath, ".NET benchmark runner");

    for (index = 0; index < sizeof(benchmarkCases) / sizeof(benchmarkCases[0]); index++) {
        char casePath[ZR_TESTS_PATH_MAX];
        int written = snprintf(casePath,
                               sizeof(casePath),
                               "%s/../tests/benchmarks/cases/%s/.case-root",
                               ZR_VM_TESTS_SOURCE_DIR,
                               benchmarkCases[index]);
        if (written <= 0 || (TZrSize) written >= sizeof(casePath)) {
            printf("Failed to format benchmark case path for %s\n", benchmarkCases[index]);
            failures++;
            continue;
        }

        failures += benchmark_registry_expect_file(casePath, benchmarkCases[index]);
        failures += benchmark_registry_expect_case_layout(benchmarkCases[index]);
    }

    if (failures != 0) {
        printf("Benchmark registry structure FAILED (%d issues)\n", failures);
        return 1;
    }

    printf("Benchmark registry structure PASS\n");
    return 0;
}
