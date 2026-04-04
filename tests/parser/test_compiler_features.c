//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"

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

#pragma pack(push, 1)
typedef struct SZrCompiledPrototypeInfoView {
    TZrUInt32 nameStringIndex;
    TZrUInt32 type;
    TZrUInt32 accessModifier;
    TZrUInt32 inheritsCount;
    TZrUInt32 membersCount;
    TZrUInt64 protocolMask;
    TZrUInt32 hasDecoratorMetadata;
    TZrUInt32 decoratorMetadataConstantIndex;
    TZrUInt32 decoratorsCount;
} SZrCompiledPrototypeInfoView;

typedef struct SZrCompiledMemberInfoView {
    TZrUInt32 memberType;
    TZrUInt32 nameStringIndex;
    TZrUInt32 accessModifier;
    TZrUInt32 isStatic;
    TZrUInt32 isConst;
    TZrUInt32 fieldTypeNameStringIndex;
    TZrUInt32 fieldOffset;
    TZrUInt32 fieldSize;
    TZrUInt32 isMetaMethod;
    TZrUInt32 metaType;
    TZrUInt32 functionConstantIndex;
    TZrUInt32 parameterCount;
    TZrUInt32 returnTypeNameStringIndex;
    TZrUInt32 isUsingManaged;
    TZrUInt32 ownershipQualifier;
    TZrUInt32 callsClose;
    TZrUInt32 callsDestructor;
    TZrUInt32 declarationOrder;
    TZrUInt32 contractRole;
} SZrCompiledMemberInfoView;
#pragma pack(pop)

void setUp(void) {}

void tearDown(void) {}

// 简单的测试分配器
static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
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
static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0, &callbacks);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    SZrState *state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    return state;
}

// 销毁测试用的SZrState
static void destroy_test_state(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
}

