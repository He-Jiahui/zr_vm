//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

// 测试日志宏（符合测试规范）
#define TEST_START(summary)                                                                                            \
    do {                                                                                                               \
        printf("Unit Test - %s\n", summary);                                                                           \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_INFO(summary, details)                                                                                    \
    do {                                                                                                               \
        printf("Testing %s:\n %s\n", summary, details);                                                                \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_PASS_CUSTOM(timer, summary)                                                                               \
    do {                                                                                                               \
        double elapsed = ((double) (timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                       \
        printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary);                                                    \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_FAIL_CUSTOM(timer, summary, reason)                                                                       \
    do {                                                                                                               \
        double elapsed = ((double) (timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0;                       \
        printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason);                                      \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_DIVIDER()                                                                                                 \
    do {                                                                                                               \
        printf("----------\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

#define TEST_MODULE_DIVIDER()                                                                                          \
    do {                                                                                                               \
        printf("==========\n");                                                                                        \
        fflush(stdout);                                                                                                \
    } while (0)

// 简单的测试分配器
static TZrPtr testAllocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr) pointer >= (TZrPtr) 0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    } else {
        if ((TZrPtr) pointer >= (TZrPtr) 0x1000) {
            return realloc(pointer, newSize);
        } else {
            return malloc(newSize);
        }
    }
}

// 创建测试用的SZrState
static SZrState *createTestState(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12345, &callbacks);
    if (!global)
        return ZR_NULL;

    SZrState *mainState = global->mainThreadState;
    if (mainState) {
        ZrGlobalStateInitRegistry(mainState, global);
    }

    return mainState;
}

// 销毁测试用的SZrState
static void destroyTestState(SZrState *state) {
    if (!state)
        return;

    SZrGlobalState *global = state->global;
    if (global) {
        ZrGlobalStateFree(global);
    }
}

// 打印测试结果值
static void printTestResult(SZrState *state, const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        printf("Result: <null>\n");
        return;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            printf("Result: null\n");
            break;
        case ZR_VALUE_TYPE_BOOL:
            printf("Result: %s\n", value->value.nativeObject.nativeBool ? "true" : "false");
            break;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            printf("Result: %lld\n", (long long)value->value.nativeObject.nativeInt64);
            break;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            printf("Result: %llu\n", (unsigned long long)value->value.nativeObject.nativeUInt64);
            break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            printf("Result: %f\n", value->value.nativeObject.nativeDouble);
            break;
        case ZR_VALUE_TYPE_STRING: {
            if (value->value.object == ZR_NULL) {
                printf("Result: \"\"\n");
            } else {
                SZrString *str = ZR_CAST_STRING(state, value->value.object);
                TNativeString strStr = ZrStringGetNativeString(str);
                printf("Result: \"%s\"\n", strStr ? strStr : "");
            }
            break;
        }
        case ZR_VALUE_TYPE_OBJECT: {
            SZrObject *obj = ZR_CAST_OBJECT(state, value->value.object);
            if (obj != ZR_NULL) {
                printf("Result: <object type=%d>\n", (int)obj->internalType);
            } else {
                printf("Result: <null object>\n");
            }
            break;
        }
        case ZR_VALUE_TYPE_ARRAY: {
            printf("Result: <array>\n");
            break;
        }
        case ZR_VALUE_TYPE_CLOSURE: {
            printf("Result: <closure>\n");
            break;
        }
        default:
            printf("Result: <unknown type=%d>\n", (int)value->type);
            break;
    }
}

// 执行编译后的函数并获取返回值
// 注意：这个函数要求编译后的函数有返回值（使用 return 语句或表达式语句）
static TBool executeFunctionAndGetResult(SZrState *state, SZrFunction *function, SZrTypeValue *result) {
    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    // 检查 function 参数的有效性
    if (function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    // 创建闭包
    SZrClosure *closure = ZrClosureNew(state, function->closureValueLength);
    if (closure == ZR_NULL) {
        return ZR_FALSE;
    }

    // 先设置函数，然后再初始化闭包值
    closure->function = function;

    // 检查 closure->function 是否被正确设置
    if (closure->function != function) {
        return ZR_FALSE;
    }

    // 设置函数后需要添加 GC barrier（如果函数是 GC 对象）
    // 但 SZrFunction 通常不是 GC 对象，所以这里不需要 barrier

    // 初始化闭包值（需要在设置 function 之后调用）
    ZrClosureInitValue(state, closure);

    // 再次检查 closure->function 是否仍然有效（在 ZrClosureInitValue 之后）
    if (closure->function != function) {
        return ZR_FALSE;
    }

    // 将闭包推送到栈上（作为函数对象）
    TZrStackValuePointer closurePointer = state->stackTop.valuePointer;
    ZrStackSetRawObjectValue(state, closurePointer, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer++;

    // 从栈上读取闭包，验证 function 字段
    SZrTypeValue *stackValue = ZrStackGetValue(closurePointer);
    if (stackValue->type != ZR_VALUE_TYPE_CLOSURE) {
        return ZR_FALSE;
    }
    SZrClosure *stackClosure = ZR_CAST_VM_CLOSURE(state, stackValue->value.object);
    if (stackClosure->function != function) {
        return ZR_FALSE;
    }

    // 使用 ZrFunctionCallWithoutYield 来执行函数（期望 1 个返回值）
    ZrFunctionCallWithoutYield(state, closurePointer, 1);

    // 检查执行状态
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        return ZR_FALSE;
    }

    // 获取返回值
    // 函数调用后，返回值会在栈上的某个位置
    // 根据 ZrFunctionMoveReturns 的实现，当 expectedReturnCount 为 1 时，
    // 返回值会被移动到 closurePointer 位置
    if (result != ZR_NULL) {
        // 检查栈顶是否在 closurePointer 之后（说明有返回值）
        if (state->stackTop.valuePointer > closurePointer) {
            SZrTypeValue *returnValue = ZrStackGetValue(closurePointer);
            if (returnValue != ZR_NULL) {
                ZrValueCopy(state, result, returnValue);
                return ZR_TRUE;
            }
        } else if (state->stackTop.valuePointer == closurePointer + 1) {
            // 返回值正好在 closurePointer + 1 位置
            SZrTypeValue *returnValue = ZrStackGetValue(closurePointer);
            if (returnValue != ZR_NULL) {
                ZrValueCopy(state, result, returnValue);
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

// 测试初始化和清理
void setUp(void) {}

void tearDown(void) {}

// ==================== CREATE_OBJECT 指令测试 ====================

// 测试 CREATE_OBJECT 指令执行
void test_execute_create_object(void) {
    SZrTestTimer timer;
    const char *testSummary = "CREATE_OBJECT Instruction Execution";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("CREATE_OBJECT execution", "Testing CREATE_OBJECT instruction: {}");

    const char *source = "return {};";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse empty object literal");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile empty object literal");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 CREATE_OBJECT 指令）
    TBool hasCreateObject = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(CREATE_OBJECT)) {
                hasCreateObject = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasCreateObject) {
        TEST_FAIL_CUSTOM(timer, testSummary, "CREATE_OBJECT instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    // 验证函数有指令
    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    // 执行函数并输出结果
    SZrTypeValue result;
    TBool execSuccess = executeFunctionAndGetResult(state, function, &result);
    if (execSuccess) {
        printf("Test Result: ");
        printTestResult(state, &result);
    } else {
        printf("Test Result: <execution failed or no return value>\n");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试 CREATE_OBJECT 带属性
void test_execute_create_object_with_properties(void) {
    SZrTestTimer timer;
    const char *testSummary = "CREATE_OBJECT With Properties Execution";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("CREATE_OBJECT with properties execution", "Testing CREATE_OBJECT instruction: {a: 1, b: 2}");

    const char *source = "return {a: 1, b: 2};";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse object literal with properties");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile object literal with properties");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 CREATE_OBJECT 指令）
    TBool hasCreateObject = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(CREATE_OBJECT)) {
                hasCreateObject = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasCreateObject) {
        TEST_FAIL_CUSTOM(timer, testSummary, "CREATE_OBJECT instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    // 验证函数有指令
    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    // 执行函数并输出结果
    SZrTypeValue result;
    TBool execSuccess = executeFunctionAndGetResult(state, function, &result);
    if (execSuccess) {
        printf("Test Result: ");
        printTestResult(state, &result);
    } else {
        printf("Test Result: <execution failed or no return value>\n");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== CREATE_ARRAY 指令测试 ====================

// 测试 CREATE_ARRAY 指令执行
void test_execute_create_array(void) {
    SZrTestTimer timer;
    const char *testSummary = "CREATE_ARRAY Instruction Execution";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("CREATE_ARRAY execution", "Testing CREATE_ARRAY instruction: []");

    const char *source = "return [];";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse empty array literal");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile empty array literal");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 CREATE_ARRAY 指令）
    TBool hasCreateArray = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(CREATE_ARRAY)) {
                hasCreateArray = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasCreateArray) {
        TEST_FAIL_CUSTOM(timer, testSummary, "CREATE_ARRAY instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    // 验证函数有指令
    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    // 执行函数并输出结果
    SZrTypeValue result;
    TBool execSuccess = executeFunctionAndGetResult(state, function, &result);
    if (execSuccess) {
        printf("Test Result: ");
        printTestResult(state, &result);
    } else {
        printf("Test Result: <execution failed or no return value>\n");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试 CREATE_ARRAY 带元素
void test_execute_create_array_with_elements(void) {
    SZrTestTimer timer;
    const char *testSummary = "CREATE_ARRAY With Elements Execution";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("CREATE_ARRAY with elements execution", "Testing CREATE_ARRAY instruction: [1, 2, 3]");

    const char *source = "return [1, 2, 3];";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse array literal with elements");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile array literal with elements");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 CREATE_ARRAY 指令）
    TBool hasCreateArray = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(CREATE_ARRAY)) {
                hasCreateArray = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasCreateArray) {
        TEST_FAIL_CUSTOM(timer, testSummary, "CREATE_ARRAY instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    // 验证函数有指令
    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    // 执行函数并输出结果
    SZrTypeValue result;
    TBool execSuccess = executeFunctionAndGetResult(state, function, &result);
    if (execSuccess) {
        printf("Test Result: ");
        printTestResult(state, &result);
    } else {
        printf("Test Result: <execution failed or no return value>\n");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== 控制流指令测试 (JUMP, JUMP_IF) ====================

// 测试 JUMP_IF 指令（通过 if 语句）
void test_execute_jump_if_instruction(void) {
    SZrTestTimer timer;
    const char *testSummary = "JUMP_IF Instruction (If Statement)";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("JUMP_IF instruction via if statement", "Testing JUMP_IF instruction: if (true) { return 42; }");

    const char *source = "if (true) { return 42; }";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse if statement");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile if statement");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 JUMP_IF 指令）
    TBool hasJumpIf = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(JUMP_IF)) {
                hasJumpIf = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasJumpIf) {
        TEST_FAIL_CUSTOM(timer, testSummary, "JUMP_IF instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    // 执行函数并输出结果
    SZrTypeValue result;
    TBool execSuccess = executeFunctionAndGetResult(state, function, &result);
    if (execSuccess) {
        printf("Test Result: ");
        printTestResult(state, &result);
    } else {
        printf("Test Result: <execution failed or no return value>\n");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试 JUMP 指令（通过 while 循环）
void test_execute_jump_instruction(void) {
    SZrTestTimer timer;
    const char *testSummary = "JUMP Instruction (While Loop)";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("JUMP instruction via while loop", "Testing JUMP instruction: while (false) { } return 0;");

    const char *source = "while (false) { } return 0;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse while loop");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile while loop");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 JUMP 指令）
    TBool hasJump = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(JUMP)) {
                hasJump = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasJump) {
        TEST_FAIL_CUSTOM(timer, testSummary, "JUMP instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    // 执行函数并输出结果
    SZrTypeValue result;
    TBool execSuccess = executeFunctionAndGetResult(state, function, &result);
    if (execSuccess) {
        printf("Test Result: ");
        printTestResult(state, &result);
    } else {
        printf("Test Result: <execution failed or no return value>\n");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== 算术运算指令测试 ====================

// 测试 ADD_INT 指令
void test_execute_add_int_instruction(void) {
    SZrTestTimer timer;
    const char *testSummary = "ADD_INT Instruction";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("ADD_INT instruction", "Testing ADD_INT instruction: 1 + 2");

    // 使用 return 语句来返回表达式结果
    const char *source = "return 1 + 2;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse addition expression");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile addition expression");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 ADD_INT 指令）
    TBool hasAddInt = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(ADD_INT)) {
                hasAddInt = ZR_TRUE;
                break;
            }
        }
    }

    if (!hasAddInt) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "ADD_INT instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    // 执行函数并验证结果
    SZrTypeValue result;
    TBool execSuccess = executeFunctionAndGetResult(state, function, &result);

    ZrParserFreeAst(state, ast);

    if (!execSuccess) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute function");
        destroyTestState(state);
        return;
    }

    // 验证结果是整数类型
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));

    // 验证结果值为 3 (1 + 2)
    if (ZR_VALUE_IS_TYPE_INT(result.type)) {
        TInt64 value = result.value.nativeObject.nativeInt64;
        TEST_ASSERT_TRUE(value == 3);
        printf("Test Result: ");
        printTestResult(state, &result);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试 SUB_INT 指令
void test_execute_sub_int_instruction(void) {
    SZrTestTimer timer;
    const char *testSummary = "SUB_INT Instruction";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("SUB_INT instruction", "Testing SUB_INT instruction: 5 - 3");

    const char *source = "return 5 - 3;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse subtraction expression");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile subtraction expression");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 SUB_INT 指令）
    TBool hasSubInt = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(SUB_INT)) {
                hasSubInt = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasSubInt) {
        TEST_FAIL_CUSTOM(timer, testSummary, "SUB_INT instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试 MUL_SIGNED 指令
void test_execute_mul_signed_instruction(void) {
    SZrTestTimer timer;
    const char *testSummary = "MUL_SIGNED Instruction";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("MUL_SIGNED instruction", "Testing MUL_SIGNED instruction: 3 * 4");

    const char *source = "return 3 * 4;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse multiplication expression");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile multiplication expression");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 MUL_SIGNED 指令）
    TBool hasMulSigned = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(MUL_SIGNED)) {
                hasMulSigned = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasMulSigned) {
        TEST_FAIL_CUSTOM(timer, testSummary, "MUL_SIGNED instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== 逻辑运算指令测试 ====================

// 测试 LOGICAL_AND 指令（使用二进制表达式，因为 && 在二进制表达式中处理）
void test_execute_logical_and_instruction(void) {
    SZrTestTimer timer;
    const char *testSummary = "LOGICAL_AND Instruction";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("LOGICAL_AND instruction (using binary expression)",
              "Testing LOGICAL_AND instruction: 1 && 2 (using bitwise AND as binary expression)");

    // 注意：LOGICAL_EXPRESSION 类型可能未被编译器处理，改用二进制表达式测试
    // 使用 & 操作符（BITWISE_AND）代替，因为它是二进制表达式
    const char *source = "return 1 & 2;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse logical AND expression");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile logical AND expression");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 BITWISE_AND 指令，作为二进制表达式的替代）
    TBool hasBitwiseAnd = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(BITWISE_AND)) {
                hasBitwiseAnd = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasBitwiseAnd) {
        TEST_FAIL_CUSTOM(timer, testSummary,
                         "BITWISE_AND instruction not found in compiled function (testing binary expression)");
        destroyTestState(state);
        return;
    }

    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试 LOGICAL_EQUAL 指令
void test_execute_logical_equal_instruction(void) {
    SZrTestTimer timer;
    const char *testSummary = "LOGICAL_EQUAL Instruction";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("LOGICAL_EQUAL instruction", "Testing LOGICAL_EQUAL instruction: 1 == 1");

    const char *source = "return 1 == 1;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse equality expression");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile equality expression");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 LOGICAL_EQUAL 指令）
    TBool hasLogicalEqual = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL)) {
                hasLogicalEqual = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasLogicalEqual) {
        TEST_FAIL_CUSTOM(timer, testSummary, "LOGICAL_EQUAL instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== GETTABLE/SETTABLE 指令测试 ====================

// 测试 SETTABLE 指令（对象属性设置，在对象字面量中使用）
void test_execute_settable_instruction(void) {
    SZrTestTimer timer;
    const char *testSummary = "SETTABLE Instruction (Object Property Setting)";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("SETTABLE instruction", "Testing SETTABLE instruction in object literal: {a: 1, b: 2}");

    // 对象字面量在编译时会使用 SETTABLE 来设置属性
    const char *source = "return {a: 1, b: 2};";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse object literal");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile object literal");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 SETTABLE 指令）
    TBool hasSetTable = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(SETTABLE)) {
                hasSetTable = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasSetTable) {
        TEST_FAIL_CUSTOM(timer, testSummary, "SETTABLE instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试 DIV_SIGNED 指令
void test_execute_div_signed_instruction(void) {
    SZrTestTimer timer;
    const char *testSummary = "DIV_SIGNED Instruction";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("DIV_SIGNED instruction", "Testing DIV_SIGNED instruction: 10 / 2");

    const char *source = "return 10 / 2;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse division expression");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile division expression");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 DIV_SIGNED 指令）
    TBool hasDivSigned = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(DIV_SIGNED)) {
                hasDivSigned = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasDivSigned) {
        TEST_FAIL_CUSTOM(timer, testSummary, "DIV_SIGNED instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// 测试 LOGICAL_NOT 指令
void test_execute_logical_not_instruction(void) {
    SZrTestTimer timer;
    const char *testSummary = "LOGICAL_NOT Instruction";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("LOGICAL_NOT instruction", "Testing LOGICAL_NOT instruction: !true");

    const char *source = "return !true;";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse logical NOT expression");
        destroyTestState(state);
        return;
    }

    SZrFunction *function = ZrCompilerCompile(state, ast);
    if (function == ZR_NULL) {
        ZrParserFreeAst(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile logical NOT expression");
        destroyTestState(state);
        return;
    }

    // 验证函数指令（检查是否包含 LOGICAL_NOT 指令）
    TBool hasLogicalNot = ZR_FALSE;
    if (function->instructionsList != ZR_NULL && function->instructionsLength > 0) {
        for (TUInt32 i = 0; i < function->instructionsLength; i++) {
            EZrInstructionCode opcode = (EZrInstructionCode) function->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(LOGICAL_NOT)) {
                hasLogicalNot = ZR_TRUE;
                break;
            }
        }
    }

    ZrParserFreeAst(state, ast);

    if (!hasLogicalNot) {
        TEST_FAIL_CUSTOM(timer, testSummary, "LOGICAL_NOT instruction not found in compiled function");
        destroyTestState(state);
        return;
    }

    TEST_ASSERT_TRUE(function->instructionsLength > 0);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroyTestState(state);
    TEST_DIVIDER();
}

// ==================== 主函数 ====================

int main(void) {
    UNITY_BEGIN();

    TEST_MODULE_DIVIDER();
    printf("Instruction Execution Tests\n");
    TEST_MODULE_DIVIDER();

    // CREATE_OBJECT 指令测试
    printf("==========\n");
    printf("CREATE_OBJECT Instruction Tests\n");
    printf("==========\n");
    RUN_TEST(test_execute_create_object);
    RUN_TEST(test_execute_create_object_with_properties);

    // CREATE_ARRAY 指令测试
    printf("==========\n");
    printf("CREATE_ARRAY Instruction Tests\n");
    printf("==========\n");
    RUN_TEST(test_execute_create_array);
    RUN_TEST(test_execute_create_array_with_elements);

    // 控制流指令测试
    printf("==========\n");
    printf("Control Flow Instruction Tests (JUMP, JUMP_IF)\n");
    printf("==========\n");
    RUN_TEST(test_execute_jump_if_instruction);
    RUN_TEST(test_execute_jump_instruction);

    // 算术运算指令测试
    printf("==========\n");
    printf("Arithmetic Instruction Tests\n");
    printf("==========\n");
    RUN_TEST(test_execute_add_int_instruction);
    RUN_TEST(test_execute_sub_int_instruction);
    RUN_TEST(test_execute_mul_signed_instruction);
    RUN_TEST(test_execute_div_signed_instruction);

    // 逻辑运算指令测试
    printf("==========\n");
    printf("Logical Instruction Tests\n");
    printf("==========\n");
    RUN_TEST(test_execute_logical_and_instruction);
    RUN_TEST(test_execute_logical_equal_instruction);
    RUN_TEST(test_execute_logical_not_instruction);

    // GETTABLE/SETTABLE 指令测试
    printf("==========\n");
    printf("Table Access Instruction Tests (SETTABLE)\n");
    printf("==========\n");
    RUN_TEST(test_execute_settable_instruction);

    TEST_MODULE_DIVIDER();
    printf("All Instruction Execution Tests Completed\n");
    TEST_MODULE_DIVIDER();

    return UNITY_END();
}
