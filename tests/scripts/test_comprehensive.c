//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "test_utils.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

// 测试日志宏（符合测试规范）
#define TEST_START(summary) do { \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while(0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while(0)

#define TEST_PASS_CUSTOM(timer, summary) do { \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while(0)

#define TEST_FAIL_CUSTOM(timer, summary, reason) do { \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason); \
    fflush(stdout); \
} while(0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while(0)

#define TEST_MODULE_DIVIDER() do { \
    printf("==========\n"); \
    fflush(stdout); \
} while(0)

// 测试初始化和清理
void setUp(void) {
}

void tearDown(void) {
}

// 辅助函数：运行测试用例
static TBool run_test_case(const TChar* testName, const TChar* fileName) {
    SZrTestTimer timer;
    timer.startTime = clock();
    
    TEST_START(testName);
    
    SZrState* state = create_test_state();
    if (state == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testName, "Failed to create test state");
        return ZR_FALSE;
    }
    
    // 构建文件路径
    TChar filePath[1024];
    const TChar* possiblePaths[] = {
        "test_cases",
        "tests/scripts/test_cases",
        ".",
        ZR_NULL
    };
    
    FILE* testFile = ZR_NULL;
    for (TZrSize i = 0; possiblePaths[i] != ZR_NULL; i++) {
        snprintf(filePath, sizeof(filePath), "%s/%s", possiblePaths[i], fileName);
        testFile = fopen(filePath, "rb");
        if (testFile != ZR_NULL) {
            break;
        }
    }
    
    if (testFile == ZR_NULL) {
        TEST_INFO("File loading", "Could not find test file, trying alternative paths");
        // 尝试直接使用文件名
        testFile = fopen(fileName, "rb");
    }
    
    if (testFile == ZR_NULL) {
        destroy_test_state(state);
        TEST_FAIL_CUSTOM(timer, testName, "Failed to open test file");
        return ZR_FALSE;
    }
    
    // 读取文件内容
    fseek(testFile, 0, SEEK_END);
    long fileSize = ftell(testFile);
    fseek(testFile, 0, SEEK_SET);
    
    if (fileSize < 0) {
        fclose(testFile);
        destroy_test_state(state);
        TEST_FAIL_CUSTOM(timer, testName, "Failed to get file size");
        return ZR_FALSE;
    }
    
    TChar* source = (TChar*)malloc((TZrSize)fileSize + 1);
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
    
    // 解析并编译
    SZrString* sourceName = ZrStringCreate(state, fileName, strlen(fileName));
    SZrTestResult* result = parse_and_compile(state, source, readSize, fileName);
    
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
    
    // 输出AST
    TChar baseName[256];
    strncpy(baseName, fileName, sizeof(baseName) - 1);
    baseName[sizeof(baseName) - 1] = '\0';
    // 移除扩展名
    TChar* dot = strrchr(baseName, '.');
    if (dot != ZR_NULL) {
        *dot = '\0';
    }
    
    TEST_INFO("Dumping AST", "Outputting syntax tree to files");
    dump_ast_to_file(state, result->ast, baseName);
    
    // 输出中间码
    TEST_INFO("Dumping Intermediate Code", "Outputting intermediate code to files");
    dump_intermediate_to_file(state, result->function, baseName);
    
    // 执行代码
    TEST_INFO("Executing", "Running compiled function");
    SZrTypeValue execResult;
    ZrValueResetAsNull(&execResult);
    TBool execSuccess = execute_function(state, result->function, &execResult);
    
    // 输出运行状态
    if (execSuccess) {
        TEST_INFO("Dumping Runtime State", "Outputting runtime state to files");
        dump_runtime_state(state, baseName);
    }
    
    // 清理
    free(source);
    free_test_result(result);
    destroy_test_state(state);
    
    timer.endTime = clock();
    if (execSuccess) {
        TEST_PASS_CUSTOM(timer, testName);
        TEST_DIVIDER();
        return ZR_TRUE;
    } else {
        TEST_FAIL_CUSTOM(timer, testName, "Execution failed");
        TEST_DIVIDER();
        return ZR_FALSE;
    }
}

// ==================== 基础功能测试 ====================

void test_basic_arithmetic_operations(void) {
    TBool result = run_test_case("Basic Arithmetic Operations", "basic_operations.zr");
    TEST_ASSERT_TRUE(result);
}

void test_basic_branch_statements(void) {
    TBool result = run_test_case("Basic Branch Statements", "basic_operations.zr");
    TEST_ASSERT_TRUE(result);
}

void test_basic_loops(void) {
    TBool result = run_test_case("Basic Loops", "basic_operations.zr");
    TEST_ASSERT_TRUE(result);
}

void test_basic_functions(void) {
    TBool result = run_test_case("Basic Functions", "basic_operations.zr");
    TEST_ASSERT_TRUE(result);
}

void test_basic_variable_declarations(void) {
    TBool result = run_test_case("Basic Variable Declarations", "basic_operations.zr");
    TEST_ASSERT_TRUE(result);
}