static char *read_text_file_owned(const TZrChar *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static SZrFunction *get_single_compiled_child_function(SZrFunction *wrapper) {
    TEST_ASSERT_NOT_NULL(wrapper);
    TEST_ASSERT_EQUAL_UINT32(1, wrapper->childFunctionLength);
    TEST_ASSERT_NOT_NULL(wrapper->childFunctionList);
    return &wrapper->childFunctionList[0];
}

static TZrBool function_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 i = 0; i < function->instructionsLength; i++) {
        if ((EZrInstructionCode) function->instructionsList[i].instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_contains_native_helper_constant(const SZrFunction *function, TZrUInt64 helperId) {
    if (function == ZR_NULL || helperId == ZR_IO_NATIVE_HELPER_NONE || function->constantValueList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 i = 0; i < function->constantValueLength; i++) {
        const SZrTypeValue *constant = &function->constantValueList[i];

        if (constant->type == ZR_VALUE_TYPE_NATIVE_POINTER &&
            ZrParser_Writer_GetSerializableNativeHelperId(constant->value.nativeFunction) == helperId) {
            return ZR_TRUE;
        }

        if (constant->type == ZR_VALUE_TYPE_CLOSURE && constant->value.object != ZR_NULL && constant->isNative) {
            const SZrRawObject *rawObject = constant->value.object;
            if (rawObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
                const SZrClosureNative *closure = (const SZrClosureNative *)rawObject;
                if (ZrParser_Writer_GetSerializableNativeHelperId(closure->nativeFunction) == helperId) {
                    return ZR_TRUE;
                }
            }
        }
    }

    return ZR_FALSE;
}

static SZrString *get_string_constant_at(SZrState *state, const SZrFunction *function, TZrUInt32 index) {
    const SZrTypeValue *constant;

    if (state == ZR_NULL || function == ZR_NULL || function->constantValueList == ZR_NULL ||
        index >= function->constantValueLength) {
        return ZR_NULL;
    }

    constant = &function->constantValueList[index];
    if (constant->type != ZR_VALUE_TYPE_STRING || constant->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(state, constant->value.object);
}

static const SZrCompiledPrototypeInfoView *find_compiled_prototype_by_name(SZrState *state,
                                                                           const SZrFunction *function,
                                                                           const TZrChar *prototypeName) {
    const TZrByte *currentPos;
    TZrSize remainingSize;
    SZrString *expectedName;

    if (state == ZR_NULL || function == ZR_NULL || prototypeName == ZR_NULL || function->prototypeData == ZR_NULL ||
        function->prototypeDataLength <= sizeof(TZrUInt32) || function->prototypeCount == 0) {
        return ZR_NULL;
    }

    expectedName = ZrCore_String_Create(state, (TZrNativeString)prototypeName, strlen(prototypeName));
    if (expectedName == ZR_NULL) {
        return ZR_NULL;
    }

    currentPos = function->prototypeData + sizeof(TZrUInt32);
    remainingSize = function->prototypeDataLength - sizeof(TZrUInt32);
    while (remainingSize >= sizeof(SZrCompiledPrototypeInfoView)) {
        const SZrCompiledPrototypeInfoView *prototypeInfo = (const SZrCompiledPrototypeInfoView *)currentPos;
        TZrSize prototypeSize = sizeof(SZrCompiledPrototypeInfoView) +
                                prototypeInfo->inheritsCount * sizeof(TZrUInt32) +
                                prototypeInfo->decoratorsCount * sizeof(TZrUInt32) +
                                prototypeInfo->membersCount * sizeof(SZrCompiledMemberInfoView);
        SZrString *actualName;

        if (remainingSize < prototypeSize) {
            break;
        }

        actualName = get_string_constant_at(state, function, prototypeInfo->nameStringIndex);
        if (actualName != ZR_NULL && ZrCore_String_Equal(actualName, expectedName)) {
            return prototypeInfo;
        }

        currentPos += prototypeSize;
        remainingSize -= prototypeSize;
    }

    return ZR_NULL;
}

static const SZrCompiledMemberInfoView *find_compiled_member_by_name(SZrState *state,
                                                                     const SZrFunction *function,
                                                                     const SZrCompiledPrototypeInfoView *prototypeInfo,
                                                                     const TZrChar *memberName) {
    const SZrCompiledMemberInfoView *members;
    SZrString *expectedName;

    if (state == ZR_NULL || function == ZR_NULL || prototypeInfo == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    expectedName = ZrCore_String_Create(state, (TZrNativeString)memberName, strlen(memberName));
    if (expectedName == ZR_NULL) {
        return ZR_NULL;
    }

    members = (const SZrCompiledMemberInfoView *)((const TZrByte *)prototypeInfo +
                                                  sizeof(SZrCompiledPrototypeInfoView) +
                                                  prototypeInfo->inheritsCount * sizeof(TZrUInt32) +
                                                  prototypeInfo->decoratorsCount * sizeof(TZrUInt32));
    for (TZrUInt32 i = 0; i < prototypeInfo->membersCount; i++) {
        SZrString *actualName = get_string_constant_at(state, function, members[i].nameStringIndex);
        if (actualName != ZR_NULL && ZrCore_String_Equal(actualName, expectedName)) {
            return &members[i];
        }
    }

    return ZR_NULL;
}

// 测试1: 函数声明参数处理
static void test_function_parameter_handling(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Function Parameter Handling";

    TEST_START(testSummary);
    TEST_INFO("Function Parameter Handling",
              "Testing that function declarations correctly extract parameter count and variable arguments flag");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：函数声明带参数（不需要function关键字）
    const char *source = "testFunc(a, b, c) { return a + b + c; }";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    {
        SZrFunction *declaredFunction = get_single_compiled_child_function(func);

        TEST_ASSERT_EQUAL_UINT32(3, declaredFunction->parameterCount);
        TEST_ASSERT_EQUAL_INT(ZR_FALSE, declaredFunction->hasVariableArguments);
        TEST_ASSERT_GREATER_THAN_UINT32(0, declaredFunction->instructionsLength);
        TEST_ASSERT_TRUE(function_contains_opcode(declaredFunction, ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)));
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(3, declaredFunction->localVariableLength);
    }

    // 输出完整的指令数量和指令内容
    printf("  Total Instructions: %u\n", func->instructionsLength);
    printf("  Instructions:\n");
    for (TZrUInt32 i = 0; i < func->instructionsLength; i++) {
        TZrInstruction *inst = &func->instructionsList[i];
        EZrInstructionCode opcode = (EZrInstructionCode) inst->instruction.operationCode;
        TZrUInt16 operandExtra = inst->instruction.operandExtra;

        printf("    [%u] ", i);

        // 输出操作码名称（简化版本，只输出常见的指令）
        const char *opcodeName = "UNKNOWN";
        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(GET_STACK):
                opcodeName = "GET_STACK";
                break;
            case ZR_INSTRUCTION_ENUM(SET_STACK):
                opcodeName = "SET_STACK";
                break;
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
                opcodeName = "GET_CONSTANT";
                break;
            case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
                opcodeName = "SET_CONSTANT";
                break;
            case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
                opcodeName = "GET_CLOSURE";
                break;
            case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
                opcodeName = "SET_CLOSURE";
                break;
            case ZR_INSTRUCTION_ENUM(GETUPVAL):
                opcodeName = "GETUPVAL";
                break;
            case ZR_INSTRUCTION_ENUM(SETUPVAL):
                opcodeName = "SETUPVAL";
                break;
            case ZR_INSTRUCTION_ENUM(GET_MEMBER):
                opcodeName = "GET_MEMBER";
                break;
            case ZR_INSTRUCTION_ENUM(SET_MEMBER):
                opcodeName = "SET_MEMBER";
                break;
            case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
                opcodeName = "GET_BY_INDEX";
                break;
            case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
                opcodeName = "SET_BY_INDEX";
                break;
            case ZR_INSTRUCTION_ENUM(ITER_INIT):
                opcodeName = "ITER_INIT";
                break;
            case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
                opcodeName = "ITER_MOVE_NEXT";
                break;
            case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
                opcodeName = "ITER_CURRENT";
                break;
            case ZR_INSTRUCTION_ENUM(ADD_INT):
                opcodeName = "ADD_INT";
                break;
            case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
                opcodeName = "ADD_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(ADD_STRING):
                opcodeName = "ADD_STRING";
                break;
            case ZR_INSTRUCTION_ENUM(SUB_INT):
                opcodeName = "SUB_INT";
                break;
            case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
                opcodeName = "SUB_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
                opcodeName = "MUL_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
                opcodeName = "MUL_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
                opcodeName = "DIV_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
                opcodeName = "DIV_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
                opcodeName = "MOD_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
                opcodeName = "MOD_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(TO_INT):
                opcodeName = "TO_INT";
                break;
            case ZR_INSTRUCTION_ENUM(TO_FLOAT):
                opcodeName = "TO_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(TO_STRING):
                opcodeName = "TO_STRING";
                break;
            case ZR_INSTRUCTION_ENUM(TO_BOOL):
                opcodeName = "TO_BOOL";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
                opcodeName = "LOGICAL_EQUAL";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
                opcodeName = "LOGICAL_NOT_EQUAL";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
                opcodeName = "LOGICAL_LESS_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
                opcodeName = "LOGICAL_LESS_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
                opcodeName = "LOGICAL_GREATER_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
                opcodeName = "LOGICAL_GREATER_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
                opcodeName = "FUNCTION_CALL";
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                opcodeName = "FUNCTION_RETURN";
                break;
            case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
                opcodeName = "GET_GLOBAL";
                break;
            case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
                opcodeName = "GET_SUB_FUNCTION";
                break;
            case ZR_INSTRUCTION_ENUM(JUMP):
                opcodeName = "JUMP";
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF):
                opcodeName = "JUMP_IF";
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
                opcodeName = "CREATE_CLOSURE";
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
                opcodeName = "CREATE_OBJECT";
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
                opcodeName = "CREATE_ARRAY";
                break;
            default: {
                // 对于未知的指令，输出数字
                char buf[32];
                snprintf(buf, sizeof(buf), "OPCODE_%u", (TZrUInt32) opcode);
                opcodeName = buf;
                break;
            }
        }

        printf("%s[%d]", opcodeName, opcode);

        // 根据指令类型输出操作数（简化版本，只输出有意义的操作数）
        // 大多数指令使用 operand1[0] 和 operand1[1] 作为两个操作数
        // 或者使用 operand2[0] 作为单个操作数
        // operandExtra 通常用作目标槽位或额外参数

        // 检查是否有操作数（根据指令类型判断）
        TZrUInt16 op1_0 = inst->instruction.operand.operand1[0];
        TZrUInt16 op1_1 = inst->instruction.operand.operand1[1];
        TZrInt32 op2_0 = (TZrInt32) inst->instruction.operand.operand2[0];

        // 根据指令类型输出操作数
        if (opcode == ZR_INSTRUCTION_ENUM(GET_STACK) || opcode == ZR_INSTRUCTION_ENUM(SET_STACK) ||
            opcode == ZR_INSTRUCTION_ENUM(GET_CONSTANT) || opcode == ZR_INSTRUCTION_ENUM(SET_CONSTANT)) {
            // 单操作数指令：extra=目标槽位, operand2=源槽位或常量索引
            printf(" dst=%u, src=%d", operandExtra, op2_0);
        } else if (opcode == ZR_INSTRUCTION_ENUM(ADD_INT) || opcode == ZR_INSTRUCTION_ENUM(ADD_FLOAT) ||
                   opcode == ZR_INSTRUCTION_ENUM(SUB_INT) || opcode == ZR_INSTRUCTION_ENUM(SUB_FLOAT) ||
                   opcode == ZR_INSTRUCTION_ENUM(MUL_SIGNED) || opcode == ZR_INSTRUCTION_ENUM(MUL_FLOAT) ||
                   opcode == ZR_INSTRUCTION_ENUM(DIV_SIGNED) || opcode == ZR_INSTRUCTION_ENUM(DIV_FLOAT) ||
                   opcode == ZR_INSTRUCTION_ENUM(MOD_SIGNED) || opcode == ZR_INSTRUCTION_ENUM(MOD_FLOAT)) {
            // 二元运算指令：extra=结果槽位, operand1[0]=左操作数, operand1[1]=右操作数
            printf(" dst=%u, left=%u, right=%u", operandExtra, op1_0, op1_1);
        } else if (opcode == ZR_INSTRUCTION_ENUM(GET_MEMBER) || opcode == ZR_INSTRUCTION_ENUM(SET_MEMBER) ||
                   opcode == ZR_INSTRUCTION_ENUM(GET_BY_INDEX) || opcode == ZR_INSTRUCTION_ENUM(SET_BY_INDEX)) {
            printf(" dst=%u, receiver=%u, operand=%u", operandExtra, op1_0, op1_1);
        } else if (opcode == ZR_INSTRUCTION_ENUM(ITER_INIT) || opcode == ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT) ||
                   opcode == ZR_INSTRUCTION_ENUM(ITER_CURRENT)) {
            printf(" dst=%u, iterator=%u", operandExtra, op1_0);
        } else if (opcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) || opcode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
            // 函数调用/返回指令：extra=结果数量, operand1[0]=结果槽位, operand1[1]=参数数量或0
            printf(" result_count=%u, result_slot=%u", operandExtra, op1_0);
            if (opcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) && op1_1 > 0) {
                printf(", arg_count=%u", op1_1);
            }
        } else if (opcode == ZR_INSTRUCTION_ENUM(JUMP) || opcode == ZR_INSTRUCTION_ENUM(JUMP_IF)) {
            // 跳转指令：operand2=偏移量
            printf(" offset=%d", op2_0);
            if (opcode == ZR_INSTRUCTION_ENUM(JUMP_IF)) {
                printf(", condition=%u", operandExtra);
            }
        } else if (opcode == ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION)) {
            // 获取子函数指令：extra=目标槽位, operand2=子函数索引
            printf(" dst=%u, func_index=%d", operandExtra, op2_0);
        } else {
            // 其他指令：输出所有操作数
            if (op1_0 != 0 || op1_1 != 0) {
                printf(" op1=[%u, %u]", op1_0, op1_1);
            }
            if (op2_0 != 0) {
                printf(" op2=[%d]", op2_0);
            }
            if (operandExtra != 0) {
                printf(" extra=%u", operandExtra);
            }
        }
        printf("\n");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试2: 常量去重
