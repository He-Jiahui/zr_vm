#if !defined(_WIN32)
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_UNIX)
    #include <dlfcn.h>
    #include <time.h>
#endif

#include "harness/path_support.h"
#include "harness/aot_c_link_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

#ifndef ZR_VM_TESTS_C_COMPILER
    #define ZR_VM_TESTS_C_COMPILER "cc"
#endif

#ifndef ZR_VM_TESTS_REPO_ROOT
    #define ZR_VM_TESTS_REPO_ROOT "."
#endif

#ifndef ZR_VM_TESTS_BUILD_LIB_DIR
    #define ZR_VM_TESTS_BUILD_LIB_DIR "lib"
#endif

static const char *const CZrAotTypedLoopSource =
        "fn sum_to(limit: int): int {\n"
        "    var index: int = 0;\n"
        "    var sum: int = 0;\n"
        "    while (index < limit) {\n"
        "        sum = sum + index;\n"
        "        index = index + 1;\n"
        "    }\n"
        "    return sum;\n"
        "}\n"
        "return sum_to(4096);";

static const char *const CZrAotGeneralLoopSource =
        "var index: int = 0;\n"
        "var sum: int = 0;\n"
        "while (index < 4096) {\n"
        "    sum = sum + index;\n"
        "    index = index + 1;\n"
        "}\n"
        "return sum;";

#if defined(ZR_PLATFORM_UNIX)
typedef TZrInt64 (*TZrAotValueConstructionProbe)(void);
typedef TZrInt64 (*TZrAotTypedLoopProbe)(TZrInt64 limit);

#define ZR_AOT_SCALAR_PERF_AOT_ITERATIONS 2000000u
#define ZR_AOT_SCALAR_PERF_INTERPRETER_ITERATIONS 4096u
#define ZR_AOT_SCALAR_PERF_SAMPLE_COUNT 3u
#define ZR_AOT_SCALAR_PERF_EXPECTED_RESULT ((TZrInt64)42)
#define ZR_AOT_SCALAR_PERF_MIN_SPEEDUP 1.0
#define ZR_AOT_SCALAR_PERF_TARGET_SPEEDUP 3.0
#define ZR_AOT_LOOP_LIMIT ((TZrInt64)4096)
#define ZR_AOT_LOOP_EXPECTED_RESULT ((TZrInt64)8386560)
#define ZR_AOT_LOOP_PERF_AOT_ITERATIONS 10000u
#define ZR_AOT_LOOP_PERF_INTERPRETER_ITERATIONS 16u
#define ZR_AOT_LOOP_PERF_GENERAL_AOT_ITERATIONS 64u
#define ZR_AOT_LOOP_PERF_SAMPLE_COUNT 3u
#define ZR_AOT_LOOP_PERF_MIN_SPEEDUP 1.0
#define ZR_AOT_LOOP_PERF_TARGET_SPEEDUP 3.0
#define ZR_AOT_LOOP_PERF_MIN_GENERAL_AOT_SPEEDUP 1.1

static double monotonic_time_ns(void) {
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        return -1.0;
    }
    return (double)value.tv_sec * 1000000000.0 + (double)value.tv_nsec;
}

static double benchmark_aot_probe_ns_per_call(TZrAotValueConstructionProbe probe,
                                               TZrUInt32 iterations,
                                               TZrInt64 *outChecksum) {
    double startNs;
    double endNs;
    TZrInt64 checksum = 0;
    TZrUInt32 index;

    if (probe == ZR_NULL || iterations == 0u || outChecksum == ZR_NULL) {
        return -1.0;
    }

    startNs = monotonic_time_ns();
    if (startNs < 0.0) {
        return -1.0;
    }
    for (index = 0u; index < iterations; index++) {
        TZrInt64 result = probe();
        if (result != ZR_AOT_SCALAR_PERF_EXPECTED_RESULT) {
            return -1.0;
        }
        checksum += result;
    }
    endNs = monotonic_time_ns();
    if (endNs < startNs) {
        return -1.0;
    }

    *outChecksum = checksum;
    return (endNs - startNs) / (double)iterations;
}

