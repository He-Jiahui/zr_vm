//
// Created by Auto on 2025/01/XX.
//

// 定义GNU源以支持realpath函数（Linux系统）
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
// 定义POSIX源以支持realpath函数（备用）
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#ifdef _MSC_VER
    #include <direct.h>
    #define getcwd _getcwd
#else
    #include <unistd.h>
#endif
#include "unity.h"
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

// 简单的测试分配器
static TZrPtr testAllocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);
    
    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }
    
    if (pointer == ZR_NULL) {
        return malloc(newSize);
    } else {
        if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
            return realloc(pointer, newSize);
        } else {
            return malloc(newSize);
        }
    }
}

// 创建测试用的SZrState
static SZrState* createTestState(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState* global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12345, &callbacks);
    if (!global) return ZR_NULL;
    
    SZrState* mainState = global->mainThreadState;
    if (mainState) {
        ZrGlobalStateInitRegistry(mainState, global);
    }
    
    return mainState;
}

// 销毁测试用的SZrState
static void destroyTestState(SZrState* state) {
    if (!state) return;
    
    SZrGlobalState* global = state->global;
    if (global) {
        ZrGlobalStateFree(global);
    }
}

// 辅助函数：执行测试函数
static TBool execute_test_function(SZrState* state, SZrFunction* testFunc, TInt64 expectedValue, const TChar* testName) {
    if (state == ZR_NULL || testFunc == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 创建闭包
    SZrClosure* closure = ZrClosureNew(state, 0);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }
    
    closure->function = testFunc;
    ZrClosureInitValue(state, closure);
    
    // 准备调用栈
    TZrStackValuePointer base = state->stackTop.valuePointer;
    ZrFunctionCheckStackAndGc(state, testFunc->stackSize + 1, base);
    
    // 将闭包压栈
    SZrTypeValue* closureValue = ZrStackGetValue(state->stackTop.valuePointer);
    ZrValueInitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_FUNCTION;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;
    
    // 调用函数
    ZrFunctionCall(state, base, 1);
    
    // 检查执行状态
    if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
        // 获取返回值
        SZrTypeValue* returnValue = ZrStackGetValue(base);
        if (ZR_VALUE_IS_TYPE_INT(returnValue->type)) {
            TInt64 value = returnValue->value.nativeObject.nativeInt64;
            if (expectedValue >= 0) {
                TEST_ASSERT_EQUAL_INT64(expectedValue, value);
            }
            return ZR_TRUE;
        }
    }
    
    return ZR_FALSE;
}

// 测试初始化和清理
void setUp(void) {
}

void tearDown(void) {
}

// ==================== 编译期执行测试 ====================