// ==================== 高级特性测试 ====================

void test_closures(void) {
    TBool result = run_test_case("Closures", "closures.zr");
    TEST_ASSERT_TRUE(result);
}

void test_lambda_expressions(void) {
    TBool result = run_test_case("Lambda Expressions", "lambda.zr");
    TEST_ASSERT_TRUE(result);
}

void test_module_declarations(void) {
    TBool result = run_test_case("Module Declarations", "modules.zr");
    TEST_ASSERT_TRUE(result);
}

void test_module_imports(void) {
    TBool result = run_test_case("Module Imports", "modules.zr");
    TEST_ASSERT_TRUE(result);
}

void test_classes(void) {
    TBool result = run_test_case("Classes", "classes.zr");
    TEST_ASSERT_TRUE(result);
}

void test_enums(void) {
    TBool result = run_test_case("Enums", "enums.zr");
    TEST_ASSERT_TRUE(result);
}

void test_intermediate_code(void) {
    TBool result = run_test_case("Intermediate Code", "intermediate.zr");
    TEST_ASSERT_TRUE(result);
}

void test_decorators(void) {
    TBool result = run_test_case("Decorators", "decorators.zr");
    TEST_ASSERT_TRUE(result);
}

void test_test_declarations(void) {
    TBool result = run_test_case("Test Declarations", "test_declarations.zr");
    TEST_ASSERT_TRUE(result);
}

// ==================== 集成测试 ====================

void test_full_compilation_pipeline(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Full Compilation Pipeline";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Full Pipeline", "Testing complete compilation pipeline: parse -> compile -> execute");
    
    // 使用basic_operations.zr作为完整流程测试
    TBool result = run_test_case("Full Pipeline Test", "basic_operations.zr");
    
    timer.endTime = clock();
    if (result) {
        TEST_PASS_CUSTOM(timer, testSummary);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Pipeline test failed");
    }
    TEST_DIVIDER();
    
    TEST_ASSERT_TRUE(result);
}

void test_multiple_modules(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Multiple Modules";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Multiple Modules", "Testing module A and module B");
    
    TBool resultA = run_test_case("Module A", "module_a.zr");
    TBool resultB = run_test_case("Module B", "module_b.zr");
    
    timer.endTime = clock();
    TBool overallResult = resultA && resultB;
    if (overallResult) {
        TEST_PASS_CUSTOM(timer, testSummary);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "One or more module tests failed");
    }
    TEST_DIVIDER();
    
    TEST_ASSERT_TRUE(overallResult);
}

void test_complex_programs(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Complex Programs";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    TEST_INFO("Complex Programs", "Testing complex program features");
    
    // 测试多个复杂特性
    TBool results[] = {
        run_test_case("Complex - Classes", "classes.zr"),
        run_test_case("Complex - Closures", "closures.zr"),
        run_test_case("Complex - Lambda", "lambda.zr"),
    };
    
    TBool overallResult = ZR_TRUE;
    for (TZrSize i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
        if (!results[i]) {
            overallResult = ZR_FALSE;
            break;
        }
    }
    
    timer.endTime = clock();
    if (overallResult) {
        TEST_PASS_CUSTOM(timer, testSummary);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "One or more complex program tests failed");
    }
    TEST_DIVIDER();
    
    TEST_ASSERT_TRUE(overallResult);
}

// ==================== 主函数 ====================

int main(void) {
    printf("\n");
    TEST_MODULE_DIVIDER();
    printf("ZR-VM Comprehensive Scripts Tests\n");
    TEST_MODULE_DIVIDER();
    printf("\n");
    
    UNITY_BEGIN();
    
    // 基础功能测试模块
    printf("==========\n");
    printf("Basic Functionality Tests\n");
    printf("==========\n");
    RUN_TEST(test_basic_arithmetic_operations);
    RUN_TEST(test_basic_branch_statements);
    RUN_TEST(test_basic_loops);
    RUN_TEST(test_basic_functions);
    RUN_TEST(test_basic_variable_declarations);
    
    // 高级特性测试模块
    printf("==========\n");
    printf("Advanced Features Tests\n");
    printf("==========\n");
    RUN_TEST(test_closures);
    RUN_TEST(test_lambda_expressions);
    RUN_TEST(test_module_declarations);
    RUN_TEST(test_module_imports);
    RUN_TEST(test_classes);
    RUN_TEST(test_enums);
    RUN_TEST(test_intermediate_code);
    RUN_TEST(test_decorators);
    RUN_TEST(test_test_declarations);
    
    // 集成测试模块
    printf("==========\n");
    printf("Integration Tests\n");
    printf("==========\n");
    RUN_TEST(test_full_compilation_pipeline);
    RUN_TEST(test_multiple_modules);
    RUN_TEST(test_complex_programs);
    
    printf("\n");
    TEST_MODULE_DIVIDER();
    printf("All Test Cases Completed\n");
    TEST_MODULE_DIVIDER();
    printf("\n");
    
    return UNITY_END();
}