static double benchmark_interpreter_ns_per_call(SZrState *state,
                                                 SZrFunction *function,
                                                 TZrUInt32 iterations,
                                                 TZrInt64 *outChecksum) {
    double startNs;
    double endNs;
    TZrInt64 checksum = 0;
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || iterations == 0u || outChecksum == ZR_NULL) {
        return -1.0;
    }

    startNs = monotonic_time_ns();
    if (startNs < 0.0) {
        return -1.0;
    }
    for (index = 0u; index < iterations; index++) {
        TZrInt64 result;
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result) ||
            result != ZR_AOT_SCALAR_PERF_EXPECTED_RESULT) {
            return -1.0;
        }
        checksum += result;
    }
    endNs = monotonic_time_ns();
    if (endNs < startNs) {
        return -1.0;
    }

    *outChecksum = checksum;
    return (endNs - startNs) / (double)iterations;
}

static double benchmark_typed_loop_probe_ns_per_call(TZrAotTypedLoopProbe probe,
                                                       TZrUInt32 iterations,
                                                       TZrInt64 *outChecksum) {
    double startNs;
    double endNs;
    TZrInt64 checksum = 0;
    TZrUInt32 index;

    if (probe == ZR_NULL || iterations == 0u || outChecksum == ZR_NULL) {
        return -1.0;
    }

    startNs = monotonic_time_ns();
    if (startNs < 0.0) {
        return -1.0;
    }
    for (index = 0u; index < iterations; index++) {
        TZrInt64 result = probe(ZR_AOT_LOOP_LIMIT);
        if (result != ZR_AOT_LOOP_EXPECTED_RESULT) {
            return -1.0;
        }
        checksum += result;
    }
    endNs = monotonic_time_ns();
    if (endNs < startNs) {
        return -1.0;
    }

    *outChecksum = checksum;
    return (endNs - startNs) / (double)iterations;
}

static double benchmark_loop_interpreter_ns_per_call(SZrState *state,
                                                       SZrFunction *function,
                                                       TZrUInt32 iterations,
                                                       TZrInt64 *outChecksum) {
    double startNs;
    double endNs;
    TZrInt64 checksum = 0;
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || iterations == 0u || outChecksum == ZR_NULL) {
        return -1.0;
    }

    startNs = monotonic_time_ns();
    if (startNs < 0.0) {
        return -1.0;
    }
    for (index = 0u; index < iterations; index++) {
        TZrInt64 result;
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result) ||
            result != ZR_AOT_LOOP_EXPECTED_RESULT) {
            return -1.0;
        }
        checksum += result;
    }
    endNs = monotonic_time_ns();
    if (endNs < startNs) {
        return -1.0;
    }

    *outChecksum = checksum;
    return (endNs - startNs) / (double)iterations;
}

static double benchmark_general_aot_loop_ns_per_call(SZrState *state,
                                                       TZrUInt32 iterations,
                                                       TZrInt64 *outChecksum) {
    double startNs;
    double endNs;
    TZrInt64 checksum = 0;
    TZrUInt32 index;

    if (state == ZR_NULL || iterations == 0u || outChecksum == ZR_NULL) {
        return -1.0;
    }

    startNs = monotonic_time_ns();
    if (startNs < 0.0) {
        return -1.0;
    }
    for (index = 0u; index < iterations; index++) {
        SZrTypeValue result = {0};
        if (!ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result) ||
            !ZR_VALUE_IS_TYPE_INT(result.type) ||
            result.value.nativeObject.nativeInt64 != ZR_AOT_LOOP_EXPECTED_RESULT) {
            return -1.0;
        }
        checksum += result.value.nativeObject.nativeInt64;
    }
    endNs = monotonic_time_ns();
    if (endNs < startNs) {
        return -1.0;
    }

    *outChecksum = checksum;
    return (endNs - startNs) / (double)iterations;
}

static double min_positive_sample(const double *samples, TZrUInt32 count) {
    double result = -1.0;
    TZrUInt32 index;

    if (samples == ZR_NULL) {
        return -1.0;
    }
    for (index = 0u; index < count; index++) {
        if (samples[index] > 0.0 && (result < 0.0 || samples[index] < result)) {
            result = samples[index];
        }
    }
    return result;
}

static int run_command_expect_success(const char *command) {
    int result;

    TEST_ASSERT_NOT_NULL(command);
    result = system(command);
    if (result != 0) {
        printf("Command failed with status %d:\n%s\n", result, command);
    }
    return result;
}

