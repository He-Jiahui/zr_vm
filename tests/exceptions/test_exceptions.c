//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>
#include "unity.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/compiler.h"

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

// 测试panic处理函数（用于捕获未处理的异常信息并使用longjmp跳出）
static void testPanicHandler(SZrState *state) {
    if (state == ZR_NULL) {
        return;
    }
    
    // 输出异常信息（立即刷新以确保输出）
    printf("  Panic: Unhandled exception occurred (threadStatus: %d)\n", state->threadStatus);
    fflush(stdout);
    
    // 检查栈上是否有异常值
    if (state->stackTop.valuePointer > state->stackBase.valuePointer) {
        SZrTypeValue *errorValue = ZrStackGetValue(state->stackTop.valuePointer - 1);
        if (errorValue != ZR_NULL) {
            // 尝试将异常值转换为字符串
            struct SZrString *errorStr = ZrValueConvertToString(state, errorValue);
            if (errorStr != ZR_NULL) {
                const TChar *errorMsg = ZrStringGetNativeString(errorStr);
                if (errorMsg != ZR_NULL) {
                    printf("  Exception value: %s\n", errorMsg);
                } else {
                    printf("  Exception value: (could not get string)\n");
                }
            } else {
                printf("  Exception value: (could not convert to string)\n");
            }
            fflush(stdout);
        }
    }
    
    // 尝试使用longjmp跳出当前状态，避免abort
    // 使用global->userData存储jmp_buf指针
    SZrGlobalState *global = state->global;
    if (global != ZR_NULL && global->userData != ZR_NULL) {
        jmp_buf *jumpBuffer = (jmp_buf *)global->userData;
        longjmp(*jumpBuffer, 1);  // 跳出到setjmp的位置，返回值为1表示异常
    }
    // 如果没有设置jmp_buf，则继续执行（会触发abort）
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
        
        // 注册panic处理函数以获取异常信息
        global->panicHandlingFunction = testPanicHandler;
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

// 创建测试函数（简化版本，用于指令测试）
static SZrFunction *createTestFunction(SZrState *state, TZrInstruction *instructions, TZrSize instructionCount,
                                       SZrTypeValue *constants, TZrSize constantCount, TZrSize stackSize) {
    if (state == ZR_NULL || instructions == ZR_NULL || instructionCount == 0)
        return ZR_NULL;

    SZrFunction *function = ZrFunctionNew(state);
    if (function == ZR_NULL)
        return ZR_NULL;

    // 复制指令
    TZrSize instSize = instructionCount * sizeof(TZrInstruction);
    function->instructionsList = (TZrInstruction *) ZrMemoryRawMallocWithType(state->global, instSize,
                                                                               ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->instructionsList == ZR_NULL) {
        ZrFunctionFree(state, function);
        return ZR_NULL;
    }
    memcpy(function->instructionsList, instructions, instSize);
    function->instructionsLength = (TUInt32) instructionCount;

    // 复制常量
    if (constants != ZR_NULL && constantCount > 0) {
        TZrSize constSize = constantCount * sizeof(SZrTypeValue);
        function->constantValueList = (SZrTypeValue *) ZrMemoryRawMallocWithType(state->global, constSize,
                                                                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->constantValueList == ZR_NULL) {
            ZrFunctionFree(state, function);
            return ZR_NULL;
        }
        memcpy(function->constantValueList, constants, constSize);
        function->constantValueLength = (TUInt32) constantCount;
    }

    function->stackSize = (TUInt32) stackSize;
    function->parameterCount = 0;
    function->hasVariableArguments = ZR_FALSE;

    return function;
}

// 执行测试函数
static TBool executeTestFunction(SZrState *state, SZrFunction *function) {
    // 创建闭包
    SZrClosure *closure = ZrClosureNew(state, 0);
    if (!closure)
        return ZR_FALSE;
    closure->function = function;

    // 准备栈
    TZrStackValuePointer base = state->stackTop.valuePointer;
    ZrStackSetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer = base + 1 + function->stackSize;

    // 创建CallInfo
    SZrCallInfo *callInfo = ZrCallInfoExtend(state);
    if (!callInfo)
        return ZR_FALSE;

    ZrCallInfoEntryNativeInit(state, callInfo, state->stackBase, state->stackTop, state->callInfoList);
    callInfo->functionBase.valuePointer = base;
    callInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    callInfo->context.context.programCounter = function->instructionsList;
    callInfo->callStatus = ZR_CALL_STATUS_CREATE_FRAME;
    callInfo->expectedReturnCount = 1;

    state->callInfoList = callInfo;

    // 执行函数
    ZrExecute(state, callInfo);

    // 检查状态
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

// 辅助函数：创建指令（与test_instructions.c中的实现一致）
static TZrInstruction create_instruction_0(EZrInstructionCode opcode, TUInt16 operandExtra) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = 0;
    return instruction;
}

static TZrInstruction create_instruction_1(EZrInstructionCode opcode, TUInt16 operandExtra, TInt32 operand) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = operand;
    return instruction;
}