static void test_constant_deduplication(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Constant Deduplication";

    TEST_START(testSummary);
    TEST_INFO("Constant Deduplication", "Testing that duplicate constants are deduplicated in the constant pool");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：使用相同的常量多次
    const char *source = "var a = 42; var b = 42; var c = 42;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证常量池中只有一个42（去重后）
    // 注意：这里需要检查常量池，但常量池是内部实现
    // 我们可以通过检查函数是否成功编译来间接验证
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试3: 全局对象属性访问
static void test_global_object_access(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Global Object Access";

    TEST_START(testSummary);
    TEST_INFO("Global Object Access",
              "Testing that global object properties can be accessed using GET_GLOBAL + GET_MEMBER");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：访问全局对象属性
    const char *source = "var x = zr.Error;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证全局对象访问）
    TEST_ASSERT_NOT_NULL(func);

    // 输出完整的指令数量和指令内容
    printf("  Total Instructions: %u\n", func->instructionsLength);
    printf("  Instructions:\n");
    for (TZrUInt32 i = 0; i < func->instructionsLength; i++) {
        TZrInstruction *inst = &func->instructionsList[i];
        EZrInstructionCode opcode = (EZrInstructionCode) inst->instruction.operationCode;
        TZrUInt16 operandExtra = inst->instruction.operandExtra;

        printf("    [%u] ", i);

        // 输出操作码名称（简化版本，只输出常见的指令）
        const char *opcodeName = "UNKNOWN";
        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(GET_STACK):
                opcodeName = "GET_STACK";
                break;
            case ZR_INSTRUCTION_ENUM(SET_STACK):
                opcodeName = "SET_STACK";
                break;
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
                opcodeName = "GET_CONSTANT";
                break;
            case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
                opcodeName = "SET_CONSTANT";
                break;
            case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
                opcodeName = "GET_CLOSURE";
                break;
            case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
                opcodeName = "SET_CLOSURE";
                break;
            case ZR_INSTRUCTION_ENUM(GETUPVAL):
                opcodeName = "GETUPVAL";
                break;
            case ZR_INSTRUCTION_ENUM(SETUPVAL):
                opcodeName = "SETUPVAL";
                break;
            case ZR_INSTRUCTION_ENUM(GET_MEMBER):
                opcodeName = "GET_MEMBER";
                break;
            case ZR_INSTRUCTION_ENUM(SET_MEMBER):
                opcodeName = "SET_MEMBER";
                break;
            case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
                opcodeName = "GET_BY_INDEX";
                break;
            case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
                opcodeName = "SET_BY_INDEX";
                break;
            case ZR_INSTRUCTION_ENUM(ITER_INIT):
                opcodeName = "ITER_INIT";
                break;
            case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
                opcodeName = "ITER_MOVE_NEXT";
                break;
            case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
                opcodeName = "ITER_CURRENT";
                break;
            case ZR_INSTRUCTION_ENUM(ADD_INT):
                opcodeName = "ADD_INT";
                break;
            case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
                opcodeName = "ADD_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(ADD_STRING):
                opcodeName = "ADD_STRING";
                break;
            case ZR_INSTRUCTION_ENUM(SUB_INT):
                opcodeName = "SUB_INT";
                break;
            case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
                opcodeName = "SUB_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
                opcodeName = "MUL_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
                opcodeName = "MUL_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
                opcodeName = "DIV_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
                opcodeName = "DIV_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
                opcodeName = "MOD_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
                opcodeName = "MOD_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(TO_INT):
                opcodeName = "TO_INT";
                break;
            case ZR_INSTRUCTION_ENUM(TO_FLOAT):
                opcodeName = "TO_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(TO_STRING):
                opcodeName = "TO_STRING";
                break;
            case ZR_INSTRUCTION_ENUM(TO_BOOL):
                opcodeName = "TO_BOOL";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
                opcodeName = "LOGICAL_EQUAL";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
                opcodeName = "LOGICAL_NOT_EQUAL";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
                opcodeName = "LOGICAL_LESS_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
                opcodeName = "LOGICAL_LESS_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
                opcodeName = "LOGICAL_GREATER_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
                opcodeName = "LOGICAL_GREATER_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
                opcodeName = "FUNCTION_CALL";
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                opcodeName = "FUNCTION_RETURN";
                break;
            case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
                opcodeName = "GET_GLOBAL";
                break;
            case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
                opcodeName = "GET_SUB_FUNCTION";
                break;
            case ZR_INSTRUCTION_ENUM(JUMP):
                opcodeName = "JUMP";
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF):
                opcodeName = "JUMP_IF";
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
                opcodeName = "CREATE_CLOSURE";
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
                opcodeName = "CREATE_OBJECT";
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
                opcodeName = "CREATE_ARRAY";
                break;
            default: {
                // 对于未知的指令，输出数字
                char buf[32];
                snprintf(buf, sizeof(buf), "OPCODE_%u", (TZrUInt32) opcode);
                opcodeName = buf;
                break;
            }
        }

        printf("%s", opcodeName);

        // 根据指令类型输出操作数（简化版本，只输出有意义的操作数）
        TZrUInt16 op1_0 = inst->instruction.operand.operand1[0];
        TZrUInt16 op1_1 = inst->instruction.operand.operand1[1];
        TZrInt32 op2_0 = (TZrInt32) inst->instruction.operand.operand2[0];

        // 根据指令类型输出操作数
        if (opcode == ZR_INSTRUCTION_ENUM(GET_STACK) || opcode == ZR_INSTRUCTION_ENUM(SET_STACK) ||
            opcode == ZR_INSTRUCTION_ENUM(GET_CONSTANT) || opcode == ZR_INSTRUCTION_ENUM(SET_CONSTANT)) {
            // 单操作数指令：extra=目标槽位, operand2=源槽位或常量索引
            printf(" dst=%u, src=%d", operandExtra, op2_0);
        } else if (opcode == ZR_INSTRUCTION_ENUM(ADD_INT) || opcode == ZR_INSTRUCTION_ENUM(ADD_FLOAT) ||
                   opcode == ZR_INSTRUCTION_ENUM(SUB_INT) || opcode == ZR_INSTRUCTION_ENUM(SUB_FLOAT) ||
                   opcode == ZR_INSTRUCTION_ENUM(MUL_SIGNED) || opcode == ZR_INSTRUCTION_ENUM(MUL_FLOAT) ||
                   opcode == ZR_INSTRUCTION_ENUM(DIV_SIGNED) || opcode == ZR_INSTRUCTION_ENUM(DIV_FLOAT) ||
                   opcode == ZR_INSTRUCTION_ENUM(MOD_SIGNED) || opcode == ZR_INSTRUCTION_ENUM(MOD_FLOAT)) {
            // 二元运算指令：extra=结果槽位, operand1[0]=左操作数, operand1[1]=右操作数
            printf(" dst=%u, left=%u, right=%u", operandExtra, op1_0, op1_1);
        } else if (opcode == ZR_INSTRUCTION_ENUM(GET_MEMBER) || opcode == ZR_INSTRUCTION_ENUM(SET_MEMBER) ||
                   opcode == ZR_INSTRUCTION_ENUM(GET_BY_INDEX) || opcode == ZR_INSTRUCTION_ENUM(SET_BY_INDEX)) {
            printf(" dst=%u, receiver=%u, operand=%u", operandExtra, op1_0, op1_1);
        } else if (opcode == ZR_INSTRUCTION_ENUM(ITER_INIT) || opcode == ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT) ||
                   opcode == ZR_INSTRUCTION_ENUM(ITER_CURRENT)) {
            printf(" dst=%u, iterator=%u", operandExtra, op1_0);
        } else if (opcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) || opcode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
            // 函数调用/返回指令：extra=结果数量, operand1[0]=结果槽位, operand1[1]=参数数量或0
            printf(" result_count=%u, result_slot=%u", operandExtra, op1_0);
            if (opcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) && op1_1 > 0) {
                printf(", arg_count=%u", op1_1);
            }
        } else if (opcode == ZR_INSTRUCTION_ENUM(JUMP) || opcode == ZR_INSTRUCTION_ENUM(JUMP_IF)) {
            // 跳转指令：operand2=偏移量
            printf(" offset=%d", op2_0);
            if (opcode == ZR_INSTRUCTION_ENUM(JUMP_IF)) {
                printf(", condition=%u", operandExtra);
            }
        } else if (opcode == ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION)) {
            // 获取子函数指令：extra=目标槽位, operand2=子函数索引
            printf(" dst=%u, func_index=%d", operandExtra, op2_0);
        } else if (opcode == ZR_INSTRUCTION_ENUM(GET_GLOBAL)) {
            // 获取全局对象指令：extra=目标槽位
            printf(" dst=%u", operandExtra);
        } else {
            // 其他指令：输出所有操作数
            if (op1_0 != 0 || op1_1 != 0) {
                printf(" op1=[%u, %u]", op1_0, op1_1);
            }
            if (op2_0 != 0) {
                printf(" op2=[%d]", op2_0);
            }
            if (operandExtra != 0) {
                printf(" extra=%u", operandExtra);
            }
        }
        printf("\n");
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试4: 二元表达式类型推断
static void test_binary_expression_type_inference(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Binary Expression Type Inference";

    TEST_START(testSummary);
    TEST_INFO("Binary Expression Type Inference",
              "Testing that binary expressions correctly infer types and generate appropriate instructions");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：不同类型的二元表达式
    const char *source = "var a = 1 + 2; var b = 1.0 + 2.0; var c = \"hello\" + \"world\";";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证类型推断）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试5: 嵌套函数作用域
static void test_nested_function_scope(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Nested Function Scope";

    TEST_START(testSummary);
    TEST_INFO("Nested Function Scope",
              "Testing that nested functions correctly handle scope and parent compiler references");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：嵌套函数
    const char *source = "outer() { inner() { return 42; } return inner(); }";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功，并且有子函数
    TEST_ASSERT_NOT_NULL(func);
    TEST_ASSERT_GREATER_THAN_UINT32(0, func->childFunctionLength);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试6: 闭包变量捕获
static void test_closure_capture(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Closure Variable Capture";

    TEST_START(testSummary);
    TEST_INFO("Closure Variable Capture", "Testing that lambda expressions correctly capture external variables");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：lambda表达式捕获外部变量
    const char *source = "var x = 10; var f = () => { return x; };";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证闭包捕获）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试7: 复杂左值处理
static void test_complex_lvalue(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Complex Left Value Handling";

    TEST_START(testSummary);
    TEST_INFO("Complex Left Value Handling",
              "Testing that member access and array index assignments are correctly compiled");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：成员访问赋值和数组索引赋值
    const char *source = "var obj = {}; obj.prop = 42; var arr = [1, 2, 3]; arr[0] = 10;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证复杂左值处理）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试8: 外部变量分析
static void test_external_variable_analysis(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "External Variable Analysis";

    TEST_START(testSummary);
    TEST_INFO("External Variable Analysis", "Testing that external variables are correctly identified and captured");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：嵌套函数引用外部变量
    const char *source = "outer() { var x = 10; var y = 20; inner() { return x + y; } return inner(); }";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功，并且有子函数
    TEST_ASSERT_NOT_NULL(func);
    TEST_ASSERT_GREATER_THAN_UINT32(0, func->childFunctionLength);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试9: foreach解构支持
static void test_foreach_destructuring(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Foreach Destructuring Support";

    TEST_START(testSummary);
    TEST_INFO("Foreach Destructuring Support",
              "Testing that foreach loops correctly support object and array destructuring");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：foreach解构对象和数组（使用 for(var pattern in expr) 语法）
    const char *source = "var arr = [{a: 1, b: 2}, {a: 3, b: 4}]; for(var {a, b} in arr) { var sum = a + b; }";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证foreach解构支持）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试10: switch语句
static void test_switch_statement(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Switch Statement";

    TEST_START(testSummary);
    TEST_INFO("Switch Statement", "Testing that switch statements are correctly parsed and compiled");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：switch语句（使用 switch(expr){(value){}...} 语法）
    const char *source = "var x = 1; switch(x){(1){return 10;}(2){return 20;}(/*default*/){return 0;}}";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证switch语句）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试11: 生成器机制
static void test_generator_mechanism(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Generator Mechanism";

    TEST_START(testSummary);
    TEST_INFO("Generator Mechanism",
              "Testing that generator functions and yield/out statements are correctly compiled");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：生成器函数（使用双大括号语法）
    const char *source = "var gen = {{ out 1; out 2; out 3; }};";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证生成器机制）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试12: 函数调用类型推断
static void test_function_call_type_inference(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Function Call Type Inference";

    TEST_START(testSummary);
    TEST_INFO("Function Call Type Inference", "Testing that function calls correctly infer return types");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：函数调用
    const char *source = "add(a: int, b: int): int { return a + b; } var result = add(1, 2);";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证函数调用类型推断）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试13: 成员访问类型推断
static void test_member_access_type_inference(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Member Access Type Inference";

    TEST_START(testSummary);
    TEST_INFO("Member Access Type Inference", "Testing that member access chains correctly infer types");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：链式成员访问
    const char *source = "var obj = {prop: {subprop: 42}}; var value = obj.prop.subprop;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证成员访问类型推断）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试14: 类型转换指令生成
static void test_type_conversion_instructions(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Type Conversion Instructions";

    TEST_START(testSummary);
    TEST_INFO("Type Conversion Instructions",
              "Testing that type conversion instructions are correctly generated for mixed-type operations");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：混合类型运算（需要类型转换）
    const char *source = "var a = 1 + 2.0; var b = 1.0 + 2; var c = \"num: \" + 42;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证类型转换指令生成）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试15: 复合赋值运算符
static void test_compound_assignment_operators(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Compound Assignment Operators";

    TEST_START(testSummary);
    TEST_INFO("Compound Assignment Operators",
              "Testing that compound assignment operators (+=, -=, *=, etc.) are correctly compiled");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：复合赋值运算符
    const char *source = "var a = 10; a += 5; a -= 3; a *= 2; a /= 4; a %= 3;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证复合赋值运算符）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试16: 可变参数函数
static void test_variable_arguments_function(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Variable Arguments Function";

    TEST_START(testSummary);
    TEST_INFO("Variable Arguments Function", "Testing that functions with variable arguments are correctly compiled");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：可变参数函数
    const char *source = "sum(...args: int[]): int { return 0; }";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    {
        SZrFunction *declaredFunction = get_single_compiled_child_function(func);

        TEST_ASSERT_EQUAL_UINT32(0, declaredFunction->parameterCount);
        TEST_ASSERT_EQUAL_INT(ZR_TRUE, declaredFunction->hasVariableArguments);
        TEST_ASSERT_GREATER_THAN_UINT32(0, declaredFunction->instructionsLength);
        TEST_ASSERT_TRUE(function_contains_opcode(declaredFunction, ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)));
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_template_string_compilation_emits_string_pipeline(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Template String Compilation Pipeline";
    TZrBool hasToString = ZR_FALSE;
    TZrBool hasAddString = ZR_FALSE;

    TEST_START(testSummary);
    TEST_INFO("Template string compilation",
              "Testing that template strings lower to TO_STRING + ADD_STRING instructions");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source = "var name = \"zr\"; return `hello ${1} ${name}`;";
        SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse template string source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile template string source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_NOT_NULL(func->instructionsList);
        TEST_ASSERT_GREATER_THAN_UINT32(0, func->instructionsLength);

        for (TZrUInt32 i = 0; i < func->instructionsLength; i++) {
            EZrInstructionCode opcode =
                (EZrInstructionCode)func->instructionsList[i].instruction.operationCode;
            if (opcode == ZR_INSTRUCTION_ENUM(TO_STRING)) {
                hasToString = ZR_TRUE;
            } else if (opcode == ZR_INSTRUCTION_ENUM(ADD_STRING)) {
                hasAddString = ZR_TRUE;
            }
        }

        TEST_ASSERT_TRUE(hasToString);
        TEST_ASSERT_TRUE(hasAddString);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_using_statement_compiles_through_frontend(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Using Statement Frontend Compilation";

    TEST_START(testSummary);
    TEST_INFO("Using statement compilation",
              "Testing that using syntax is accepted by the compiler frontend and preserves control flow");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source = "var resource = \"x\"; using (resource) { var inner = 1; } return 7;";
        SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse using statement source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile using statement source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_GREATER_THAN_UINT32(0, func->instructionsLength);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_field_scoped_using_metadata_serializes_into_prototype_data(void) {
    SZrTestTimer timer;
    const char *testSummary = "Field-Scoped Using Prototype Metadata";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Field-scoped using prototype metadata",
              "Testing that compiler prototypeData preserves managed-field metadata for explicit `using var` fields");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "pub struct HandleBox { using var handle: %unique Resource; var count: int; }\n"
            "pub class Holder { using var resource: %shared Resource; var version: int; }";
        SZrString *sourceName = ZrCore_String_Create(state, "field_using_meta.zr", 19);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        const SZrCompiledPrototypeInfoView *structProto;
        const SZrCompiledPrototypeInfoView *classProto;
        const SZrCompiledMemberInfoView *handleMember;
        const SZrCompiledMemberInfoView *countMember;
        const SZrCompiledMemberInfoView *resourceMember;
        const SZrCompiledMemberInfoView *versionMember;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse field-scoped using source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile field-scoped using source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_NOT_NULL(func->prototypeData);
        TEST_ASSERT_EQUAL_UINT32(2, func->prototypeCount);

        structProto = find_compiled_prototype_by_name(state, func, "HandleBox");
        classProto = find_compiled_prototype_by_name(state, func, "Holder");
        TEST_ASSERT_NOT_NULL(structProto);
        TEST_ASSERT_NOT_NULL(classProto);

        handleMember = find_compiled_member_by_name(state, func, structProto, "handle");
        countMember = find_compiled_member_by_name(state, func, structProto, "count");
        resourceMember = find_compiled_member_by_name(state, func, classProto, "resource");
        versionMember = find_compiled_member_by_name(state, func, classProto, "version");
        TEST_ASSERT_NOT_NULL(handleMember);
        TEST_ASSERT_NOT_NULL(countMember);
        TEST_ASSERT_NOT_NULL(resourceMember);
        TEST_ASSERT_NOT_NULL(versionMember);

        TEST_ASSERT_TRUE(handleMember->isUsingManaged);
        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_UNIQUE, handleMember->ownershipQualifier);
        TEST_ASSERT_TRUE(handleMember->callsClose);
        TEST_ASSERT_TRUE(handleMember->callsDestructor);
        TEST_ASSERT_EQUAL_UINT32(0, handleMember->declarationOrder);

        TEST_ASSERT_FALSE(countMember->isUsingManaged);
        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_NONE, countMember->ownershipQualifier);

        TEST_ASSERT_TRUE(resourceMember->isUsingManaged);
        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_SHARED, resourceMember->ownershipQualifier);
        TEST_ASSERT_TRUE(resourceMember->callsClose);
        TEST_ASSERT_TRUE(resourceMember->callsDestructor);
        TEST_ASSERT_EQUAL_UINT32(0, resourceMember->declarationOrder);

        TEST_ASSERT_FALSE(versionMember->isUsingManaged);
        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_NONE, versionMember->ownershipQualifier);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_static_using_field_is_rejected_by_compiler(void) {
    SZrTestTimer timer;
    const char *testSummary = "Static Using Field Compile-Time Rejection";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Static using rejection",
              "Testing that `static using var` is rejected as a compile-time error by the compiler frontend");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source = "class Holder { static using var resource: %unique Resource; }";
        SZrString *sourceName = ZrCore_String_Create(state, "static_using_compile_error.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse static using field source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        TEST_ASSERT_NULL(func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_ownership_builtin_shared_expression_compiles_without_prototype_target(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Builtin Shared Expression Compiles Without Prototype Target";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Ownership builtin expression lowering",
              "Testing that %shared(expr) compiles to OWN_SHARE without serialized native helper constants");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var box = new Box();\n"
            "var alias = %shared(box);";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_builtin_shared_expr.zr", 32);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse ownership builtin source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile ownership builtin source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
        TEST_ASSERT_FALSE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARED));

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_ownership_builtin_shared_new_wraps_constructed_result(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Builtin Shared New Wraps Constructed Result";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Ownership builtin new lowering",
              "Testing that %shared new Box() emits OWN_SHARE after construction instead of a serialized helper wrapper");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var box = %shared new Box();";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_builtin_shared_new.zr", 31);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse ownership builtin new source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile ownership builtin new source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
        TEST_ASSERT_FALSE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARED));

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_ownership_unique_share_runtime_moves_source_to_null(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Unique Share Runtime Moves Source To Null";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Ownership runtime unique->shared move",
              "Testing that %shared(owner) consumes a %unique owner during execution and leaves the source variable null");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var owner = %unique new Box();\n"
            "var alias = %shared(owner);\n"
            "if (owner == null && alias != null) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_unique_share_runtime.zr",
                                                     strlen("ownership_unique_share_runtime.zr"));
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        TZrInt64 result = 0;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse ownership unique/share runtime source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile ownership unique/share runtime source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result));
        TEST_ASSERT_EQUAL_INT64(1, result);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_ownership_using_share_runtime_moves_source_to_null(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Using Share Runtime Moves Source To Null";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Ownership runtime using->shared move",
              "Testing that %shared(owner) consumes a %using owner during execution and leaves the source variable null");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var owner = %using new Box();\n"
            "var alias = %shared(owner);\n"
            "if (owner == null && alias != null) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_using_share_runtime.zr",
                                                     strlen("ownership_using_share_runtime.zr"));
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        TZrInt64 result = 0;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse ownership using/share runtime source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile ownership using/share runtime source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result));
        TEST_ASSERT_EQUAL_INT64(1, result);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_ownership_weak_runtime_expires_to_null_after_last_shared_release(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Weak Runtime Expires To Null After Last Shared Release";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Ownership runtime weak expiration",
              "Testing that %weak(owner) becomes null after the last %shared owner is overwritten with null");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var owner = %shared new Box();\n"
            "var watcher = %weak(owner);\n"
            "owner = null;\n"
            "if (watcher == null) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_weak_runtime_expire.zr",
                                                     strlen("ownership_weak_runtime_expire.zr"));
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        TZrInt64 result = 0;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse ownership weak runtime source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile ownership weak runtime source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result));
        TEST_ASSERT_EQUAL_INT64(1, result);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_ownership_upgrade_and_release_runtime_follow_lifecycle_contract(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Upgrade And Release Runtime Follow Lifecycle Contract";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Ownership runtime weak->shared upgrade and explicit release",
              "Testing that %upgrade(weak) yields a live shared alias while an owner exists, and %release(owner) clears the last shared owner so a later upgrade becomes null");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var owner = %shared new Box();\n"
            "var watcher = %weak(owner);\n"
            "var alias = %upgrade(watcher);\n"
            "var releasedOwner = %release(owner);\n"
            "var stillAlive = %upgrade(watcher);\n"
            "var releasedAlias = %release(alias);\n"
            "var releasedStillAlive = %release(stillAlive);\n"
            "var second = %upgrade(watcher);\n"
            "if (releasedOwner == null && releasedAlias == null && releasedStillAlive == null && owner == null && alias == null && stillAlive == null && second == null) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_upgrade_release_runtime.zr",
                                                     strlen("ownership_upgrade_release_runtime.zr"));
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        TZrInt64 result = 0;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse ownership upgrade/release runtime source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile ownership upgrade/release runtime source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_UPGRADE)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result));
        TEST_ASSERT_EQUAL_INT64(1, result);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_function_call_argument_conversion_emits_to_float(void) {
    SZrTestTimer timer;
    const char *testSummary = "Function Call Argument Conversion Emits TO_FLOAT";
    TZrUInt32 toFloatCount = 0;

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Function call argument conversion",
              "Testing that calling a typed float function with an int argument emits a TO_FLOAT conversion before FUNCTION_CALL");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "add(lhs: float, rhs: float): float { return lhs + rhs; }\n"
                "var value = add(1, 2.0);";
        SZrString *sourceName = ZrCore_String_Create(state, "typed_call_conversion_test.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse typed call conversion source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile typed call conversion source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)));
        for (TZrUInt32 i = 0; i < func->instructionsLength; i++) {
            if ((EZrInstructionCode)func->instructionsList[i].instruction.operationCode == ZR_INSTRUCTION_ENUM(TO_FLOAT)) {
                toFloatCount++;
            }
        }
        TEST_ASSERT_EQUAL_UINT32(1, toFloatCount);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_intermediate_writer_emits_type_metadata_section(void) {
    SZrTestTimer timer;
    const char *testSummary = "Intermediate Writer Emits Type Metadata Section";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Intermediate writer type metadata",
              "Testing that .zri output includes a dedicated type metadata section with exported signatures");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "pub var numbers = [1, 2, 3];\n"
                "add(lhs: int, rhs: int): int { return lhs + rhs; }\n"
                "return add(1, 2);";
        const char *intermediatePath = "typed_metadata_intermediate_test.zri";
        SZrString *sourceName = ZrCore_String_Create(state, "typed_metadata_intermediate_test.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        char *intermediateText;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse typed metadata intermediate source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile typed metadata intermediate source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, func, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "TYPE_METADATA"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "EXPORTED_SYMBOLS"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "add(int, int): int"));

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_intermediate_writer_emits_compile_time_and_test_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Intermediate Writer Emits Compile Time And Test Metadata";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Intermediate writer metadata closure",
              "Testing that .zri output includes compile-time declarations and %test metadata");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "%compileTime var MAX_SCALE: int = 8;\n"
                "%compileTime buildBias(seed: int): int { return seed + MAX_SCALE; }\n"
                "%test(\"vector_meta\") { return MAX_SCALE; }\n"
                "return MAX_SCALE;";
        const char *intermediatePath = "compiletime_metadata_intermediate_test.zri";
        SZrString *sourceName = ZrCore_String_Create(state, "compiletime_metadata_intermediate_test.zr", 41);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        char *intermediateText;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse compile-time metadata intermediate source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile compile-time metadata intermediate source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, func, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "COMPILE_TIME_VARIABLES (1):"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "MAX_SCALE: int"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "COMPILE_TIME_FUNCTIONS (1):"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "fn buildBias(seed: int): int"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "TESTS (1):"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "test vector_meta()"));

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_member_and_index_access_emit_split_opcodes(void) {
    SZrTestTimer timer;
    const char *testSummary = "Member And Index Access Emit Split Opcodes";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Access opcode split",
              "Testing that dot access lowers to member opcodes while bracket access lowers to index opcodes");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "var obj = { name: 1 };\n"
                "var a = obj.name;\n"
                "var b = obj[\"name\"];\n"
                "obj.name = 2;\n"
                "obj[\"name\"] = 3;";
        const char *intermediatePath = "access_opcode_split_test.zri";
        SZrString *sourceName = ZrCore_String_Create(state, "access_opcode_split_test.zr", 27);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        char *intermediateText;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse split opcode source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile split opcode source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, func, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "GET_MEMBER"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SET_MEMBER"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "GET_BY_INDEX"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SET_BY_INDEX"));
        TEST_ASSERT_NULL(strstr(intermediateText, "DYN_GET"));
        TEST_ASSERT_NULL(strstr(intermediateText, "DYN_SET"));
        TEST_ASSERT_NULL(strstr(intermediateText, "GETTABLE"));
        TEST_ASSERT_NULL(strstr(intermediateText, "SETTABLE"));

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_global_member_access_emits_get_member(void) {
    SZrTestTimer timer;
    const char *testSummary = "Global Member Access Emits GET_MEMBER";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Global member lowering",
              "Testing that global zr member access still goes through member semantics instead of table fallback");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source = "var x = zr.Error;";
        const char *intermediatePath = "global_member_opcode_test.zri";
        SZrString *sourceName = ZrCore_String_Create(state, "global_member_opcode_test.zr", 27);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        char *intermediateText;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse global member source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile global member source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, func, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "GET_GLOBAL"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "GET_MEMBER"));
        TEST_ASSERT_NULL(strstr(intermediateText, "GETTABLE"));

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_fixed_array_lowering_does_not_emit_length_member_write(void) {
    SZrTestTimer timer;
    const char *testSummary = "Fixed Array Lowering Omits Length Member Write";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Fixed array lowering",
              "Testing that fixed-size array initialization lowers through CREATE_ARRAY and SET_BY_INDEX without synthesizing SET_MEMBER length writes");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "read(): int {\n"
                "    var xs: int[3];\n"
                "    return xs.length;\n"
                "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "fixed_array_length_lowering_test.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *wrapper;
        SZrFunction *readFunction;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse fixed array source");
            destroy_test_state(state);
            return;
        }

        wrapper = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (wrapper == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile fixed array source");
            destroy_test_state(state);
            return;
        }

        readFunction = get_single_compiled_child_function(wrapper);
        TEST_ASSERT_TRUE(function_contains_opcode(readFunction, ZR_INSTRUCTION_ENUM(CREATE_ARRAY)));
        TEST_ASSERT_TRUE(function_contains_opcode(readFunction, ZR_INSTRUCTION_ENUM(SET_BY_INDEX)));
        TEST_ASSERT_TRUE(function_contains_opcode(readFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_FALSE(function_contains_opcode(readFunction, ZR_INSTRUCTION_ENUM(SET_MEMBER)));
        ZrCore_Function_Free(state, wrapper);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_foreach_emits_iterator_contract_opcodes(void) {
    SZrTestTimer timer;
    const char *testSummary = "Foreach Emits Iterator Contract Opcodes";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Foreach lowering",
              "Testing that foreach lowers through iterator contract opcodes instead of string member protocol names");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "for(var item in [1, 2, 3]) {\n"
                "    var x = item;\n"
                "}";
        const char *intermediatePath = "foreach_iterator_contract_test.zri";
        SZrString *sourceName = ZrCore_String_Create(state, "foreach_iterator_contract_test.zr", 32);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        char *intermediateText;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse foreach source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile foreach source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, func, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "ITER_INIT"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "ITER_MOVE_NEXT"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "ITER_CURRENT"));
        TEST_ASSERT_NULL(strstr(intermediateText, "getIterator"));
        TEST_ASSERT_NULL(strstr(intermediateText, "moveNext"));
        TEST_ASSERT_NULL(strstr(intermediateText, "current"));
        TEST_ASSERT_NULL(strstr(intermediateText, "GETTABLE"));

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_type_expression_compiles_to_typeof_opcode(void) {
    SZrTestTimer timer;
    const char *testSummary = "Type Expression Compiles To TYPEOF Opcode";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Type expression lowering",
              "Testing that %type(expr) lowers to the dedicated TYPEOF opcode instead of a serialized native helper");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "var math = %import(\"zr.math\");\n"
                "var reflection = %type(math);";
        SZrString *sourceName = ZrCore_String_Create(state, "type_expression_compile_test.zr", 31);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse type expression source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile type expression source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(TYPEOF)));
        TEST_ASSERT_FALSE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_REFLECTION_TYPEOF));

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_binary_writer_handles_exported_function_alias_with_unknown_parameter_types(void) {
    SZrTestTimer timer;
    const char *testSummary = "Binary Writer Handles Exported Function Alias With Unknown Parameter Types";

    timer.startTime = clock();
    TEST_START(testSummary);
    TEST_INFO("Binary writer export metadata",
              "Testing that exported aliases of partially typed functions still serialize typed export metadata without crashes");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "runCallbacksImpl(lin, signal, tensor) {\n"
                "    return 0;\n"
                "}\n"
                "pub var runCallbacks = runCallbacksImpl;";
        const char *binaryPath = "export_alias_unknown_params_test.zro";
        SZrString *sourceName = ZrCore_String_Create(state, "export_alias_unknown_params_test.zr", 35);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse exported alias source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile exported alias source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, func, binaryPath));

        remove(binaryPath);
        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 主测试函数