static void *load_symbol(void *library, const char *symbolName) {
    void *symbol;

    dlerror();
    symbol = dlsym(library, symbolName);
    if (symbol == NULL) {
        const char *error = dlerror();
        printf("dlsym(%s) failed: %s\n", symbolName, error != NULL ? error : "<unknown>");
    }
    return symbol;
}
#endif

static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceNameText) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static char *read_text_file_owned_or_fail(const TZrChar *path) {
    TZrBytePtr bytes = ZR_NULL;
    TZrSize byteLength = 0u;
    char *text;

    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &bytes, &byteLength));
    text = (char *)malloc((size_t)byteLength + 1u);
    TEST_ASSERT_NOT_NULL(text);
    if (byteLength > 0u) {
        memcpy(text, bytes, (size_t)byteLength);
    }
    text[byteLength] = '\0';
    free(bytes);
    return text;
}

static void append_text_file_or_fail(const TZrChar *path, const char *text) {
    FILE *file;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(text);
    file = fopen(path, "ab");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1u, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static void write_text_file_or_fail(const TZrChar *path, const char *text) {
    FILE *file;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(text);
    file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1u, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static void hash_file_or_fail(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, bufferSize);

    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    while ((readSize = fread(chunk, 1u, sizeof(chunk), file)) > 0u) {
        TZrSize index;

        for (index = 0u; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
}

static void test_full_aot_typed_i64_thunk_constructs_no_type_values(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C value-construction guardrail executes the Unix shared-library path");
#else
    static const char *const source =
            "fn add(left: int, right: int): int {\n"
            "    return left + right;\n"
            "}\n"
            "return add(19, 23);";
    static const char *const probeSource =
            "\nZR_VM_AOT_EXPORT TZrInt64 ZrVm_Test_RunTypedValueConstructionProbe(void) {\n"
            "    return zr_aot_typed_i64_fn_1((TZrInt64)19, (TZrInt64)23);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrProfileRuntime profileRuntime;
    SZrTypeValue positiveControl = {0};
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0u;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];
    void *library;
    void *symbol;
    TZrAotValueConstructionProbe probe = ZR_NULL;
    TZrInt64 result;
    TZrInt64 aotChecksum = 0;
    TZrInt64 interpreterChecksum = 0;
    double aotSamples[ZR_AOT_SCALAR_PERF_SAMPLE_COUNT];
    double interpreterSamples[ZR_AOT_SCALAR_PERF_SAMPLE_COUNT];
    double aotNsPerCall;
    double interpreterNsPerCall;
    double speedup;
    TZrUInt32 sampleIndex;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "value_construction_guardrail.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_construction_guardrail",
                                                       "typed_i64_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_construction_guardrail",
                                                       "typed_i64_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_construction_guardrail",
                                                       "typed_i64_project/bin/aot_c/lib",
                                                       "zrvm_aot_value_construction_guardrail",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    aotOptions.requireFullAot = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0, TZrInt64 zr_aot_arg1)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "return (TZrInt64)(zr_aot_arg0 + zr_aot_arg1);"));
    free(generatedCText);
    append_text_file_or_fail(generatedCPath, probeSource);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -O2 -g -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    library = dlopen(sharedLibraryPath, RTLD_NOW | RTLD_LOCAL);
    if (library == NULL) {
        printf("dlopen(%s) failed: %s\n", sharedLibraryPath, dlerror());
    }
    TEST_ASSERT_NOT_NULL(library);
    symbol = load_symbol(library, "ZrVm_Test_RunTypedValueConstructionProbe");
    TEST_ASSERT_NOT_NULL(symbol);
    memcpy(&probe, &symbol, sizeof(probe));

    memset(&profileRuntime, 0, sizeof(profileRuntime));
    profileRuntime.recordHelpers = ZR_TRUE;
    state->global->profileRuntime = &profileRuntime;
    ZrCore_Profile_SetCurrentState(state);

    result = probe();
    TEST_ASSERT_EQUAL_INT64(42, result);
    TEST_ASSERT_EQUAL_UINT64(0u,
                             profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_CONSTRUCT]);
    TEST_ASSERT_EQUAL_UINT64(0u,
                             profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);
    TEST_ASSERT_EQUAL_UINT64(0u,
                             profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_RESET_NULL]);

    ZrCore_Value_InitAsInt(state, &positiveControl, result);
    TEST_ASSERT_EQUAL_UINT64(1u,
                             profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_CONSTRUCT]);

    state->global->profileRuntime = ZR_NULL;
    ZrCore_Profile_SetCurrentState(ZR_NULL);

    for (sampleIndex = 0u; sampleIndex < 32u; sampleIndex++) {
        TEST_ASSERT_EQUAL_INT64(ZR_AOT_SCALAR_PERF_EXPECTED_RESULT, probe());
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(ZR_AOT_SCALAR_PERF_EXPECTED_RESULT, result);
    }
    for (sampleIndex = 0u; sampleIndex < ZR_AOT_SCALAR_PERF_SAMPLE_COUNT; sampleIndex++) {
        aotSamples[sampleIndex] = benchmark_aot_probe_ns_per_call(probe,
                                                                 ZR_AOT_SCALAR_PERF_AOT_ITERATIONS,
                                                                 &aotChecksum);
        interpreterSamples[sampleIndex] = benchmark_interpreter_ns_per_call(
                state,
                function,
                ZR_AOT_SCALAR_PERF_INTERPRETER_ITERATIONS,
                &interpreterChecksum);
        TEST_ASSERT_TRUE(aotSamples[sampleIndex] > 0.0);
        TEST_ASSERT_TRUE(interpreterSamples[sampleIndex] > 0.0);
        TEST_ASSERT_EQUAL_INT64(ZR_AOT_SCALAR_PERF_EXPECTED_RESULT *
                                        (TZrInt64)ZR_AOT_SCALAR_PERF_AOT_ITERATIONS,
                                aotChecksum);
        TEST_ASSERT_EQUAL_INT64(ZR_AOT_SCALAR_PERF_EXPECTED_RESULT *
                                        (TZrInt64)ZR_AOT_SCALAR_PERF_INTERPRETER_ITERATIONS,
                                interpreterChecksum);
    }
    aotNsPerCall = min_positive_sample(aotSamples, ZR_AOT_SCALAR_PERF_SAMPLE_COUNT);
    interpreterNsPerCall = min_positive_sample(interpreterSamples, ZR_AOT_SCALAR_PERF_SAMPLE_COUNT);
    TEST_ASSERT_TRUE(aotNsPerCall > 0.0);
    TEST_ASSERT_TRUE(interpreterNsPerCall > 0.0);
    speedup = interpreterNsPerCall / aotNsPerCall;
    printf("AOT typed scalar performance: aot=%.3f ns/call interpreter=%.3f ns/call speedup=%.2fx target_met=%s\n",
           aotNsPerCall,
           interpreterNsPerCall,
           speedup,
           speedup >= ZR_AOT_SCALAR_PERF_TARGET_SPEEDUP ? "yes" : "no");
    TEST_ASSERT_TRUE_MESSAGE(speedup >= ZR_AOT_SCALAR_PERF_MIN_SPEEDUP,
                             "typed scalar AOT must not be slower than the interpreter");

    TEST_ASSERT_EQUAL_INT(0, dlclose(library));
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_full_aot_typed_i64_counting_loop_emits_state_free_thunk(void) {
    static const TZrByte embeddedBlob[] = {0x7a};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions aotOptions;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, CZrAotTypedLoopSource, "typed_i64_counting_loop.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_construction_guardrail",
                                                       "typed_i64_loop_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.sourceHash = "typed-i64-counting-loop";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    aotOptions.inputHash = "typed-i64-counting-loop";
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = sizeof(embeddedBlob);
    aotOptions.requireExecutableLowering = ZR_TRUE;
    aotOptions.requireFullAot = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state,
                                                             function,
                                                             generatedCPath,
                                                             &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "static TZrInt64 zr_aot_typed_i64_fn_1(TZrInt64 zr_aot_arg0) {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "    TZrInt64 zr_aot_loop_index = 0;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "    TZrInt64 zr_aot_loop_accumulator = 0;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "    while (zr_aot_loop_index < zr_aot_arg0) {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "        zr_aot_loop_accumulator = (TZrInt64)(zr_aot_loop_accumulator + zr_aot_loop_index);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "        zr_aot_loop_index = (TZrInt64)(zr_aot_loop_index + 1);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "    return zr_aot_loop_accumulator;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_static_i64_one_arg_direct_call_full_aot */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_typed_i64_fn_1(zr_aot_s3);"));
    free(generatedCText);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_full_aot_typed_i64_counting_loop_runtime_and_performance_gate(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C typed-loop performance guardrail executes the Unix shared-library path");
#else
    static const char *const typedProbeSource =
            "\nZR_VM_AOT_EXPORT TZrInt64 ZrVm_Test_RunTypedLoopProbe(TZrInt64 limit) {\n"
            "    return zr_aot_typed_i64_fn_1(limit);\n"
            "}\n";
    static const char *const projectJson =
            "{"
            "\"name\":\"aot-loop-general-baseline\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    static const TZrByte typedEmbeddedBlob[] = {0x7a};
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrState *generalAotState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunction *generalAotFunction;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrProfileRuntime profileRuntime;
    SZrProfileRuntime generalAotProfileRuntime;
    SZrTypeValue generalAotResult = {0};
    TZrBytePtr generalAotEmbeddedBlob = ZR_NULL;
    TZrSize generalAotEmbeddedBlobLength = 0u;
    TZrChar generalAotHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar typedGeneratedCPath[ZR_TESTS_PATH_MAX];
    TZrChar typedSharedLibraryPath[ZR_TESTS_PATH_MAX];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generalAotGeneratedCPath[ZR_TESTS_PATH_MAX];
    TZrChar generalAotSharedLibraryPath[ZR_TESTS_PATH_MAX];
    char command[4096];
    char *generatedCText;
    void *library;
    void *symbol;
    TZrAotTypedLoopProbe typedProbe = ZR_NULL;
    TZrInt64 result;
    TZrInt64 typedChecksum = 0;
    TZrInt64 interpreterChecksum = 0;
    TZrInt64 generalAotChecksum = 0;
    double typedSamples[ZR_AOT_LOOP_PERF_SAMPLE_COUNT];
    double interpreterSamples[ZR_AOT_LOOP_PERF_SAMPLE_COUNT];
    double generalAotSamples[ZR_AOT_LOOP_PERF_SAMPLE_COUNT];
    double typedNsPerCall;
    double interpreterNsPerCall;
    double generalAotNsPerCall;
    double interpreterSpeedup;
    double generalAotSpeedup;
    TZrUInt32 sampleIndex;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(generalAotState);
    function = compile_source(state, CZrAotTypedLoopSource, "typed_i64_counting_loop_runtime.zr");
    generalAotFunction = compile_source(generalAotState,
                                        CZrAotGeneralLoopSource,
                                        "general_aot_counting_loop_runtime.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(generalAotFunction);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_construction_guardrail",
                                                       "typed_i64_loop_runtime/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       typedGeneratedCPath,
                                                       sizeof(typedGeneratedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_construction_guardrail",
                                                       "typed_i64_loop_runtime/bin/aot_c/lib",
                                                       "zrvm_aot_typed_loop_guardrail",
                                                       ".so",
                                                       typedSharedLibraryPath,
                                                       sizeof(typedSharedLibraryPath)));

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.sourceHash = "typed-i64-counting-loop-runtime";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    aotOptions.inputHash = "typed-i64-counting-loop-runtime";
    aotOptions.embeddedModuleBlob = typedEmbeddedBlob;
    aotOptions.embeddedModuleBlobLength = sizeof(typedEmbeddedBlob);
    aotOptions.requireExecutableLowering = ZR_TRUE;
    aotOptions.requireFullAot = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state,
                                                             function,
                                                             typedGeneratedCPath,
                                                             &aotOptions));
    append_text_file_or_fail(typedGeneratedCPath, typedProbeSource);
    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -O2 -g -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             typedGeneratedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             typedSharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    library = dlopen(typedSharedLibraryPath, RTLD_NOW | RTLD_LOCAL);
    if (library == NULL) {
        printf("dlopen(%s) failed: %s\n", typedSharedLibraryPath, dlerror());
    }
    TEST_ASSERT_NOT_NULL(library);
    symbol = load_symbol(library, "ZrVm_Test_RunTypedLoopProbe");
    TEST_ASSERT_NOT_NULL(symbol);
    memcpy(&typedProbe, &symbol, sizeof(typedProbe));

    memset(&profileRuntime, 0, sizeof(profileRuntime));
    profileRuntime.recordHelpers = ZR_TRUE;
    state->global->profileRuntime = &profileRuntime;
    ZrCore_Profile_SetCurrentState(state);
    result = typedProbe(ZR_AOT_LOOP_LIMIT);
    TEST_ASSERT_EQUAL_INT64(ZR_AOT_LOOP_EXPECTED_RESULT, result);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_CONSTRUCT]);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_COPY]);
    TEST_ASSERT_EQUAL_UINT64(0u, profileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_RESET_NULL]);
    state->global->profileRuntime = ZR_NULL;
    ZrCore_Profile_SetCurrentState(ZR_NULL);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_construction_guardrail",
                                                       "general_aot_loop_project",
                                                       "general_aot_loop",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_construction_guardrail",
                                                       "general_aot_loop_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_construction_guardrail",
                                                       "general_aot_loop_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_construction_guardrail",
                                                       "general_aot_loop_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generalAotGeneratedCPath,
                                                       sizeof(generalAotGeneratedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_value_construction_guardrail",
                                                       "general_aot_loop_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       generalAotSharedLibraryPath,
                                                       sizeof(generalAotSharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, CZrAotGeneralLoopSource);
    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(generalAotState,
                                                                generalAotFunction,
                                                                zroPath,
                                                                &binaryOptions));
    hash_file_or_fail(zroPath, generalAotHash, sizeof(generalAotHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath,
                                          &generalAotEmbeddedBlob,
                                          &generalAotEmbeddedBlobLength));
    TEST_ASSERT_NOT_NULL(generalAotEmbeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, generalAotEmbeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = generalAotHash;
    aotOptions.embeddedModuleBlob = generalAotEmbeddedBlob;
    aotOptions.embeddedModuleBlobLength = generalAotEmbeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    aotOptions.requireFullAot = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(generalAotState,
                                                             generalAotFunction,
                                                             generalAotGeneratedCPath,
                                                             &aotOptions));
    ZrCore_Function_Free(generalAotState, generalAotFunction);
    generalAotFunction = ZR_NULL;
    generatedCText = read_text_file_owned_or_fail(generalAotGeneratedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "static TZrInt64 zr_aot_fn_0(struct SZrState *state) {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_gc_safepoint_back_edge */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ReturnI64(state,"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_typed_i64_fn_"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -O2 -g -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generalAotGeneratedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             generalAotSharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    project = ZrLibrary_Project_New(generalAotState,
                                    (TZrNativeString)projectJson,
                                    (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    generalAotState->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(generalAotState->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    memset(&generalAotProfileRuntime, 0, sizeof(generalAotProfileRuntime));
    generalAotProfileRuntime.recordHelpers = ZR_TRUE;
    generalAotState->global->profileRuntime = &generalAotProfileRuntime;
    ZrCore_Profile_SetCurrentState(generalAotState);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(generalAotState,
                                                               ZR_AOT_BACKEND_KIND_C,
                                                               &generalAotResult),
                             ZrLibrary_AotRuntime_GetLastError(generalAotState->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(generalAotResult.type));
    TEST_ASSERT_EQUAL_INT64(ZR_AOT_LOOP_EXPECTED_RESULT,
                            generalAotResult.value.nativeObject.nativeInt64);
    TEST_ASSERT_GREATER_THAN_UINT64(
            0u,
            generalAotProfileRuntime.helperCounts[ZR_PROFILE_HELPER_VALUE_CONSTRUCT]);
    generalAotState->global->profileRuntime = ZR_NULL;
    ZrCore_Profile_SetCurrentState(ZR_NULL);

    for (sampleIndex = 0u; sampleIndex < 3u; sampleIndex++) {
        TEST_ASSERT_EQUAL_INT64(ZR_AOT_LOOP_EXPECTED_RESULT, typedProbe(ZR_AOT_LOOP_LIMIT));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(ZR_AOT_LOOP_EXPECTED_RESULT, result);
        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ExecuteEntry(generalAotState,
                                                           ZR_AOT_BACKEND_KIND_C,
                                                           &generalAotResult));
        TEST_ASSERT_EQUAL_INT64(ZR_AOT_LOOP_EXPECTED_RESULT,
                                generalAotResult.value.nativeObject.nativeInt64);
    }

    for (sampleIndex = 0u; sampleIndex < ZR_AOT_LOOP_PERF_SAMPLE_COUNT; sampleIndex++) {
        typedSamples[sampleIndex] = benchmark_typed_loop_probe_ns_per_call(
                typedProbe,
                ZR_AOT_LOOP_PERF_AOT_ITERATIONS,
                &typedChecksum);
        interpreterSamples[sampleIndex] = benchmark_loop_interpreter_ns_per_call(
                state,
                function,
                ZR_AOT_LOOP_PERF_INTERPRETER_ITERATIONS,
                &interpreterChecksum);
        generalAotSamples[sampleIndex] = benchmark_general_aot_loop_ns_per_call(
                generalAotState,
                ZR_AOT_LOOP_PERF_GENERAL_AOT_ITERATIONS,
                &generalAotChecksum);
        TEST_ASSERT_TRUE(typedSamples[sampleIndex] > 0.0);
        TEST_ASSERT_TRUE(interpreterSamples[sampleIndex] > 0.0);
        TEST_ASSERT_TRUE(generalAotSamples[sampleIndex] > 0.0);
        TEST_ASSERT_EQUAL_INT64(ZR_AOT_LOOP_EXPECTED_RESULT *
                                        (TZrInt64)ZR_AOT_LOOP_PERF_AOT_ITERATIONS,
                                typedChecksum);
        TEST_ASSERT_EQUAL_INT64(ZR_AOT_LOOP_EXPECTED_RESULT *
                                        (TZrInt64)ZR_AOT_LOOP_PERF_INTERPRETER_ITERATIONS,
                                interpreterChecksum);
        TEST_ASSERT_EQUAL_INT64(ZR_AOT_LOOP_EXPECTED_RESULT *
                                        (TZrInt64)ZR_AOT_LOOP_PERF_GENERAL_AOT_ITERATIONS,
                                generalAotChecksum);
    }

    typedNsPerCall = min_positive_sample(typedSamples, ZR_AOT_LOOP_PERF_SAMPLE_COUNT);
    interpreterNsPerCall = min_positive_sample(interpreterSamples, ZR_AOT_LOOP_PERF_SAMPLE_COUNT);
    generalAotNsPerCall = min_positive_sample(generalAotSamples, ZR_AOT_LOOP_PERF_SAMPLE_COUNT);
    TEST_ASSERT_TRUE(typedNsPerCall > 0.0);
    TEST_ASSERT_TRUE(interpreterNsPerCall > 0.0);
    TEST_ASSERT_TRUE(generalAotNsPerCall > 0.0);
    interpreterSpeedup = interpreterNsPerCall / typedNsPerCall;
    generalAotSpeedup = generalAotNsPerCall / typedNsPerCall;
    printf("AOT typed loop performance: typed=%.3f ns/call interpreter=%.3f ns/call general_aot=%.3f ns/call "
           "interpreter_speedup=%.2fx general_aot_speedup=%.2fx target_met=%s\n",
           typedNsPerCall,
           interpreterNsPerCall,
           generalAotNsPerCall,
           interpreterSpeedup,
           generalAotSpeedup,
           interpreterSpeedup >= ZR_AOT_LOOP_PERF_TARGET_SPEEDUP ? "yes" : "no");
    TEST_ASSERT_TRUE_MESSAGE(interpreterSpeedup >= ZR_AOT_LOOP_PERF_MIN_SPEEDUP,
                             "typed loop AOT must not be slower than the interpreter");
    TEST_ASSERT_TRUE_MESSAGE(generalAotSpeedup >= ZR_AOT_LOOP_PERF_MIN_GENERAL_AOT_SPEEDUP,
                             "typed loop AOT must be measurably faster than general environment AOT");

    generalAotState->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(generalAotState, project);
    TEST_ASSERT_EQUAL_INT(0, dlclose(library));
    free(generalAotEmbeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(generalAotState);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_full_aot_typed_i64_thunk_constructs_no_type_values);
    RUN_TEST(test_full_aot_typed_i64_counting_loop_emits_state_free_thunk);
    RUN_TEST(test_full_aot_typed_i64_counting_loop_runtime_and_performance_gate);
    return UNITY_END();
}
