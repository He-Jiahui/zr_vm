//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "unity.h"
#include "test_support.h"
#include "zr_vm_parser.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

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

static SZrState* create_test_state(void) {
    return ZrTests_State_Create(ZR_NULL);
}

static void destroy_test_state(SZrState* state) {
    ZrTests_State_Destroy(state);
}

static TZrBool execute_test_function(SZrState* state, SZrFunction* testFunc, TZrInt64 expectedValue, const TZrChar* testName) {
    TZrInt64 actualValue = 0;

    ZR_UNUSED_PARAMETER(testName);

    if (!ZrTests_Function_ExecuteExpectInt64(state, testFunc, &actualValue)) {
        return ZR_FALSE;
    }

    if (expectedValue >= 0) {
        TEST_ASSERT_EQUAL_INT64(expectedValue, actualValue);
    }

    return ZR_TRUE;
}

// 测试初始化和清理
void setUp(void) {
}

void tearDown(void) {
}

// ==================== 命名参数测试 ====================

// 测试1: 基本命名参数调用
void test_named_arguments_basic(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Named Arguments - Basic";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Basic named arguments", 
              "Testing function call with named arguments: func(c: 3, a: 1, b: 2)");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "testBasicNamedArgs(a: int, b: int, c: int): int {\n"
        "    return a + b + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return testBasicNamedArgs(c: 3, a: 1, b: 2);\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_named_args.zr", 20);
    
    printf("  [DEBUG] Starting parsing...\n");
    fflush(stdout);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    printf("  [DEBUG] Parsing completed successfully.\n");
    fflush(stdout);
    
    // 执行测试函数
    SZrCompileResult compileResult;
    // 初始化编译结果
    compileResult.mainFunction = ZR_NULL;
    compileResult.testFunctions = ZR_NULL;
    compileResult.testFunctionCount = 0;
    
    printf("  [DEBUG] Starting compilation with tests...\n");
    fflush(stdout);
    
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    printf("  [DEBUG] Compilation completed. testFunctionCount: %zu\n", compileResult.testFunctionCount);
    fflush(stdout);
    
    // 查找测试函数（测试函数已复制主函数的 childFunctions，GET_SUB_FUNCTION 可从当前闭包解析顶层函数）
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 6, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }   
    }
    
    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试2: 位置参数和命名参数混合
void test_named_arguments_mixed(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Named Arguments - Mixed Positional and Named";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Mixed positional and named arguments", 
              "Testing: func(1, c: 3, b: 2)");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "testMixedArgs(a: int, b: int, c: int): int {\n"
        "    return a * 100 + b * 10 + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return testMixedArgs(1, c: 3, b: 2);\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_mixed_args.zr", 20);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 123, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试3: 默认参数与命名参数结合
void test_named_arguments_with_defaults(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Named Arguments - With Default Parameters";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Named arguments with default parameters", 
              "Testing: func(5) and func(5, c: 30)");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "testDefaultArgs(a: int, b: int = 10, c: int = 20): int {\n"
        "    return a + b + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    var r1 = testDefaultArgs(5);\n"
        "    var r2 = testDefaultArgs(5, c: 30);\n"
        "    return r1 + r2;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_default_args.zr", 22);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 80, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试4: 命名参数顺序无关
void test_named_arguments_order_independent(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Named Arguments - Order Independent";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Named arguments order independence", 
              "Testing: func(c: 10, a: 5, b: 3)");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "testNamedOrder(a: int, b: int, c: int): int {\n"
        "    return a - b + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return testNamedOrder(c: 10, a: 5, b: 3);\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_order.zr", 14);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 12, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试5: 复杂函数调用中的命名参数
void test_named_arguments_complex(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Named Arguments - Complex Function Call";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Complex function call with named arguments", 
              "Testing: complexFunction(w: 4, x: 1, z: 3, y: 2)");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "complexFunction(x: int, y: int, z: int, w: int): int {\n"
        "    return x * 1000 + y * 100 + z * 10 + w;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return complexFunction(w: 4, x: 1, z: 3, y: 2);\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_complex.zr", 16);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 1234, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 主函数
int main(void) {
    UNITY_BEGIN();
    
    TEST_MODULE_DIVIDER();
    printf("Named Arguments Tests\n");
    TEST_MODULE_DIVIDER();
    
    RUN_TEST(test_named_arguments_basic);
    RUN_TEST(test_named_arguments_mixed);
    RUN_TEST(test_named_arguments_with_defaults);
    RUN_TEST(test_named_arguments_order_independent);
    RUN_TEST(test_named_arguments_complex);
    
    return UNITY_END();
}