// 测试1: 编译期变量声明和使用
void test_compile_time_variables(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Compile-Time Execution - Variables";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time variable declaration and usage", 
              "Testing %compileTime var MAX_SIZE = 100");
    
    const TChar* source = 
        "module \"test\";\n"
        "%compileTime var MAX_SIZE = 100;\n"
        "%compileTime var MIN_SIZE = 1;\n"
        "%compileTime var DEFAULT_VALUE = 42;\n"
        "var runtimeVar = DEFAULT_VALUE;\n"
        "%test(\"test\") {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrStringCreate(state, "test_compile_time_vars.zr", 26);
    SZrAstNode* ast = ZrParserParse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrCompilerCompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParserFreeAst(state, ast);
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 42, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrCompileResultFree(state, &compileResult);
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试2: 编译期函数声明和调用
void test_compile_time_functions(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Compile-Time Execution - Functions";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time function declaration and call", 
              "Testing %compileTime function calculateSum");
    
    const TChar* source = 
        "module \"test\";\n"
        "%compileTime calculateSum(a: int, b: int): int {\n"
        "    return a + b;\n"
        "}\n"
        "%compileTime var computedValue = calculateSum(10, 20);\n"
        "var runtimeVar = computedValue;\n"
        "%test(\"test\") {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrStringCreate(state, "test_compile_time_funcs.zr", 27);
    SZrAstNode* ast = ZrParserParse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrCompilerCompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParserFreeAst(state, ast);
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction** testFuncPtr = (SZrFunction**)ZrArrayGet(&compileResult.testFunctions, 0);
        if (testFuncPtr != ZR_NULL && *testFuncPtr != ZR_NULL) {
            SZrFunction* testFunc = *testFuncPtr;
            if (!execute_test_function(state, testFunc, 30, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrCompileResultFree(state, &compileResult);
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试3: 编译期表达式计算
static void test_compile_time_expressions(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Compile-Time Execution - Expressions";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time expression evaluation", 
              "Testing %compileTime var complexExpr = (1+2)*3*4 - 10");
    
    const TChar* source = 
        "module \"test\";\n"
        "%compileTime calculateSum(a: int, b: int): int {\n"
        "    return a + b;\n"
        "}\n"
        "%compileTime multiply(x: int, y: int): int {\n"
        "    return x * y;\n"
        "}\n"
        "%compileTime var complexExpr = (calculateSum(1, 2) * multiply(3, 4)) - 10;\n"
        "var runtimeVar = complexExpr;\n"
        "%test(\"test\") {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrStringCreate(state, "test_compile_time_expr.zr", 26);
    SZrAstNode* ast = ZrParserParse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrCompilerCompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParserFreeAst(state, ast);
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 26, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }   
        }
    }
    
    ZrCompileResultFree(state, &compileResult);
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试4: 编译期递归调用
void test_compile_time_recursion(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Compile-Time Execution - Recursion";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time recursive function call", 
              "Testing %compileTime function factorial");
    
    const TChar* source = 
        "module \"test\";\n"
        "%compileTime factorial(n: int): int {\n"
        "    if (n <= 1) {\n"
        "        return 1;\n"
        "    }\n"
        "    return n * factorial(n - 1);\n"
        "}\n"
        "%compileTime var fact5 = factorial(5);\n"
        "var runtimeVar = fact5;\n"
        "%test(\"test\") {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrStringCreate(state, "test_compile_time_recursion.zr", 32);
    SZrAstNode* ast = ZrParserParse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrCompilerCompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParserFreeAst(state, ast);
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 120, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrCompileResultFree(state, &compileResult);
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试5: 编译期语句块
void test_compile_time_statements(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Compile-Time Execution - Statement Blocks";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time statement block execution", 
              "Testing %compileTime { ... }");
    
    const TChar* source = 
        "module \"test\";\n"
        "%compileTime var MAX_SIZE = 100;\n"
        "%compileTime var MIN_SIZE = 1;\n"
        "%compileTime {\n"
        "    if (MAX_SIZE < MIN_SIZE) {\n"
        "        FatalError(\"MAX_SIZE must be greater than MIN_SIZE\");\n"
        "    }\n"
        "}\n"
        "var runtimeVar = 42;\n"
        "%test(\"test\") {\n"
        "    return runtimeVar;\n"
        "}\n";
    
    SZrString* sourceName = ZrStringCreate(state, "test_compile_time_stmts.zr", 28);
    SZrAstNode* ast = ZrParserParse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrCompilerCompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParserFreeAst(state, ast);
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 42, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrCompileResultFree(state, &compileResult);
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试6: 编译期数组大小验证
void test_compile_time_array_validation(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Compile-Time Execution - Array Size Validation";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compile-time array size validation", 
              "Testing array size validation using compile-time function");
    
    const TChar* source = 
        "module \"test\";\n"
        "%compileTime var MAX_SIZE = 100;\n"
        "%compileTime var MIN_SIZE = 1;\n"
        "%compileTime validateArraySize(size: int): bool {\n"
        "    return size >= MIN_SIZE && size <= MAX_SIZE;\n"
        "}\n"
        "var validatedArray: int[validateArraySize(50) ? 50 : 10];\n"
        "%test(\"test\") {\n"
        "    return validatedArray.length;\n"
        "}\n";
    
    SZrString* sourceName = ZrStringCreate(state, "test_compile_time_array.zr", 28);
    SZrAstNode* ast = ZrParserParse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrCompilerCompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParserFreeAst(state, ast);
        destroyTestState(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 50, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrCompileResultFree(state, &compileResult);
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 主函数
int main(void) {
    UNITY_BEGIN();
    
    TEST_MODULE_DIVIDER();
    printf("Compile-Time Execution Tests\n");
    TEST_MODULE_DIVIDER();
    
    RUN_TEST(test_compile_time_variables);
    RUN_TEST(test_compile_time_functions);
    RUN_TEST(test_compile_time_expressions);
    RUN_TEST(test_compile_time_recursion);
    RUN_TEST(test_compile_time_statements);
    RUN_TEST(test_compile_time_array_validation);
    
    return UNITY_END();
}