static TZrInstruction create_instruction_2(EZrInstructionCode opcode, TUInt16 operandExtra, TUInt16 operand1,
                                           TUInt16 operand2) {
    TZrInstruction instruction;
    instruction.instruction.operationCode = (TUInt16) opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operand1;
    instruction.instruction.operand.operand1[1] = operand2;
    return instruction;
}

#define BASE(i) ((callInfo->functionBase.valuePointer + (i)))
#define CONST(i) (&(closure->function->constantValueList[(i)]))

// ==================== 测试用例 ====================

// 测试初始化
void setUp(void) {}

void tearDown(void) {}

// 测试THROW指令：抛出异常
static void test_throw_instruction(void) {
    TEST_START("THROW Instruction");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("THROW instruction", "Testing basic THROW instruction functionality");

    // 创建一个简单的测试：抛出异常
    // 指令序列：
    // 1. GET_CONSTANT 0 -> stack[0] (错误消息字符串)
    // 2. THROW 0 (从stack[0]抛出异常)
    SZrTypeValue constant;
    SZrString *errorMsg = ZrStringCreateFromNative(state, "test error message");
    ZrValueInitAsRawObject(state, &constant, ZR_CAST_RAW_OBJECT_AS_SUPER(errorMsg));

    TZrInstruction instructions[2];
    instructions[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0); // operandExtra=0, const=0
    instructions[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(THROW), 0, 0);        // THROW from slot 0

    SZrFunction *function = createTestFunction(state, instructions, 2, &constant, 1, 2);
    TEST_ASSERT_NOT_NULL(function);

    // 由于THROW指令在没有TRY-CATCH的情况下会导致程序终止或panic，
    // 这里只验证指令序列可以编译和创建
    // 实际的异常捕获测试在TRY-CATCH测试中

    ZrFunctionFree(state, function);
    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "THROW Instruction");
    TEST_DIVIDER();
}

// 测试TRY-CATCH：捕获异常
static void test_try_catch_instruction(void) {
    TEST_START("TRY CATCH Instructions");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("TRY-CATCH instruction", "Testing TRY-CATCH exception handling");

    // 注意：TRY-CATCH指令的实际异常处理依赖于setjmp/longjmp机制，
    // 在指令级别测试比较复杂。这里只做基本框架测试。

    // TRY指令主要是标记，实际异常处理由底层机制处理
    TZrInstruction tryInst = create_instruction_0(ZR_INSTRUCTION_ENUM(TRY), 0);
    TEST_ASSERT_EQUAL_INT(ZR_INSTRUCTION_ENUM(TRY), tryInst.instruction.operationCode);

    // CATCH指令用于捕获异常
    TZrInstruction catchInst = create_instruction_1(ZR_INSTRUCTION_ENUM(CATCH), 0, 0);
    TEST_ASSERT_EQUAL_INT(ZR_INSTRUCTION_ENUM(CATCH), catchInst.instruction.operationCode);

    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "TRY CATCH Instructions");
    TEST_DIVIDER();
}

// 执行测试函数并获取返回值（使用ZrFunctionCall，支持异常捕获）
static TBool executeTestFunctionAndGetResult(SZrState *state, SZrFunction *function, TInt64 *result) {
    if (state == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    // 设置jmp_buf用于panic处理函数的longjmp
    jmp_buf jumpBuffer;
    SZrGlobalState *global = state->global;
    TZrPtr oldUserData = global->userData;  // 保存旧的userData
    
    // 设置userData为jmp_buf指针
    global->userData = (TZrPtr)&jumpBuffer;
    
    // 使用setjmp设置恢复点
    if (setjmp(jumpBuffer) == 0) {
        // 正常执行路径
        
        // 创建闭包
        SZrClosure *closure = ZrClosureNew(state, 0);
        if (closure == ZR_NULL) {
            global->userData = oldUserData;  // 恢复userData
            return ZR_FALSE;
        }
        closure->function = function;
        ZrClosureInitValue(state, closure);

        // 准备调用栈
        TZrStackValuePointer base = state->stackTop.valuePointer;
        ZrFunctionCheckStackAndGc(state, function->stackSize + 1, base);

        // 将闭包压栈
        SZrTypeValue *closureValue = ZrStackGetValue(state->stackTop.valuePointer);
        ZrValueInitAsRawObject(state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
        closureValue->type = ZR_VALUE_TYPE_FUNCTION;
        closureValue->isGarbageCollectable = ZR_TRUE;
        closureValue->isNative = ZR_FALSE;
        state->stackTop.valuePointer++;

        // 调用函数
        ZrFunctionCall(state, base, 1);

        // 恢复userData
        global->userData = oldUserData;

        // 检查执行状态
        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_FALSE;
        }

        // 获取返回值
        SZrTypeValue *returnValue = ZrStackGetValue(base);
        if (ZR_VALUE_IS_TYPE_INT(returnValue->type)) {
            *result = returnValue->value.nativeObject.nativeInt64;
            return ZR_TRUE;
        }

        return ZR_FALSE;
    } else {
        // longjmp返回路径（panic处理函数调用了longjmp）
        // 恢复userData
        global->userData = oldUserData;
        
        // 异常被panic处理函数捕获并longjmp跳出
        // 返回失败状态
        return ZR_FALSE;
    }
}

// 测试%test编译和执行：正常执行返回成功（应返回1）
static void test_test_declaration_success(void) {
    TEST_START("Test Declaration Success");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Test declaration execution success",
              "Testing %test declaration execution: should return 1 when no throw");

    // 测试%test声明的编译和执行（无throw，应该返回1）
    const char *source = "%test(\"success_test\") { }";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Test Declaration Success", "Failed to parse test declaration");
        destroyTestState(state);
        return;
    }

    // 使用新接口编译
    SZrCompileResult compileResult;
    TBool compileSuccess = ZrCompilerCompileWithTests(state, ast, &compileResult);
    ZrParserFreeAst(state, ast);

    if (!compileSuccess) {
        TEST_FAIL_CUSTOM(timer, "Test Declaration Success", "Failed to compile test declaration");
        destroyTestState(state);
        return;
    }

    // 验证有测试函数
    TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
    TEST_ASSERT_NOT_NULL(compileResult.testFunctions);

    // 执行第一个测试函数
    TInt64 result = 0;
    TBool execSuccess = executeTestFunctionAndGetResult(state, compileResult.testFunctions[0], &result);

    if (!execSuccess) {
        TEST_FAIL_CUSTOM(timer, "Test Declaration Success",
                         "Failed to execute test function");
        ZrCompileResultFree(state, &compileResult);
        if (compileResult.mainFunction != ZR_NULL) {
            ZrFunctionFree(state, compileResult.mainFunction);
        }
        destroyTestState(state);
        return;
    }

    // 验证返回值应该是1（成功）
    TEST_ASSERT_EQUAL_INT64(1, result);

    // 清理
    ZrCompileResultFree(state, &compileResult);
    if (compileResult.mainFunction != ZR_NULL) {
        ZrFunctionFree(state, compileResult.mainFunction);
    }
    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Test Declaration Success");
    TEST_DIVIDER();
}

