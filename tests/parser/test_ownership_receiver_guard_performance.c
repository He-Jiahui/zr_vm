#include "unity.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_parser/compiler.h"

#define ZR_RECEIVER_GUARD_PERF_ITERATIONS 16384u
#define ZR_RECEIVER_GUARD_PERF_SAMPLES 3u
#define ZR_RECEIVER_GUARD_PERF_VALUE 7
#define ZR_RECEIVER_GUARD_PERF_EXPECTED_RESULT \
    ((TZrInt64)ZR_RECEIVER_GUARD_PERF_ITERATIONS * ZR_RECEIVER_GUARD_PERF_VALUE)

typedef struct SZrReceiverGuardBenchmarkCase {
    const TZrChar *name;
    const TZrChar *source;
    SZrFunction *function;
    double nanosecondsPerOperation;
    TZrInt64 checksum;
} SZrReceiverGuardBenchmarkCase;

static SZrState *g_state;

static const TZrChar *const CZrDirectAccessSource =
        "class Service {\n"
        "    pub fn read(): int { return 7; }\n"
        "}\n"
        "var target = new Service();\n"
        "var index = 0;\n"
        "var checksum = 0;\n"
        "while (index < 16384) {\n"
        "    checksum = checksum + target.read();\n"
        "    index = index + 1;\n"
        "}\n"
        "return checksum;\n";

static const TZrChar *const CZrWeakDirectSource =
        "resource class Service {\n"
        "    pub const fn read(): int { return 7; }\n"
        "}\n"
        "var seed = own Service();\n"
        "var shared = share(seed);\n"
        "var weak = degrade(shared);\n"
        "var index = 0;\n"
        "var checksum = 0;\n"
        "while (index < 16384) {\n"
        "    checksum = checksum + weak.read();\n"
        "    index = index + 1;\n"
        "}\n"
        "return checksum;\n";

static const TZrChar *const CZrWeakOptionalSuccessSource =
        "resource class Service {\n"
        "    pub const fn read(): int { return 7; }\n"
        "}\n"
        "var seed = own Service();\n"
        "var shared = share(seed);\n"
        "var weak = degrade(shared);\n"
        "var index = 0;\n"
        "var checksum = 0;\n"
        "while (index < 16384) {\n"
        "    var value = weak?.read();\n"
        "    if (value == 7) { checksum = checksum + 7; }\n"
        "    index = index + 1;\n"
        "}\n"
        "return checksum;\n";

static const TZrChar *const CZrWeakOptionalFailureSource =
        "resource class Service {\n"
        "    pub const fn read(): int { return 7; }\n"
        "}\n"
        "var seed = own Service();\n"
        "var shared = share(seed);\n"
        "var weak = degrade(shared);\n"
        "drop(shared);\n"
        "var index = 0;\n"
        "var checksum = 0;\n"
        "while (index < 16384) {\n"
        "    var value = weak?.read();\n"
        "    if (value == null) { checksum = checksum + 7; }\n"
        "    index = index + 1;\n"
        "}\n"
        "return checksum;\n";

static const TZrChar *const CZrDeepWeakGuardSource =
        "resource class Child {\n"
        "    pub var value: int;\n"
        "    pub @constructor(value: int) { this.value = value; }\n"
        "}\n"
        "resource class Root {\n"
        "    pub var child: Unique<Child>;\n"
        "    pub @constructor() { this.child = own Child(7); }\n"
        "}\n"
        "var seed = own Root();\n"
        "var shared = share(seed);\n"
        "var weak = degrade(shared);\n"
        "var index = 0;\n"
        "var checksum = 0;\n"
        "while (index < 16384) {\n"
        "    var value = weak?.child.value;\n"
        "    if (value == 7) { checksum = checksum + 7; }\n"
        "    index = index + 1;\n"
        "}\n"
        "return checksum;\n";

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static double receiver_guard_elapsed_nanoseconds(clock_t start, clock_t end) {
    return ((double)(end - start) * 1000000000.0) / (double)CLOCKS_PER_SEC;
}