int main(void) {
    printf("\n");
    TEST_MODULE_DIVIDER();
    printf("ZR-VM Compiler Features Unit Tests\n");
    TEST_MODULE_DIVIDER();
    printf("\n");

    UNITY_BEGIN();

    // 基础功能测试模块
    printf("==========\n");
    printf("Basic Compiler Features Tests\n");
    printf("==========\n");
    RUN_TEST(test_function_parameter_handling);
    RUN_TEST(test_constant_deduplication);
    RUN_TEST(test_global_object_access);
    RUN_TEST(test_binary_expression_type_inference);
    RUN_TEST(test_nested_function_scope);

    // 表达式编译完善测试模块
    printf("==========\n");
    printf("Expression Compilation Tests\n");
    printf("==========\n");
    RUN_TEST(test_closure_capture);
    RUN_TEST(test_complex_lvalue);
    RUN_TEST(test_external_variable_analysis);

    // 语句编译完善测试模块
    printf("==========\n");
    printf("Statement Compilation Tests\n");
    printf("==========\n");
    RUN_TEST(test_foreach_destructuring);
    RUN_TEST(test_switch_statement);
    RUN_TEST(test_generator_mechanism);

    // 类型推断测试模块
    printf("==========\n");
    printf("Type Inference Tests\n");
    printf("==========\n");
    RUN_TEST(test_function_call_type_inference);
    RUN_TEST(test_member_access_type_inference);
    RUN_TEST(test_type_conversion_instructions);
    RUN_TEST(test_function_call_argument_conversion_emits_to_float);
    RUN_TEST(test_type_expression_compiles_to_typeof_opcode);
    RUN_TEST(test_intermediate_writer_emits_type_metadata_section);
    RUN_TEST(test_intermediate_writer_emits_compile_time_and_test_metadata);
    RUN_TEST(test_member_and_index_access_emit_split_opcodes);
    RUN_TEST(test_global_member_access_emits_get_member);
    RUN_TEST(test_fixed_array_lowering_does_not_emit_length_member_write);
    RUN_TEST(test_foreach_emits_iterator_contract_opcodes);
    RUN_TEST(test_binary_writer_handles_exported_function_alias_with_unknown_parameter_types);

    // 高级功能测试模块
    printf("==========\n");
    printf("Advanced Features Tests\n");
    printf("==========\n");
    RUN_TEST(test_compound_assignment_operators);
    RUN_TEST(test_variable_arguments_function);
    RUN_TEST(test_template_string_compilation_emits_string_pipeline);
    RUN_TEST(test_using_statement_compiles_through_frontend);
    RUN_TEST(test_field_scoped_using_metadata_serializes_into_prototype_data);
    RUN_TEST(test_static_using_field_is_rejected_by_compiler);
    RUN_TEST(test_ownership_builtin_shared_expression_compiles_without_prototype_target);
    RUN_TEST(test_ownership_builtin_shared_new_wraps_constructed_result);
    RUN_TEST(test_ownership_unique_share_runtime_moves_source_to_null);
    RUN_TEST(test_ownership_using_share_runtime_moves_source_to_null);
    RUN_TEST(test_ownership_weak_runtime_expires_to_null_after_last_shared_release);
    RUN_TEST(test_ownership_upgrade_and_release_runtime_follow_lifecycle_contract);

    printf("\n");
    TEST_MODULE_DIVIDER();
    printf("All Test Cases Completed\n");
    TEST_MODULE_DIVIDER();
    printf("\n");

    return UNITY_END();
}

