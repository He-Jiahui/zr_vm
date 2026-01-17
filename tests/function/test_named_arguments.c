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
    
    // 验证闭包值是否正确设置
    if (closureValue->type != ZR_VALUE_TYPE_FUNCTION || closureValue->value.object == ZR_NULL) {
        printf("  [ERROR] Failed to set closure value: type=%d, object=%p\n", closureValue->type, closureValue->value.object);
        return ZR_FALSE;
    }
    
    state->stackTop.valuePointer++;
    
    // 验证栈上的函数值
    SZrTypeValue* funcValueOnStack = ZrStackGetValue(base);
    if (funcValueOnStack->type != ZR_VALUE_TYPE_FUNCTION || funcValueOnStack->value.object == ZR_NULL) {
        printf("  [ERROR] Function value on stack is invalid: type=%d, object=%p\n", funcValueOnStack->type, funcValueOnStack->value.object);
        return ZR_FALSE;
    }
    
    // 调用函数
    ZrFunctionCall(state, base, 1);
    
    // 检查执行状态
    if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
        // 获取返回值（ZrFunctionPostCall 写到 functionBase=base）
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

// ==================== 命名参数测试 ====================

// 测试1: 基本命名参数调用
void test_named_arguments_basic(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Named Arguments - Basic";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Basic named arguments", 
              "Testing function call with named arguments: func(c: 3, a: 1, b: 2)");
    
    const TChar* source = 
        "module \"test\";\n"
        "testBasicNamedArgs(a: int, b: int, c: int): int {\n"
        "    return a + b + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return testBasicNamedArgs(c: 3, a: 1, b: 2);\n"
        "}\n";
    
    SZrString* sourceName = ZrStringCreate(state, "test_named_args.zr", 20);
    
    printf("  [DEBUG] Starting parsing...\n");
    fflush(stdout);
    SZrAstNode* ast = ZrParserParse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroyTestState(state);
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
    
    if (!ZrCompilerCompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParserFreeAst(state, ast);
        destroyTestState(state);
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
    
    ZrCompileResultFree(state, &compileResult);
    ZrParserFreeAst(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试2: 位置参数和命名参数混合
void test_named_arguments_mixed(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Named Arguments - Mixed Positional and Named";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Mixed positional and named arguments", 
              "Testing: func(1, c: 3, b: 2)");
    
    const TChar* source = 
        "module \"test\";\n"
        "testMixedArgs(a: int, b: int, c: int): int {\n"
        "    return a * 100 + b * 10 + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return testMixedArgs(1, c: 3, b: 2);\n"
        "}\n";
    
    SZrString* sourceName = ZrStringCreate(state, "test_mixed_args.zr", 20);
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
            if (!execute_test_function(state, testFunc, 123, testSummary)) {
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

// 测试3: 默认参数与命名参数结合
void test_named_arguments_with_defaults(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Named Arguments - With Default Parameters";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Named arguments with default parameters", 
              "Testing: func(5) and func(5, c: 30)");
    
    const TChar* source = 
        "module \"test\";\n"
        "testDefaultArgs(a: int, b: int = 10, c: int = 20): int {\n"
        "    return a + b + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    var r1 = testDefaultArgs(5);\n"
        "    var r2 = testDefaultArgs(5, c: 30);\n"
        "    return r1 + r2;\n"
        "}\n";
    
    SZrString* sourceName = ZrStringCreate(state, "test_default_args.zr", 22);
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
            if (!execute_test_function(state, testFunc, 80, testSummary)) {
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

// 测试4: 命名参数顺序无关
void test_named_arguments_order_independent(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Named Arguments - Order Independent";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Named arguments order independence", 
              "Testing: func(c: 10, a: 5, b: 3)");
    
    const TChar* source = 
        "module \"test\";\n"
        "testNamedOrder(a: int, b: int, c: int): int {\n"
        "    return a - b + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return testNamedOrder(c: 10, a: 5, b: 3);\n"
        "}\n";
    
    SZrString* sourceName = ZrStringCreate(state, "test_order.zr", 14);
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
            if (!execute_test_function(state, testFunc, 12, testSummary)) {
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

// 测试5: 复杂函数调用中的命名参数
void test_named_arguments_complex(void) {
    SZrTestTimer timer;
    const TChar* testSummary = "Named Arguments - Complex Function Call";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = createTestState();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Complex function call with named arguments", 
              "Testing: complexFunction(w: 4, x: 1, z: 3, y: 2)");
    
    const TChar* source = 
        "module \"test\";\n"
        "complexFunction(x: int, y: int, z: int, w: int): int {\n"
        "    return x * 1000 + y * 100 + z * 10 + w;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return complexFunction(w: 4, x: 1, z: 3, y: 2);\n"
        "}\n";
    
    SZrString* sourceName = ZrStringCreate(state, "test_complex.zr", 16);
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
            if (!execute_test_function(state, testFunc, 1234, testSummary)) {
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
    printf("Named Arguments Tests\n");
    TEST_MODULE_DIVIDER();
    
    RUN_TEST(test_named_arguments_basic);
    RUN_TEST(test_named_arguments_mixed);
    RUN_TEST(test_named_arguments_with_defaults);
    RUN_TEST(test_named_arguments_order_independent);
    RUN_TEST(test_named_arguments_complex);
    
    return UNITY_END();
}
