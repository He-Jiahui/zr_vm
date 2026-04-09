#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test_support.h"

static int benchmark_registry_expect_file(const char *path, const char *label) {
    if (ZrTests_File_Exists(path)) {
        return 0;
    }

    printf("Missing %s: %s\n", label, path);
    return 1;
}

static int benchmark_registry_expect_case_layout(const char *caseName,
                                                 TZrBool requireExtendedRuntimes,
                                                 TZrBool requireJavaRuntime) {
    static const char *const requiredRelativeFiles[] = {
            "zr/src/main.zr",
            "python/main.py",
            "node/main.js",
            "c/benchmark_case.c"
    };
    static const char *const optionalRelativeFiles[] = {
            "qjs/main.js",
            "lua/main.lua",
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

    if (requireExtendedRuntimes) {
        for (index = 0; index < sizeof(optionalRelativeFiles) / sizeof(optionalRelativeFiles[0]); index++) {
            char path[ZR_TESTS_PATH_MAX];
            int written = snprintf(path,
                                   sizeof(path),
                                   "%s/../tests/benchmarks/cases/%s/%s",
                                   ZR_VM_TESTS_SOURCE_DIR,
                                   caseName,
                                   optionalRelativeFiles[index]);
            if (written <= 0 || (TZrSize) written >= sizeof(path)) {
                printf("Failed to format benchmark extended implementation path for %s -> %s\n",
                       caseName,
                       optionalRelativeFiles[index]);
                failures++;
                continue;
            }

            failures += benchmark_registry_expect_file(path, optionalRelativeFiles[index]);
        }
    }

    if (requireJavaRuntime) {
        char javaPath[ZR_TESTS_PATH_MAX];
        int written = snprintf(javaPath,
                               sizeof(javaPath),
                               "%s/../tests/benchmarks/cases/%s/java/benchmark_case.java",
                               ZR_VM_TESTS_SOURCE_DIR,
                               caseName);
        if (written <= 0 || (TZrSize) written >= sizeof(javaPath)) {
            printf("Failed to format benchmark Java implementation path for %s\n", caseName);
            failures++;
        } else {
            failures += benchmark_registry_expect_file(javaPath, "java/benchmark_case.java");
        }
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

static char *benchmark_registry_read_text_file(const char *path) {
    FILE *file;
    long length;
    size_t readLength;
    char *buffer;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)length + 1u);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    readLength = fread(buffer, 1u, (size_t)length, file);
    fclose(file);
    if (readLength != (size_t)length) {
        free(buffer);
        return NULL;
    }

    buffer[readLength] = '\0';
    return buffer;
}

static int benchmark_registry_expect_file_contains(const char *path, const char *needle, const char *label) {
    char *content;

    if (path == NULL || needle == NULL || label == NULL) {
        return 1;
    }

    content = benchmark_registry_read_text_file(path);
    if (content == NULL) {
        printf("Failed to read %s: %s\n", label, path);
        return 1;
    }

    if (strstr(content, needle) == NULL) {
        printf("Missing %s marker '%s' in %s\n", label, needle, path);
        free(content);
        return 1;
    }

    free(content);
    return 0;
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
            "map_object_access",
            "fib_recursive",
            "call_chain_polymorphic",
            "object_field_hot",
            "array_index_dense",
            "branch_jump_dense",
            "mixed_service_loop"
    };
    char registryPath[ZR_TESTS_PATH_MAX];
    char readmePath[ZR_TESTS_PATH_MAX];
    char nativeRunnerPath[ZR_TESTS_PATH_MAX];
    char rustRunnerManifestPath[ZR_TESTS_PATH_MAX];
    char rustRunnerMainPath[ZR_TESTS_PATH_MAX];
    char dotnetRunnerProjectPath[ZR_TESTS_PATH_MAX];
    char dotnetRunnerMainPath[ZR_TESTS_PATH_MAX];
    char javaRunnerMainPath[ZR_TESTS_PATH_MAX];
    char javaRunnerSupportPath[ZR_TESTS_PATH_MAX];
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
                  ZR_VM_TESTS_SOURCE_DIR) ||
        !snprintf(javaRunnerMainPath,
                  sizeof(javaRunnerMainPath),
                  "%s/../tests/benchmarks/java_runner/src/BenchmarkRunner.java",
                  ZR_VM_TESTS_SOURCE_DIR) ||
        !snprintf(javaRunnerSupportPath,
                  sizeof(javaRunnerSupportPath),
                  "%s/../tests/benchmarks/java_runner/src/BenchmarkSupport.java",
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
    failures += benchmark_registry_expect_file(javaRunnerMainPath, "Java benchmark runner");
    failures += benchmark_registry_expect_file(javaRunnerSupportPath, "Java benchmark support");
    failures += benchmark_registry_expect_file_contains(registryPath,
                                                        "ZR_VM_BENCHMARK_TIER_SCALE_profile",
                                                        "profile tier");
    failures += benchmark_registry_expect_file_contains(registryPath, "\"java\"", "Java implementation id");
    failures += benchmark_registry_expect_file_contains(registryPath, "\"lua\"", "Lua implementation id");
    failures += benchmark_registry_expect_file_contains(registryPath, "\"qjs\"", "QuickJS implementation id");
    failures += benchmark_registry_expect_file_contains(readmePath, "profile", "README profile tier");
    failures += benchmark_registry_expect_file_contains(readmePath, "Java", "README Java");
    failures += benchmark_registry_expect_file_contains(readmePath, "Lua", "README Lua");
    failures += benchmark_registry_expect_file_contains(readmePath, "QuickJS", "README QuickJS");
    failures += benchmark_registry_expect_file_contains(readmePath,
                                                        "instruction_report",
                                                        "README instruction report");
    failures += benchmark_registry_expect_file_contains(readmePath, "hotspot_report", "README hotspot report");
    failures += benchmark_registry_expect_file_contains(readmePath,
                                                        "comparison_report",
                                                        "README comparison report");

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
        failures += benchmark_registry_expect_case_layout(
                benchmarkCases[index],
                index < 8u ? ZR_TRUE : ZR_FALSE,
                ZR_TRUE);
    }

    if (failures != 0) {
        printf("Benchmark registry structure FAILED (%d issues)\n", failures);
        return 1;
    }

    printf("Benchmark registry structure PASS\n");
    return 0;
}