static void receiver_guard_compile_case(SZrReceiverGuardBenchmarkCase *benchmarkCase) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(benchmarkCase);
    sourceName = ZrCore_String_Create(
            g_state,
            (TZrNativeString)benchmarkCase->name,
            strlen(benchmarkCase->name));
    benchmarkCase->function = ZrParser_Source_Compile(
            g_state,
            benchmarkCase->source,
            strlen(benchmarkCase->source),
            sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(benchmarkCase->function, benchmarkCase->name);
}

static void receiver_guard_run_case(SZrReceiverGuardBenchmarkCase *benchmarkCase) {
    clock_t start;
    clock_t end;
    TZrInt64 result = 0;
    TZrUInt32 sample;

    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(
                    g_state, benchmarkCase->function, &result),
            benchmarkCase->name);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(
            ZR_RECEIVER_GUARD_PERF_EXPECTED_RESULT, result, benchmarkCase->name);

    benchmarkCase->checksum = 0;
    start = clock();
    for (sample = 0u; sample < ZR_RECEIVER_GUARD_PERF_SAMPLES; sample++) {
        TEST_ASSERT_TRUE_MESSAGE(
                ZrTests_Runtime_Function_ExecuteExpectInt64(
                        g_state, benchmarkCase->function, &result),
                benchmarkCase->name);
        TEST_ASSERT_EQUAL_INT64_MESSAGE(
                ZR_RECEIVER_GUARD_PERF_EXPECTED_RESULT,
                result,
                benchmarkCase->name);
        benchmarkCase->checksum += result;
    }
    end = clock();

    TEST_ASSERT_GREATER_OR_EQUAL(0, (long long)(end - start));
    benchmarkCase->nanosecondsPerOperation =
            receiver_guard_elapsed_nanoseconds(start, end) /
            ((double)ZR_RECEIVER_GUARD_PERF_ITERATIONS *
             (double)ZR_RECEIVER_GUARD_PERF_SAMPLES);
    TEST_ASSERT_GREATER_THAN_DOUBLE(0.0, benchmarkCase->nanosecondsPerOperation);
    TEST_ASSERT_EQUAL_INT64(
            ZR_RECEIVER_GUARD_PERF_EXPECTED_RESULT *
                    (TZrInt64)ZR_RECEIVER_GUARD_PERF_SAMPLES,
            benchmarkCase->checksum);
}

static void test_receiver_guard_benchmark_reports_costs(void) {
    SZrReceiverGuardBenchmarkCase cases[] = {
            {"direct_non_null", CZrDirectAccessSource, ZR_NULL, 0.0, 0},
            {"weak_direct", CZrWeakDirectSource, ZR_NULL, 0.0, 0},
            {"weak_optional_success", CZrWeakOptionalSuccessSource, ZR_NULL, 0.0, 0},
            {"weak_optional_failure", CZrWeakOptionalFailureSource, ZR_NULL, 0.0, 0},
            {"deep_weak_guard", CZrDeepWeakGuardSource, ZR_NULL, 0.0, 0},
    };
    const TZrSize caseCount = sizeof(cases) / sizeof(cases[0]);
    double directNanoseconds;
    TZrSize index;

    for (index = 0u; index < caseCount; index++) {
        receiver_guard_compile_case(&cases[index]);
        receiver_guard_run_case(&cases[index]);
    }

    directNanoseconds = cases[0].nanosecondsPerOperation;
    for (index = 0u; index < caseCount; index++) {
        printf(
                "OWNERSHIP_RECEIVER_GUARD_PERF variant=%s iterations=%u "
                "samples=%u ns_per_operation=%.3f ratio_to_direct=%.3f "
                "checksum=%lld\n",
                cases[index].name,
                (unsigned int)ZR_RECEIVER_GUARD_PERF_ITERATIONS,
                (unsigned int)ZR_RECEIVER_GUARD_PERF_SAMPLES,
                cases[index].nanosecondsPerOperation,
                cases[index].nanosecondsPerOperation / directNanoseconds,
                (long long)cases[index].checksum);
        ZrCore_Function_Free(g_state, cases[index].function);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_receiver_guard_benchmark_reports_costs);
    return UNITY_END();
}