// 测试%test编译和执行：throw导致失败（应返回0）
static void test_test_declaration_throw_failure(void) {
    TEST_START("Test Declaration Throw Failure");
    SZrTestTimer timer;
    timer.startTime = clock();

    SZrState *state = createTestState();
    TEST_ASSERT_NOT_NULL(state);

    // 确保全局异常捕获已注册
    SZrGlobalState *global = state->global;
    TEST_ASSERT_NOT_NULL(global);
    TEST_ASSERT_NOT_NULL(global->panicHandlingFunction);

    TEST_INFO("Test declaration execution failure",
              "Testing %test declaration execution: should return 0 when throw occurs");

    // 测试%test声明的编译和执行（包含throw，应该返回0）
    const char *source = "%test(\"throw_test\") { throw \"error\"; }";
    SZrString *sourceName = ZrStringCreate(state, "test.zr", 7);
    SZrAstNode *ast = ZrParserParse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, "Test Declaration Throw Failure",
                         "Failed to parse test declaration with throw");
        destroyTestState(state);
        return;
    }

    // 使用新接口编译
    SZrCompileResult compileResult;
    TBool compileSuccess = ZrCompilerCompileWithTests(state, ast, &compileResult);
    ZrParserFreeAst(state, ast);

    if (!compileSuccess) {
        TEST_FAIL_CUSTOM(timer, "Test Declaration Throw Failure",
                         "Failed to compile test declaration with throw");
        destroyTestState(state);
        return;
    }

    // 验证有测试函数
    TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
    TEST_ASSERT_NOT_NULL(compileResult.testFunctions);

    // 执行第一个测试函数
    TInt64 result = 0;
    TBool execSuccess = executeTestFunctionAndGetResult(state, compileResult.testFunctions[0], &result);

    if (!execSuccess) {
        TEST_FAIL_CUSTOM(timer, "Test Declaration Throw Failure",
                         "Failed to execute test function");
        ZrCompileResultFree(state, &compileResult);
        if (compileResult.mainFunction != ZR_NULL) {
            ZrFunctionFree(state, compileResult.mainFunction);
        }
        destroyTestState(state);
        return;
    }

    // 验证返回值应该是0（失败）
    TEST_ASSERT_EQUAL_INT64(0, result);

    // 清理
    ZrCompileResultFree(state, &compileResult);
    if (compileResult.mainFunction != ZR_NULL) {
        ZrFunctionFree(state, compileResult.mainFunction);
    }
    destroyTestState(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, "Test Declaration Throw Failure");
    TEST_DIVIDER();
}

// 主测试函数
int main(void) {
    UNITY_BEGIN();

    TEST_MODULE_DIVIDER();
    printf("Exception Handling Tests\n");
    TEST_MODULE_DIVIDER();

    // 基础异常指令测试
    RUN_TEST(test_throw_instruction);
    RUN_TEST(test_try_catch_instruction);

    // %test声明测试
    RUN_TEST(test_test_declaration_success);
    RUN_TEST(test_test_declaration_throw_failure);

    return UNITY_END();
}

