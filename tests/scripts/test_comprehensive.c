//
// Curated script regression suite for stable M1-M3 capabilities.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "test_utils.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#define TEST_START(summary) do { \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while (0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while (0)

#define TEST_PASS_CUSTOM(timer, summary) do { \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while (0)

#define TEST_FAIL_CUSTOM(timer, summary, reason) do { \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason); \
    fflush(stdout); \
} while (0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while (0)

#define TEST_MODULE_DIVIDER() do { \
    printf("==========\n"); \
    fflush(stdout); \
} while (0)

typedef struct {
    const TChar *summary;
    const TChar *fileName;
} SZrScriptRegressionCase;

static const SZrScriptRegressionCase ZR_SCRIPT_REGRESSION_CASES[] = {
    {"Basic Script Smoke", "basic_operations.zr"},
    {"Classes", "classes.zr"},
    {"Classes Full", "classes_full.zr"},
};

void setUp(void) {}

void tearDown(void) {}

static TBool run_test_case(const TChar *testName, const TChar *fileName) {
    SZrTestTimer timer;
    timer.startTime = clock();

    TEST_START(testName);

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testName, "Failed to create test state");
        return ZR_FALSE;
    }

    TChar filePath[1024];
    const TChar *possiblePaths[] = {
        "test_cases",
        "tests/scripts/test_cases",
        ".",
        ZR_NULL
    };

    FILE *testFile = ZR_NULL;
    for (TZrSize i = 0; possiblePaths[i] != ZR_NULL; i++) {
        snprintf(filePath, sizeof(filePath), "%s/%s", possiblePaths[i], fileName);
        testFile = fopen(filePath, "rb");
        if (testFile != ZR_NULL) {
            break;
        }
    }

    if (testFile == ZR_NULL) {
        TEST_INFO("File loading", "Could not find test file, trying direct path");
        testFile = fopen(fileName, "rb");
    }

    if (testFile == ZR_NULL) {
        destroy_test_state(state);
        TEST_FAIL_CUSTOM(timer, testName, "Failed to open test file");
        return ZR_FALSE;
    }

    fseek(testFile, 0, SEEK_END);
    long fileSize = ftell(testFile);
    fseek(testFile, 0, SEEK_SET);
    if (fileSize < 0) {
        fclose(testFile);
        destroy_test_state(state);
        TEST_FAIL_CUSTOM(timer, testName, "Failed to get file size");
        return ZR_FALSE;
    }

    TChar *source = (TChar *)malloc((TZrSize)fileSize + 1);
    if (source == ZR_NULL) {
        fclose(testFile);
        destroy_test_state(state);
        TEST_FAIL_CUSTOM(timer, testName, "Failed to allocate memory for source");
        return ZR_FALSE;
    }

    TZrSize readSize = (TZrSize)fread(source, 1, (TZrSize)fileSize, testFile);
    fclose(testFile);
    source[readSize] = '\0';

    TEST_INFO("Parsing and Compiling", fileName);

    SZrTestResult *result = parse_and_compile(state, source, readSize, fileName);

    if (result == ZR_NULL || !result->success) {
        free(source);
        destroy_test_state(state);
        if (result != ZR_NULL) {
            TEST_FAIL_CUSTOM(timer, testName, result->errorMessage ? result->errorMessage : "Unknown error");
            free_test_result(result);
        } else {
            TEST_FAIL_CUSTOM(timer, testName, "Failed to parse and compile");
        }
        return ZR_FALSE;
    }

    TChar baseName[256];
    strncpy(baseName, fileName, sizeof(baseName) - 1);
    baseName[sizeof(baseName) - 1] = '\0';
    TChar *dot = strrchr(baseName, '.');
    if (dot != ZR_NULL) {
        *dot = '\0';
    }

    TEST_INFO("Dumping AST", "Outputting syntax tree to files");
    dump_ast_to_file(state, result->ast, baseName);

    TEST_INFO("Dumping Intermediate Code", "Outputting intermediate code to files");
    dump_intermediate_to_file(state, result->function, baseName);

    TEST_INFO("Executing", "Running compiled function");
    SZrTypeValue execResult;
    ZrValueResetAsNull(&execResult);
    TBool execSuccess = execute_function(state, result->function, &execResult);

    if (execSuccess) {
        TEST_INFO("Dumping Runtime State", "Outputting runtime state to files");
        dump_runtime_state(state, baseName);
    }

    free(source);
    free_test_result(result);
    destroy_test_state(state);

    timer.endTime = clock();
    if (execSuccess) {
        TEST_PASS_CUSTOM(timer, testName);
        TEST_DIVIDER();
        return ZR_TRUE;
    }

    TEST_FAIL_CUSTOM(timer, testName, "Execution failed");
    TEST_DIVIDER();
    return ZR_FALSE;
}

static void assert_script_case_passes(const SZrScriptRegressionCase *testCase) {
    TEST_ASSERT_NOT_NULL(testCase);
    TEST_ASSERT_TRUE(run_test_case(testCase->summary, testCase->fileName));
}

void test_basic_script_smoke(void) {
    assert_script_case_passes(&ZR_SCRIPT_REGRESSION_CASES[0]);
}

void test_script_classes(void) {
    assert_script_case_passes(&ZR_SCRIPT_REGRESSION_CASES[1]);
}

void test_script_classes_full(void) {
    assert_script_case_passes(&ZR_SCRIPT_REGRESSION_CASES[2]);
}

void test_script_regression_matrix(void) {
    for (TZrSize i = 0; i < sizeof(ZR_SCRIPT_REGRESSION_CASES) / sizeof(ZR_SCRIPT_REGRESSION_CASES[0]); i++) {
        assert_script_case_passes(&ZR_SCRIPT_REGRESSION_CASES[i]);
    }
}

int main(void) {
    printf("\n");
    TEST_MODULE_DIVIDER();
    printf("ZR-VM Curated Scripts Regression Tests\n");
    TEST_MODULE_DIVIDER();
    printf("\n");

    UNITY_BEGIN();

    printf("==========\n");
    printf("Stable Regression Matrix\n");
    printf("==========\n");
    RUN_TEST(test_basic_script_smoke);
    RUN_TEST(test_script_classes);
    RUN_TEST(test_script_classes_full);
    RUN_TEST(test_script_regression_matrix);

    printf("\n");
    TEST_MODULE_DIVIDER();
    printf("Curated Script Regression Completed\n");
    TEST_MODULE_DIVIDER();
    printf("\n");

    return UNITY_END();
}
