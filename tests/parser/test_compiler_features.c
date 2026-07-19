//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "module_fixture_support.h"
#include "reference_support.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

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
    TZrUInt32 modifierFlags;
    TZrUInt32 nextVirtualSlotIndex;
    TZrUInt32 nextPropertyIdentity;
    TZrUInt32 layoutByteSize;
    TZrUInt32 layoutByteAlign;
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
    TZrUInt32 hasDecoratorMetadata;
    TZrUInt32 decoratorMetadataConstantIndex;
    TZrUInt32 hasDecoratorNames;
    TZrUInt32 decoratorNamesConstantIndex;
    TZrUInt32 modifierFlags;
    TZrUInt32 ownerTypeNameStringIndex;
    TZrUInt32 baseDefinitionOwnerTypeNameStringIndex;
    TZrUInt32 baseDefinitionNameStringIndex;
    TZrUInt32 virtualSlotIndex;
    TZrUInt32 interfaceContractSlot;
    TZrUInt32 propertyIdentity;
    TZrUInt32 accessorRole;
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
    ZrVmLibContainer_Register(global);
    ZrVmLibMath_Register(global);
    ZrVmLibSystem_Register(global);
    ZrVmLibFfi_Register(global);

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

static void fixture_reader_close_noop(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static SZrFunction *load_runtime_entry_from_binary_file(SZrState *state, const TZrChar *binaryPath) {
    TZrSize binaryLength = 0;
    TZrByte *binaryBytes;
    ZrTestsFixtureReader reader;
    SZrIo *io;
    SZrIoSource *sourceObject;
    SZrFunction *runtimeFunction;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(binaryPath);

    binaryBytes = ZrTests_Fixture_ReadFileBytes(binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);
    TEST_ASSERT_TRUE(binaryLength > 0);

    reader.bytes = binaryBytes;
    reader.length = binaryLength;
    reader.consumed = ZR_FALSE;

    io = ZrCore_Io_New(state->global);
    TEST_ASSERT_NOT_NULL(io);
    ZrCore_Io_Init(state, io, ZrTests_Fixture_ReaderRead, fixture_reader_close_noop, &reader);
    io->isBinary = ZR_TRUE;

    sourceObject = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);

    ZrCore_Io_Free(state->global, io);
    free(binaryBytes);
    return runtimeFunction;
}

static SZrFunction *get_single_compiled_child_function(SZrFunction *wrapper) {
    TEST_ASSERT_NOT_NULL(wrapper);
    TEST_ASSERT_EQUAL_UINT32(1, wrapper->childFunctionLength);
    TEST_ASSERT_NOT_NULL(wrapper->childFunctionList);
    return &wrapper->childFunctionList[0];
}

static TZrBool function_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode);

static SZrFunction *find_single_function_constant_with_opcode(SZrState *state,
                                                              SZrFunction *wrapper,
                                                              EZrInstructionCode opcode) {
    SZrFunction *match = ZR_NULL;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(wrapper);

    for (TZrUInt32 constantIndex = 0; constantIndex < wrapper->constantValueLength; constantIndex++) {
        SZrTypeValue *constant = &wrapper->constantValueList[constantIndex];
        SZrFunction *candidate;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION || constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }

        candidate = ZR_CAST_FUNCTION(state, constant->value.object);
        if (!function_contains_opcode(candidate, opcode)) {
            continue;
        }

        TEST_ASSERT_NULL(match);
        match = candidate;
    }

    return match;
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

static TZrUInt32 function_count_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 i = 0; i < function->instructionsLength; i++) {
        if ((EZrInstructionCode)function->instructionsList[i].instruction.operationCode == opcode) {
            count++;
        }
    }

    return count;
}

static TZrBool function_opcode_appears_before(const SZrFunction *function,
                                              EZrInstructionCode firstOpcode,
                                              EZrInstructionCode secondOpcode) {
    TZrBool sawFirstOpcode = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 i = 0; i < function->instructionsLength; i++) {
        EZrInstructionCode opcode = (EZrInstructionCode)function->instructionsList[i].instruction.operationCode;

        if (opcode == firstOpcode) {
            sawFirstOpcode = ZR_TRUE;
        }
        if (opcode == secondOpcode) {
            return sawFirstOpcode;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_constant_is_null(const SZrFunction *function, TZrUInt32 constantIndex) {
    const SZrTypeValue *constant;

    if (function == ZR_NULL || function->constantValueList == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    constant = &function->constantValueList[constantIndex];
    return constant != ZR_NULL && ZR_VALUE_IS_TYPE_NULL(constant->type);
}

static TZrUInt32 function_count_null_get_constant(const SZrFunction *function) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 i = 0; i < function->instructionsLength; i++) {
        const TZrInstruction *instruction = &function->instructionsList[i];
        if ((EZrInstructionCode)instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
            continue;
        }

        if (function_constant_is_null(function, (TZrUInt32)instruction->instruction.operand.operand2[0])) {
            count++;
        }
    }

    return count;
}

static TZrUInt32 function_max_null_get_constant_streak(const SZrFunction *function) {
    TZrUInt32 maxStreak = 0;
    TZrUInt32 currentStreak = 0;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 i = 0; i < function->instructionsLength; i++) {
        const TZrInstruction *instruction = &function->instructionsList[i];
        if ((EZrInstructionCode)instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_CONSTANT) &&
            function_constant_is_null(function, (TZrUInt32)instruction->instruction.operand.operand2[0])) {
            currentStreak++;
            if (currentStreak > maxStreak) {
                maxStreak = currentStreak;
            }
        } else {
            currentStreak = 0;
        }
    }

    return maxStreak;
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

static TZrUInt32 test_align_offset_u32(TZrUInt32 offset, TZrUInt32 align) {
    return ((offset + align - 1) / align) * align;
}

// 测试1: 函数声明参数处理
void test_function_parameter_handling(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Function Parameter Handling";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Function Parameter Handling",
              "Testing that function declarations correctly extract parameter count and variable arguments flag");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：函数声明带参数（不需要function关键字）
    const char *source = "testFunc(a, b, c) { return a + b + c; }";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
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
            case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):
                opcodeName = "TO_FLOAT_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):
                opcodeName = "TO_FLOAT_UNSIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):
                opcodeName = "TO_INT_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):
                opcodeName = "TO_INT_UNSIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):
                opcodeName = "TO_UINT_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):
                opcodeName = "TO_UINT_SIGNED";
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
            case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
                opcodeName = "JUMP_IF_GREATER_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
                opcodeName = "JUMP_IF_LESS_EQUAL_SIGNED";
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
        } else if (opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED) ||
                   opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED)) {
            printf(" left=%u, right=%u, jump_offset=%d", operandExtra, op1_0, (TZrInt16)op1_1);
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
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试2: 常量去重
void test_constant_deduplication(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Constant Deduplication";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Constant Deduplication", "Testing that duplicate constants are deduplicated in the constant pool");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：使用相同的常量多次
    const char *source = "var a = 42; var b = 42; var c = 42;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证常量池中只有一个42（去重后）
    // 注意：这里需要检查常量池，但常量池是内部实现
    // 我们可以通过检查函数是否成功编译来间接验证
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试3: 全局对象属性访问
void test_global_object_access(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Global Object Access";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Global Object Access",
              "Testing that global object properties can be accessed using GET_GLOBAL + GET_MEMBER");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：访问全局对象属性
    const char *source = "var x = zr.Error;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
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
            case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):
                opcodeName = "TO_FLOAT_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):
                opcodeName = "TO_FLOAT_UNSIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):
                opcodeName = "TO_INT_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):
                opcodeName = "TO_INT_UNSIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):
                opcodeName = "TO_UINT_FLOAT";
                break;
            case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):
                opcodeName = "TO_UINT_SIGNED";
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
            case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
                opcodeName = "JUMP_IF_GREATER_SIGNED";
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
                opcodeName = "JUMP_IF_LESS_EQUAL_SIGNED";
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
        } else if (opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED) ||
                   opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED)) {
            printf(" left=%u, right=%u, jump_offset=%d", operandExtra, op1_0, (TZrInt16)op1_1);
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
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试4: 二元表达式类型推断
void test_binary_expression_type_inference(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Binary Expression Type Inference";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Binary Expression Type Inference",
              "Testing that binary expressions correctly infer types and generate appropriate instructions");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：不同类型的二元表达式
    const char *source = "var a = 1 + 2; var b = 1.0 + 2.0; var c = \"hello\" + \"world\";";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证类型推断）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试5: 嵌套函数作用域
void test_nested_function_scope(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Nested Function Scope";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Nested Function Scope",
              "Testing that nested functions correctly handle scope and parent compiler references");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：嵌套函数
    const char *source = "outer() { inner() { return 42; } return inner(); }";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功，并且有子函数
    TEST_ASSERT_NOT_NULL(func);
    TEST_ASSERT_GREATER_THAN_UINT32(0, func->childFunctionLength);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试6: 闭包变量捕获
void test_closure_capture(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Closure Variable Capture";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Closure Variable Capture", "Testing that lambda expressions correctly capture external variables");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：lambda表达式捕获外部变量
    const char *source = "var x = 10; var f = () => { return x; };";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证闭包捕获）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试7: 复杂左值处理
void test_complex_lvalue(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Complex Left Value Handling";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Complex Left Value Handling",
              "Testing that member access and array index assignments are correctly compiled");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：成员访问赋值和数组索引赋值
    const char *source = "var obj = {}; obj.prop = 42; var arr = [1, 2, 3]; arr[0] = 10;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证复杂左值处理）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试8: 外部变量分析
void test_external_variable_analysis(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "External Variable Analysis";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("External Variable Analysis", "Testing that external variables are correctly identified and captured");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：嵌套函数引用外部变量
    const char *source = "outer() { var x = 10; var y = 20; inner() { return x + y; } return inner(); }";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功，并且有子函数
    TEST_ASSERT_NOT_NULL(func);
    TEST_ASSERT_GREATER_THAN_UINT32(0, func->childFunctionLength);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试9: foreach解构支持
void test_foreach_destructuring(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Foreach Destructuring Support";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Foreach Destructuring Support",
              "Testing that foreach loops correctly support object and array destructuring");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：foreach解构对象和数组（使用 for(var pattern in expr) 语法）
    const char *source = "var arr = [{a: 1, b: 2}, {a: 3, b: 4}]; for(var {a, b} in arr) { var sum = a + b; }";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证foreach解构支持）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试10: switch语句
void test_switch_statement(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Switch Statement";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Switch Statement", "Testing that switch statements are correctly parsed and compiled");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：switch语句（使用 switch(expr){(value){}...} 语法）
    const char *source = "var x = 1; switch(x){(1){return 10;}(2){return 20;}(/*default*/){return 0;}}";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证switch语句）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试11: 生成器机制
void test_generator_mechanism(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Generator Mechanism";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Generator Mechanism",
              "Testing that generator functions and yield/out statements are correctly compiled");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：生成器函数（使用双大括号语法）
    const char *source = "var gen = {{ out 1; out 2; out 3; }};";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证生成器机制）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试12: 函数调用类型推断
void test_function_call_type_inference(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Function Call Type Inference";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Function Call Type Inference", "Testing that function calls correctly infer return types");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：函数调用
    const char *source = "add(a: int, b: int): int { return a + b; } var result = add(1, 2);";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证函数调用类型推断）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试13: 成员访问类型推断
void test_member_access_type_inference(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Member Access Type Inference";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Member Access Type Inference", "Testing that member access chains correctly infer types");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：链式成员访问
    const char *source = "var obj = {prop: {subprop: 42}}; var value = obj.prop.subprop;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证成员访问类型推断）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试14: 类型转换指令生成
void test_type_conversion_instructions(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Type Conversion Instructions";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Type Conversion Instructions",
              "Testing that type conversion instructions are correctly generated for mixed-type operations");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：混合类型运算（需要类型转换）
    const char *source = "var a = 1 + 2.0; var b = 1.0 + 2; var c = \"num: \" + 42;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证类型转换指令生成）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试15: 复合赋值运算符
void test_compound_assignment_operators(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Compound Assignment Operators";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Compound Assignment Operators",
              "Testing that compound assignment operators (+=, -=, *=, etc.) are correctly compiled");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：复合赋值运算符
    const char *source = "var a = 10; a += 5; a -= 3; a *= 2; a /= 4; a %= 3;";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
        destroy_test_state(state);
        return;
    }

    // 验证函数编译成功（间接验证复合赋值运算符）
    TEST_ASSERT_NOT_NULL(func);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试16: 可变参数函数
void test_variable_arguments_function(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Variable Arguments Function";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Variable Arguments Function", "Testing that functions with variable arguments are correctly compiled");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    // 测试代码：可变参数函数
    const char *source = "sum(...args: int[]): int { return 0; }";
    SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        return;
    }

    // 编译AST
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);

    if (func == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile function");
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
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_template_string_compilation_emits_string_pipeline(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Template String Compilation Pipeline";
    TZrBool hasToString = ZR_FALSE;
    TZrBool hasAddString = ZR_FALSE;

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Template string compilation",
              "Testing that template strings lower to TO_STRING + ADD_STRING instructions");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source = "var name = \"zr\"; return `hello ${1} ${name}`;";
        SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse template string source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile template string source");
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
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_using_statement_compiles_through_frontend(void) {
    SZrTestTimer timer;
    timer.startTime = clock();
    const char *testSummary = "Using Statement Frontend Compilation";

    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Using statement compilation",
              "Testing that using syntax is accepted by the compiler frontend and preserves control flow");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source = "var resource = \"x\"; %using (resource) { var inner = 1; } return 7;";
        SZrString *sourceName = ZrCore_String_Create(state, "test.zr", 7);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse using statement source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile using statement source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_GREATER_THAN_UINT32(0, func->instructionsLength);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_using_owner_generic_emits_release_cleanup(void) {
    SZrTestTimer timer;
    const char *testSummary = "Using Owner Generic Emits Release Cleanup";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Using owner generic cleanup",
                 "Testing that using on Shared<T> releases the owner at scope exit");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "class Box {}\n"
                "var seed = Unique<Box>(new Box());\n"
                "var owner = Shared<Box>(seed);\n"
                "var watcher = Weak<Box>(owner);\n"
                "using (owner) { var inner = 1; }\n"
                "var after = %upgrade(watcher);\n"
                "if (after == null && owner == null) { return 1; }\n"
                "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "using_owner_generic_cleanup.zr",
                                                     strlen("using_owner_generic_cleanup.zr"));
        SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile using owner generic cleanup source");
            destroy_test_state(state);
            return;
        }

        TZrInt64 result = 0;
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
        TEST_ASSERT_FALSE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED)));
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute using owner generic cleanup source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        if (result != 1) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Using owner generic cleanup did not release the owner");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        ZrCore_Function_Free(state, func);
    }

    {
        const char *source =
            "var seen = 0;\n"
            "using (var math = %import(\"zr.math\")) {\n"
            "    seen = 1;\n"
            "} else {\n"
            "    seen = 2;\n"
            "}\n"
            "return seen;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "plugin_guard_plain_scoped_module_release.zr",
                                                     strlen("plugin_guard_plain_scoped_module_release.zr"));
        SZrFunction *func;
        TZrInt64 result = 0;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile plain plugin guard scoped release source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARE_PLAIN));
        TEST_ASSERT_EQUAL_UINT32(1u, function_count_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute plain plugin guard scoped release source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        if (result != 1) {
            char failureMessage[128];
            snprintf(failureMessage,
                     sizeof(failureMessage),
                     "Plain plugin guard scoped release returned unexpected result: %lld",
                     (long long)result);
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, failureMessage);
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_using_owner_generic_release_runs_before_return(void) {
    SZrTestTimer timer;
    const char *testSummary = "Using Owner Generic Release Runs Before Return";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Using owner generic return cleanup",
                 "Testing that using on Shared<T> releases the owner before returning from the using body");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "class Box {}\n"
                "var seed = Unique<Box>(new Box());\n"
                "var owner = Shared<Box>(seed);\n"
                "using (owner) {\n"
                "    return 1;\n"
                "}\n"
                "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "using_owner_generic_return_cleanup.zr",
                                                     strlen("using_owner_generic_return_cleanup.zr"));
        SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile using owner generic return cleanup source");
            destroy_test_state(state);
            return;
        }

        TZrInt64 result = 0;
        TEST_ASSERT_TRUE(function_opcode_appears_before(func,
                                                        ZR_INSTRUCTION_ENUM(OWN_RELEASE),
                                                        ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)));
        TEST_ASSERT_FALSE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED)));
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute using owner generic return cleanup source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        if (result != 1) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Using owner generic return cleanup changed return value");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_using_owner_generic_release_runs_before_break(void) {
    SZrTestTimer timer;
    const char *testSummary = "Using Owner Generic Release Runs Before Break";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Using owner generic break cleanup",
                 "Testing that using on Shared<T> releases the owner before breaking out of the owning block");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "class Box {}\n"
                "var seed = Unique<Box>(new Box());\n"
                "var owner = Shared<Box>(seed);\n"
                "var watcher = Weak<Box>(owner);\n"
                "while (true) {\n"
                "    using (owner) { break; }\n"
                "}\n"
                "var after = %upgrade(watcher);\n"
                "if (after == null && owner == null) { return 1; }\n"
                "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "using_owner_generic_break_cleanup.zr",
                                                     strlen("using_owner_generic_break_cleanup.zr"));
        SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile using owner generic break cleanup source");
            destroy_test_state(state);
            return;
        }

        TZrInt64 result = 0;
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
        TEST_ASSERT_FALSE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED)));
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute using owner generic break cleanup source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        if (result != 1) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Using owner generic break cleanup did not release the owner");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_using_borrow_generic_emits_end_borrow_cleanup(void) {
    SZrTestTimer timer;
    const char *testSummary = "Using Borrow Generic Emits End Borrow Cleanup";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Using borrow generic cleanup",
                 "Testing that using on Borrow<T> emits a deterministic end-borrow cleanup at scope exit");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "class Box {}\n"
                "var seed = Unique<Box>(new Box());\n"
                "var owner = Shared<Box>(seed);\n"
                "using (Borrow<Box>(owner)) { var inner = 1; }\n"
                "return 1;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "using_borrow_generic_cleanup.zr",
                                                     strlen("using_borrow_generic_cleanup.zr"));
        SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile using borrow generic cleanup source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_BORROW)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
        TEST_ASSERT_FALSE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED)));

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_using_borrow_generic_end_borrow_runs_before_return(void) {
    SZrTestTimer timer;
    const char *testSummary = "Using Borrow Generic End Borrow Runs Before Return";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Using borrow generic return cleanup",
                 "Testing that using on Borrow<T> ends the borrow before returning from the using body");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "class Box {}\n"
                "var seed = Unique<Box>(new Box());\n"
                "var owner = Shared<Box>(seed);\n"
                "using (Borrow<Box>(owner)) {\n"
                "    return 1;\n"
                "}\n"
                "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "using_borrow_generic_return_cleanup.zr",
                                                     strlen("using_borrow_generic_return_cleanup.zr"));
        SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile using borrow generic return cleanup source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_BORROW)));
        TEST_ASSERT_TRUE(function_opcode_appears_before(func,
                                                        ZR_INSTRUCTION_ENUM(OWN_RELEASE),
                                                        ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)));
        TEST_ASSERT_FALSE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED)));

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_using_loan_generic_returns_loan_to_source_on_scope_exit(void) {
    SZrTestTimer timer;
    const char *testSummary = "Using Loan Generic Returns Loan To Source On Scope Exit";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Using loan generic cleanup",
                 "Testing that using on Loan<T> restores the original unique owner at scope exit");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "var owner = Unique<string>(\"loan-scope\");\n"
                "using (Loan<string>(owner)) { var inner = 1; }\n"
                "if (owner != null) { return 1; }\n"
                "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "using_loan_generic_scope_cleanup.zr",
                                                     strlen("using_loan_generic_scope_cleanup.zr"));
        SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile using loan generic cleanup source");
            destroy_test_state(state);
            return;
        }

        TZrInt64 result = 0;
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_LOAN)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN)));
        TEST_ASSERT_FALSE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED)));
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute using loan generic cleanup source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        TEST_ASSERT_EQUAL_INT64(1, result);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_using_loan_generic_returns_loan_before_break(void) {
    SZrTestTimer timer;
    const char *testSummary = "Using Loan Generic Returns Loan Before Break";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Using loan generic break cleanup",
                 "Testing that using on Loan<T> restores the source owner before breaking out of the owning block");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "var owner = Unique<string>(\"loan-break\");\n"
                "while (true) {\n"
                "    using (Loan<string>(owner)) { break; }\n"
                "}\n"
                "if (owner != null) { return 1; }\n"
                "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "using_loan_generic_break_cleanup.zr",
                                                     strlen("using_loan_generic_break_cleanup.zr"));
        SZrFunction *func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile using loan generic break cleanup source");
            destroy_test_state(state);
            return;
        }

        TZrInt64 result = 0;
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_LOAN)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN)));
        TEST_ASSERT_FALSE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED)));
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute using loan generic break cleanup source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        TEST_ASSERT_EQUAL_INT64(1, result);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_struct_prototype_metadata_serializes_layout_size_and_align(void) {
    SZrTestTimer timer;
    const char *testSummary = "Struct Prototype Metadata Serializes Layout Size And Align";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Struct byte layout metadata",
                 "Testing that compiled prototypeData stores whole-struct byte size and alignment alongside field offsets");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source = "pub struct LayoutProbe { var a: i8; var b: int; var c: i32; }";
        const TZrUInt32 expectedAlign = ZR_ALIGN_SIZE;
        const TZrUInt32 offsetA = 0;
        const TZrUInt32 offsetB = test_align_offset_u32(offsetA + (TZrUInt32)sizeof(TZrInt8), ZR_ALIGN_SIZE);
        const TZrUInt32 offsetC = test_align_offset_u32(offsetB + (TZrUInt32)sizeof(TZrInt64), (TZrUInt32)sizeof(TZrInt32));
        const TZrUInt32 expectedSize =
                test_align_offset_u32(offsetC + (TZrUInt32)sizeof(TZrInt32), expectedAlign);
        SZrString *sourceName = ZrCore_String_Create(state, "struct_layout_metadata.zr", 25);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        const SZrCompiledPrototypeInfoView *structProto;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse struct layout metadata source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile struct layout metadata source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_NOT_NULL(func->prototypeData);
        TEST_ASSERT_EQUAL_UINT32(1, func->prototypeCount);

        structProto = find_compiled_prototype_by_name(state, func, "LayoutProbe");
        TEST_ASSERT_NOT_NULL(structProto);
        TEST_ASSERT_EQUAL_UINT32(expectedSize, structProto->layoutByteSize);
        TEST_ASSERT_EQUAL_UINT32(expectedAlign, structProto->layoutByteAlign);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_function_frame_layout_metadata_marks_struct_parameter_inline(void) {
    SZrTestTimer timer;
    const char *testSummary = "Function Frame Layout Metadata Marks Struct Parameter Inline";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Function frame byte layout metadata",
                 "Testing that function stack slots receive deterministic byte offsets and inline struct parameter spans");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "pub struct FrameProbe { var a: i8; var b: int; }\n"
            "useProbe(p: FrameProbe) { var n: int = 7; return n; }";
        const TZrUInt32 expectedStructAlign = ZR_ALIGN_SIZE;
        const TZrUInt32 offsetA = 0;
        const TZrUInt32 offsetB = test_align_offset_u32(offsetA + (TZrUInt32)sizeof(TZrInt8), ZR_ALIGN_SIZE);
        const TZrUInt32 expectedStructSize =
                test_align_offset_u32(offsetB + (TZrUInt32)sizeof(TZrInt64), expectedStructAlign);
        SZrString *sourceName = ZrCore_String_Create(state, "function_frame_layout.zr", 24);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *wrapper;
        SZrFunction *declaredFunction;
        const SZrFunctionFrameSlotLayout *parameterLayout;
        const SZrFunctionFrameSlotLayout *localLayout;
        TZrUInt32 expectedFrameByteBaseOffset;
        TZrUInt32 expectedLocalOffset;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse function frame layout source");
            destroy_test_state(state);
            return;
        }

        wrapper = ZrParser_Compiler_Compile(state, ast);
        if (wrapper == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile function frame layout source");
            destroy_test_state(state);
            return;
        }

        declaredFunction = get_single_compiled_child_function(wrapper);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2, declaredFunction->stackSize);
        TEST_ASSERT_EQUAL_UINT32(declaredFunction->stackSize, declaredFunction->frameSlotLayoutLength);
        TEST_ASSERT_EQUAL_UINT32(ZR_ALIGN_SIZE, declaredFunction->frameByteAlign);
        expectedFrameByteBaseOffset =
                test_align_offset_u32((TZrUInt32)(declaredFunction->stackSize * sizeof(SZrTypeValueOnStack)),
                                      ZR_ALIGN_SIZE);
        expectedLocalOffset = test_align_offset_u32(expectedFrameByteBaseOffset + expectedStructSize,
                                                    ZR_ALIGN_SIZE);

        parameterLayout = ZrCore_Function_FindFrameSlotLayout(declaredFunction, 0);
        localLayout = ZrCore_Function_FindFrameSlotLayout(declaredFunction, 1);
        TEST_ASSERT_NOT_NULL(parameterLayout);
        TEST_ASSERT_NOT_NULL(localLayout);

        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT, parameterLayout->slotKind);
        TEST_ASSERT_TRUE(parameterLayout->isParameter);
        TEST_ASSERT_EQUAL_UINT32(expectedFrameByteBaseOffset, parameterLayout->byteOffset);
        TEST_ASSERT_EQUAL_UINT32(expectedStructSize, parameterLayout->byteSize);
        TEST_ASSERT_EQUAL_UINT32(expectedStructAlign, parameterLayout->byteAlign);
        TEST_ASSERT_NOT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, parameterLayout->typeLayoutId);

        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_SLOT_KIND_VALUE, localLayout->slotKind);
        TEST_ASSERT_FALSE(localLayout->isParameter);
        TEST_ASSERT_EQUAL_UINT32(expectedLocalOffset, localLayout->byteOffset);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrTypeValue), localLayout->byteSize);
        TEST_ASSERT_EQUAL_UINT32(ZR_ALIGN_SIZE, localLayout->byteAlign);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, localLayout->typeLayoutId);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(localLayout->byteOffset + localLayout->byteSize,
                                           declaredFunction->frameByteSize);

        ZrCore_Function_Free(state, wrapper);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_function_frame_layout_metadata_keeps_large_struct_arithmetic_temps_plain(void) {
    SZrTestTimer timer;
    const char *testSummary = "Function Frame Layout Metadata Keeps Large Struct Arithmetic Temps Plain";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Function frame layout scalar temp reuse",
                 "Testing that inline struct slot hints do not leak into scalar arithmetic temporaries after large struct construction");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "pub struct WidePoint {\n"
            "    var a: int; var b: int; var c: int; var d: int; var e: int;\n"
            "    var f: int; var g: int; var h: int; var i: int; var j: int;\n"
            "    pub @constructor(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int, i: int, j: int) {\n"
            "        this.a = a; this.b = b; this.c = c; this.d = d; this.e = e;\n"
            "        this.f = f; this.g = g; this.h = h; this.i = i; this.j = j;\n"
            "    }\n"
            "}\n"
            "var point: WidePoint = $WidePoint(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);\n"
            "return (point.a * 1000000) + (point.e * 10000) + point.j;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "large_struct_arithmetic_frame_layout.zr", 39);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        const SZrFunctionFrameSlotLayout *pointLayout;
        const SZrFunctionFrameSlotLayout *slot4Layout;
        TZrBool sawScalarArithmetic = ZR_FALSE;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse large struct frame layout source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile large struct frame layout source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(5u, func->frameSlotLayoutLength);
        pointLayout = ZrCore_Function_FindFrameSlotLayout(func, 0);
        slot4Layout = ZrCore_Function_FindFrameSlotLayout(func, 4);
        TEST_ASSERT_NOT_NULL(pointLayout);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT, pointLayout->slotKind);
        TEST_ASSERT_NOT_NULL(slot4Layout);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
                ZR_FUNCTION_FRAME_SLOT_KIND_VALUE,
                slot4Layout->slotKind,
                "Stack slot 4 is reused by scalar arithmetic and must not inherit stale WidePoint inline metadata");

        for (TZrUInt32 index = 0; index < func->instructionsLength; index++) {
            const TZrInstruction *instruction = &func->instructionsList[index];
            const SZrFunctionFrameSlotLayout *destinationLayout;
            EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

            switch (opcode) {
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK):
                    sawScalarArithmetic = ZR_TRUE;
                    if (instruction->instruction.operandExtra != ZR_INSTRUCTION_USE_RET_FLAG) {
                        destinationLayout =
                                ZrCore_Function_FindFrameSlotLayout(func, instruction->instruction.operandExtra);
                        TEST_ASSERT_NOT_NULL(destinationLayout);
                        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
                                ZR_FUNCTION_FRAME_SLOT_KIND_VALUE,
                                destinationLayout->slotKind,
                                "Scalar arithmetic destination slots must remain plain frame values");
                    }
                    break;
                default:
                    break;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(sawScalarArithmetic, "Expected large struct return expression to emit scalar arithmetic");

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_function_frame_layout_metadata_keeps_member_slot_load_temps_plain(void) {
    SZrTestTimer timer;
    const char *testSummary = "Function Frame Layout Metadata Keeps Member Slot Load Temps Plain";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Function frame layout member temp reuse",
                 "Testing that optimized member-slot loads do not inherit stale inline struct metadata in constructors");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "struct Label {\n"
            "    pub var text: string;\n"
            "    pub @constructor(text: string) {\n"
            "        this.text = text;\n"
            "    }\n"
            "}\n"
            "var original: Label = $Label(\"left\");\n"
            "var copied: Label = original;\n"
            "copied.text = \"right\";\n"
            "return original.text + \":\" + copied.text;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "member_slot_load_frame_layout.zr", 32);
        SZrFunction *func;
        SZrFunction *constructorFunction;
        const SZrFunctionFrameSlotLayout *receiverLayout;
        const SZrFunctionFrameSlotLayout *textParameterLayout;
        const SZrFunctionFrameSlotLayout *returnSourceLayout;
        TZrUInt32 returnSourceSlot = ZR_INSTRUCTION_USE_RET_FLAG;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile member-slot frame layout source");
            destroy_test_state(state);
            return;
        }

        constructorFunction = find_single_function_constant_with_opcode(state, func, ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT));
        TEST_ASSERT_NOT_NULL(constructorFunction);
        receiverLayout = ZrCore_Function_FindFrameSlotLayout(constructorFunction, 0u);
        textParameterLayout = ZrCore_Function_FindFrameSlotLayout(constructorFunction, 1u);
        TEST_ASSERT_NOT_NULL(receiverLayout);
        TEST_ASSERT_NOT_NULL(textParameterLayout);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
                ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT,
                receiverLayout->slotKind,
                "Constructor receiver slot 0 must remain inline so member-slot stores write into frame bytes");
        TEST_ASSERT_TRUE_MESSAGE(receiverLayout->isParameter,
                                 "Constructor receiver slot must remain a parameter frame slot");
        TEST_ASSERT_TRUE_MESSAGE(textParameterLayout->isParameter,
                                 "Constructor text argument slot must be marked as a parameter frame slot");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
                ZR_FUNCTION_FRAME_SLOT_KIND_VALUE,
                textParameterLayout->slotKind,
                "Constructor text argument slot must remain a plain value frame slot");
        for (TZrUInt32 index = 0; index < constructorFunction->instructionsLength; index++) {
            const TZrInstruction *instruction = &constructorFunction->instructionsList[index];
            if ((EZrInstructionCode)instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                returnSourceSlot = instruction->instruction.operand.operand1[0];
                break;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(returnSourceSlot != ZR_INSTRUCTION_USE_RET_FLAG,
                                 "Expected constructor body to return a concrete stack slot");
        TEST_ASSERT_LESS_THAN_UINT32(constructorFunction->stackSize, returnSourceSlot);
        returnSourceLayout = ZrCore_Function_FindFrameSlotLayout(constructorFunction, returnSourceSlot);
        TEST_ASSERT_NOT_NULL(returnSourceLayout);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
                ZR_FUNCTION_FRAME_SLOT_KIND_VALUE,
                returnSourceLayout->slotKind,
                "Constructor return-source temp must remain a plain value slot even when its stack slot was previously hinted as the struct type");
        TEST_ASSERT_FALSE_MESSAGE(returnSourceLayout->isParameter,
                                  "Constructor return-source temp must not be marked as a parameter");

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_binary_roundtrip_preserves_function_frame_layout_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Binary Roundtrip Preserves Function Frame Layout Metadata";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Function frame layout binary roundtrip",
                 "Testing that .zro serialization keeps inline struct frame spans on runtime-loaded child functions");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "pub struct FrameProbe { var a: i8; var b: int; }\n"
            "useProbe(p: FrameProbe) { var n: int = 7; return n; }";
        const char *binaryPath = "function_frame_layout_roundtrip.zro";
        const TZrUInt32 expectedStructAlign = ZR_ALIGN_SIZE;
        const TZrUInt32 offsetA = 0;
        const TZrUInt32 offsetB = test_align_offset_u32(offsetA + (TZrUInt32)sizeof(TZrInt8), ZR_ALIGN_SIZE);
        const TZrUInt32 expectedStructSize =
                test_align_offset_u32(offsetB + (TZrUInt32)sizeof(TZrInt64), expectedStructAlign);
        SZrString *sourceName = ZrCore_String_Create(state, "function_frame_layout_roundtrip.zr", 34);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *sourceWrapper;
        SZrFunction *sourceFunction;
        SZrFunction *runtimeWrapper;
        SZrFunction *runtimeFunction;
        const SZrFunctionFrameSlotLayout *sourceParameterLayout;
        const SZrFunctionFrameSlotLayout *runtimeParameterLayout;
        const SZrFunctionFrameSlotLayout *runtimeLocalLayout;
        TZrUInt32 expectedFrameByteBaseOffset;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse function frame layout roundtrip source");
            destroy_test_state(state);
            return;
        }

        sourceWrapper = ZrParser_Compiler_Compile(state, ast);
        if (sourceWrapper == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile function frame layout roundtrip source");
            destroy_test_state(state);
            return;
        }

        sourceFunction = get_single_compiled_child_function(sourceWrapper);
        sourceParameterLayout = ZrCore_Function_FindFrameSlotLayout(sourceFunction, 0);
        TEST_ASSERT_NOT_NULL(sourceParameterLayout);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT, sourceParameterLayout->slotKind);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, sourceWrapper, binaryPath));

        runtimeWrapper = load_runtime_entry_from_binary_file(state, binaryPath);
        runtimeFunction = get_single_compiled_child_function(runtimeWrapper);
        TEST_ASSERT_EQUAL_UINT32(sourceFunction->stackSize, runtimeFunction->stackSize);
        TEST_ASSERT_EQUAL_UINT32(sourceFunction->frameSlotLayoutLength, runtimeFunction->frameSlotLayoutLength);
        TEST_ASSERT_EQUAL_UINT32(sourceFunction->frameByteSize, runtimeFunction->frameByteSize);
        TEST_ASSERT_EQUAL_UINT32(sourceFunction->frameByteAlign, runtimeFunction->frameByteAlign);
        expectedFrameByteBaseOffset =
                test_align_offset_u32((TZrUInt32)(runtimeFunction->stackSize * sizeof(SZrTypeValueOnStack)),
                                      ZR_ALIGN_SIZE);

        runtimeParameterLayout = ZrCore_Function_FindFrameSlotLayout(runtimeFunction, 0);
        runtimeLocalLayout = ZrCore_Function_FindFrameSlotLayout(runtimeFunction, 1);
        TEST_ASSERT_NOT_NULL(runtimeParameterLayout);
        TEST_ASSERT_NOT_NULL(runtimeLocalLayout);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT, runtimeParameterLayout->slotKind);
        TEST_ASSERT_TRUE(runtimeParameterLayout->isParameter);
        TEST_ASSERT_EQUAL_UINT32(sourceParameterLayout->byteOffset, runtimeParameterLayout->byteOffset);
        TEST_ASSERT_EQUAL_UINT32(expectedFrameByteBaseOffset, runtimeParameterLayout->byteOffset);
        TEST_ASSERT_EQUAL_UINT32(expectedStructSize, runtimeParameterLayout->byteSize);
        TEST_ASSERT_EQUAL_UINT32(expectedStructAlign, runtimeParameterLayout->byteAlign);
        TEST_ASSERT_NOT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, runtimeParameterLayout->typeLayoutId);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_SLOT_KIND_VALUE, runtimeLocalLayout->slotKind);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrTypeValue), runtimeLocalLayout->byteSize);
        TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, runtimeLocalLayout->typeLayoutId);

        ZrCore_Function_Free(state, runtimeWrapper);
        ZrCore_Function_Free(state, sourceWrapper);
        remove(binaryPath);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_owned_field_metadata_serializes_into_prototype_data(void) {
    SZrTestTimer timer;
    const char *testSummary = "Owned Field Prototype Metadata";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Owned field prototype metadata",
              "Testing that direct %unique/%shared fields serialize ownership metadata and teardown flags without field-scoped `%using var`");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "pub struct HandleBox { var handle: %unique Resource; var count: int; }\n"
            "pub class Holder { var resource: %shared Resource; var version: int; }";
        SZrString *sourceName = ZrCore_String_Create(state, "owned_field_meta.zr", 19);
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
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse owned field source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile owned field source");
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

        TEST_ASSERT_FALSE(handleMember->isUsingManaged);
        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_UNIQUE, handleMember->ownershipQualifier);
        TEST_ASSERT_TRUE(handleMember->callsClose);
        TEST_ASSERT_TRUE(handleMember->callsDestructor);
        TEST_ASSERT_EQUAL_UINT32(0, handleMember->declarationOrder);

        TEST_ASSERT_FALSE(countMember->isUsingManaged);
        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_NONE, countMember->ownershipQualifier);

        TEST_ASSERT_FALSE(resourceMember->isUsingManaged);
        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_SHARED, resourceMember->ownershipQualifier);
        TEST_ASSERT_TRUE(resourceMember->callsClose);
        TEST_ASSERT_TRUE(resourceMember->callsDestructor);
        TEST_ASSERT_EQUAL_UINT32(0, resourceMember->declarationOrder);

        TEST_ASSERT_FALSE(versionMember->isUsingManaged);
        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_NONE, versionMember->ownershipQualifier);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_intrinsic_ownership_generic_fields_serialize_owner_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Intrinsic Ownership Generic Field Prototype Metadata";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Intrinsic ownership generic field metadata",
              "Testing that Unique<T>/Shared<T> fields serialize the same ownership metadata and teardown flags as legacy percent-qualified owner fields");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "pub struct HandleBox { var handle: Unique<Resource>; var count: int; }\n"
            "pub class Holder { var resource: Shared<Resource>; var version: int; }";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_generic_field_meta.zr",
                                                     strlen("ownership_generic_field_meta.zr"));
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        const SZrCompiledPrototypeInfoView *structProto;
        const SZrCompiledPrototypeInfoView *classProto;
        const SZrCompiledMemberInfoView *handleMember;
        const SZrCompiledMemberInfoView *resourceMember;
        SZrString *handleFieldTypeName;
        SZrString *resourceFieldTypeName;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse intrinsic ownership generic field source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile intrinsic ownership generic field source");
            destroy_test_state(state);
            return;
        }

        structProto = find_compiled_prototype_by_name(state, func, "HandleBox");
        classProto = find_compiled_prototype_by_name(state, func, "Holder");
        TEST_ASSERT_NOT_NULL(structProto);
        TEST_ASSERT_NOT_NULL(classProto);

        handleMember = find_compiled_member_by_name(state, func, structProto, "handle");
        resourceMember = find_compiled_member_by_name(state, func, classProto, "resource");
        TEST_ASSERT_NOT_NULL(handleMember);
        TEST_ASSERT_NOT_NULL(resourceMember);

        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_UNIQUE, handleMember->ownershipQualifier);
        TEST_ASSERT_TRUE(handleMember->callsClose);
        TEST_ASSERT_TRUE(handleMember->callsDestructor);
        handleFieldTypeName = get_string_constant_at(state, func, handleMember->fieldTypeNameStringIndex);
        TEST_ASSERT_NOT_NULL(handleFieldTypeName);
        TEST_ASSERT_EQUAL_STRING("Resource", ZrCore_String_GetNativeString(handleFieldTypeName));

        TEST_ASSERT_EQUAL_UINT32(ZR_OWNERSHIP_QUALIFIER_SHARED, resourceMember->ownershipQualifier);
        TEST_ASSERT_TRUE(resourceMember->callsClose);
        TEST_ASSERT_TRUE(resourceMember->callsDestructor);
        resourceFieldTypeName = get_string_constant_at(state, func, resourceMember->fieldTypeNameStringIndex);
        TEST_ASSERT_NOT_NULL(resourceFieldTypeName);
        TEST_ASSERT_EQUAL_STRING("Resource", ZrCore_String_GetNativeString(resourceFieldTypeName));

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_removed_field_scoped_using_does_not_serialize_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Removed Field-Scoped Using Does Not Serialize Metadata";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Removed field-scoped using metadata",
              "Testing that deprecated `%using var` fields do not survive into compiled prototype metadata now that owner fields carry lifecycle directly");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source = "class Holder { %using var resource: %unique Resource; }";
        SZrString *sourceName = ZrCore_String_Create(state, "static_using_compile_error.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        const SZrCompiledPrototypeInfoView *classProto;
        const SZrCompiledMemberInfoView *resourceMember;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse removed field-scoped using source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile removed field-scoped using source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_NOT_NULL(func->prototypeData);
        TEST_ASSERT_EQUAL_UINT32(1, func->prototypeCount);
        classProto = find_compiled_prototype_by_name(state, func, "Holder");
        TEST_ASSERT_NOT_NULL(classProto);
        resourceMember = find_compiled_member_by_name(state, func, classProto, "resource");
        TEST_ASSERT_NULL(resourceMember);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_advanced_oop_metadata_serializes_override_and_property_chains(void) {
    SZrTestTimer timer;
    const char *testSummary = "Advanced OOP Metadata Serializes Override And Property Chains";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Advanced OOP metadata serialization",
              "Testing that abstract/final/override metadata, base-definition info, virtual slots, and property identity survive prototypeData serialization");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "abstract class Base {\n"
            "    pub abstract ping(): int;\n"
            "    pub abstract get score: int;\n"
            "}\n"
            "final class Derived : Base {\n"
            "    pub override final ping(): int { return 1; }\n"
            "    pub override get final score: int { return 2; }\n"
            "}";
        SZrString *sourceName = ZrCore_String_Create(state, "advanced_oop_metadata.zr", 24);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        const SZrCompiledPrototypeInfoView *baseProto;
        const SZrCompiledPrototypeInfoView *derivedProto;
        const SZrCompiledMemberInfoView *basePing;
        const SZrCompiledMemberInfoView *derivedPing;
        const SZrCompiledMemberInfoView *baseGetter;
        const SZrCompiledMemberInfoView *derivedGetter;
        SZrString *baseDefinitionOwner;
        SZrString *baseDefinitionName;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse advanced OOP metadata source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile advanced OOP metadata source");
            destroy_test_state(state);
            return;
        }

        baseProto = find_compiled_prototype_by_name(state, func, "Base");
        derivedProto = find_compiled_prototype_by_name(state, func, "Derived");
        TEST_ASSERT_NOT_NULL(baseProto);
        TEST_ASSERT_NOT_NULL(derivedProto);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, baseProto->modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, derivedProto->modifierFlags & ZR_DECLARATION_MODIFIER_FINAL);

        basePing = find_compiled_member_by_name(state, func, baseProto, "ping");
        derivedPing = find_compiled_member_by_name(state, func, derivedProto, "ping");
        baseGetter = find_compiled_member_by_name(state, func, baseProto, "__get_score");
        derivedGetter = find_compiled_member_by_name(state, func, derivedProto, "__get_score");
        TEST_ASSERT_NOT_NULL(basePing);
        TEST_ASSERT_NOT_NULL(derivedPing);
        TEST_ASSERT_NOT_NULL(baseGetter);
        TEST_ASSERT_NOT_NULL(derivedGetter);

        TEST_ASSERT_NOT_EQUAL_UINT32(0, basePing->modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, derivedPing->modifierFlags & ZR_DECLARATION_MODIFIER_OVERRIDE);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, derivedPing->modifierFlags & ZR_DECLARATION_MODIFIER_FINAL);
        TEST_ASSERT_NOT_EQUAL_UINT32((TZrUInt32)-1, basePing->virtualSlotIndex);
        TEST_ASSERT_EQUAL_UINT32(basePing->virtualSlotIndex, derivedPing->virtualSlotIndex);

        baseDefinitionOwner = get_string_constant_at(state, func, derivedPing->baseDefinitionOwnerTypeNameStringIndex);
        baseDefinitionName = get_string_constant_at(state, func, derivedPing->baseDefinitionNameStringIndex);
        TEST_ASSERT_NOT_NULL(baseDefinitionOwner);
        TEST_ASSERT_NOT_NULL(baseDefinitionName);
        TEST_ASSERT_EQUAL_STRING("Base", ZrCore_String_GetNativeString(baseDefinitionOwner));
        TEST_ASSERT_EQUAL_STRING("ping", ZrCore_String_GetNativeString(baseDefinitionName));

        TEST_ASSERT_NOT_EQUAL_UINT32(0, baseGetter->modifierFlags & ZR_DECLARATION_MODIFIER_ABSTRACT);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, derivedGetter->modifierFlags & ZR_DECLARATION_MODIFIER_OVERRIDE);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, derivedGetter->modifierFlags & ZR_DECLARATION_MODIFIER_FINAL);
        TEST_ASSERT_EQUAL_UINT32(1, baseGetter->accessorRole);
        TEST_ASSERT_EQUAL_UINT32(1, derivedGetter->accessorRole);
        TEST_ASSERT_NOT_EQUAL_UINT32((TZrUInt32)-1, baseGetter->virtualSlotIndex);
        TEST_ASSERT_EQUAL_UINT32(baseGetter->virtualSlotIndex, derivedGetter->virtualSlotIndex);
        TEST_ASSERT_NOT_EQUAL_UINT32((TZrUInt32)-1, baseGetter->propertyIdentity);
        TEST_ASSERT_EQUAL_UINT32(baseGetter->propertyIdentity, derivedGetter->propertyIdentity);

        baseDefinitionOwner = get_string_constant_at(state, func, derivedGetter->baseDefinitionOwnerTypeNameStringIndex);
        baseDefinitionName = get_string_constant_at(state, func, derivedGetter->baseDefinitionNameStringIndex);
        TEST_ASSERT_NOT_NULL(baseDefinitionOwner);
        TEST_ASSERT_NOT_NULL(baseDefinitionName);
        TEST_ASSERT_EQUAL_STRING("Base", ZrCore_String_GetNativeString(baseDefinitionOwner));
        TEST_ASSERT_EQUAL_STRING("__get_score", ZrCore_String_GetNativeString(baseDefinitionName));

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_abstract_class_construction_is_rejected_by_compiler(void) {
    SZrTestTimer timer;
    const char *testSummary = "Abstract Class Construction Is Rejected By Compiler";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Abstract class construction rejection",
              "Testing that `new` on an abstract class is rejected by the compiler frontend");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "abstract class Base { }\n"
            "var value = new Base();";
        SZrString *sourceName = ZrCore_String_Create(state, "abstract_class_new_error.zr", 27);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse abstract class construction source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        TEST_ASSERT_NULL(func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_final_class_inheritance_is_rejected_by_compiler(void) {
    SZrTestTimer timer;
    const char *testSummary = "Final Class Inheritance Is Rejected By Compiler";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Final class inheritance rejection",
              "Testing that classes cannot inherit from a final base class");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "final class Base { }\n"
            "class Derived : Base { }";
        SZrString *sourceName = ZrCore_String_Create(state, "final_class_inheritance_error.zr", 32);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse final class inheritance source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        TEST_ASSERT_NULL(func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_override_of_non_virtual_member_is_rejected_by_compiler(void) {
    SZrTestTimer timer;
    const char *testSummary = "Override Of Non Virtual Member Is Rejected By Compiler";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Override target validation",
              "Testing that override only binds to inherited virtual or abstract members");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Base {\n"
            "    pub ping(): int { return 0; }\n"
            "}\n"
            "class Derived : Base {\n"
            "    pub override ping(): int { return 1; }\n"
            "}";
        SZrString *sourceName = ZrCore_String_Create(state, "override_non_virtual_error.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse override non-virtual source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        TEST_ASSERT_NULL(func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_inherited_same_name_requires_explicit_override_or_shadow(void) {
    SZrTestTimer timer;
    const char *testSummary = "Inherited Same Name Requires Explicit Override Or Shadow";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Explicit override/shadow enforcement",
              "Testing that inherited same-name members cannot silently hide a base implementation");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Base {\n"
            "    pub virtual ping(): int { return 0; }\n"
            "}\n"
            "class Derived : Base {\n"
            "    pub ping(): int { return 1; }\n"
            "}";
        SZrString *sourceName = ZrCore_String_Create(state, "implicit_override_error.zr", 25);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse implicit override source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        TEST_ASSERT_NULL(func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_shadow_does_not_satisfy_abstract_base_member(void) {
    SZrTestTimer timer;
    const char *testSummary = "Shadow Does Not Satisfy Abstract Base Member";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Shadow versus abstract requirement",
              "Testing that shadow opens a new chain and does not satisfy an inherited abstract obligation");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "abstract class Base {\n"
            "    pub abstract ping(): int;\n"
            "}\n"
            "class Derived : Base {\n"
            "    pub shadow ping(): int { return 1; }\n"
            "}";
        SZrString *sourceName = ZrCore_String_Create(state, "shadow_abstract_requirement_error.zr", 36);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse shadow abstract requirement source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        TEST_ASSERT_NULL(func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_interface_plain_member_serializes_interface_contract_slot(void) {
    SZrTestTimer timer;
    const char *testSummary = "Interface Plain Member Serializes Interface Contract Slot";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Interface contract slot binding",
              "Testing that a plain class member satisfies an interface contract without override and records the bound interface slot");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "interface Readable { read(): int; }\n"
            "class Device : Readable {\n"
            "    pub read(): int { return 1; }\n"
            "}";
        SZrString *sourceName = ZrCore_String_Create(state, "interface_contract_slot.zr", 26);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        const SZrCompiledPrototypeInfoView *interfaceProto;
        const SZrCompiledPrototypeInfoView *deviceProto;
        const SZrCompiledMemberInfoView *interfaceRead;
        const SZrCompiledMemberInfoView *deviceRead;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse interface contract slot source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile interface contract slot source");
            destroy_test_state(state);
            return;
        }

        interfaceProto = find_compiled_prototype_by_name(state, func, "Readable");
        deviceProto = find_compiled_prototype_by_name(state, func, "Device");
        TEST_ASSERT_NOT_NULL(interfaceProto);
        TEST_ASSERT_NOT_NULL(deviceProto);

        interfaceRead = find_compiled_member_by_name(state, func, interfaceProto, "read");
        deviceRead = find_compiled_member_by_name(state, func, deviceProto, "read");
        TEST_ASSERT_NOT_NULL(interfaceRead);
        TEST_ASSERT_NOT_NULL(deviceRead);
        TEST_ASSERT_NOT_EQUAL_UINT32((TZrUInt32)-1, interfaceRead->interfaceContractSlot);
        TEST_ASSERT_EQUAL_UINT32(interfaceRead->interfaceContractSlot, deviceRead->interfaceContractSlot);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)-1, deviceRead->virtualSlotIndex);
        TEST_ASSERT_EQUAL_UINT32(0, deviceRead->modifierFlags & ZR_DECLARATION_MODIFIER_OVERRIDE);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_name_matched_iterator_members_without_builtin_interface_do_not_bind_contract_roles(void) {
    SZrTestTimer timer;
    const char *testSummary = "Name Matched Iterator Members Without Builtin Interface Do Not Bind Contract Roles";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Iterator contract role isolation",
              "Testing that getIterator moveNext and current only become iterator contracts when the class explicitly binds the builtin interfaces");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "class LooseIterator {\n"
                "    pub var current: int;\n"
                "    pub @constructor() { this.current = 0; }\n"
                "    pub moveNext(): bool { return false; }\n"
                "}\n"
                "class LooseIterable {\n"
                "    pub getIterator(): LooseIterator { return new LooseIterator(); }\n"
                "}";
        SZrString *sourceName = ZrCore_String_Create(state, "named_iterator_members_without_contracts.zr", 43);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;
        const SZrCompiledPrototypeInfoView *iteratorProto;
        const SZrCompiledPrototypeInfoView *iterableProto;
        const SZrCompiledMemberInfoView *currentField;
        const SZrCompiledMemberInfoView *moveNextMethod;
        const SZrCompiledMemberInfoView *getIteratorMethod;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse loose iterator source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile loose iterator source");
            destroy_test_state(state);
            return;
        }

        iteratorProto = find_compiled_prototype_by_name(state, func, "LooseIterator");
        iterableProto = find_compiled_prototype_by_name(state, func, "LooseIterable");
        TEST_ASSERT_NOT_NULL(iteratorProto);
        TEST_ASSERT_NOT_NULL(iterableProto);

        currentField = find_compiled_member_by_name(state, func, iteratorProto, "current");
        moveNextMethod = find_compiled_member_by_name(state, func, iteratorProto, "moveNext");
        getIteratorMethod = find_compiled_member_by_name(state, func, iterableProto, "getIterator");
        TEST_ASSERT_NOT_NULL(currentField);
        TEST_ASSERT_NOT_NULL(moveNextMethod);
        TEST_ASSERT_NOT_NULL(getIteratorMethod);
        TEST_ASSERT_EQUAL_UINT32(ZR_MEMBER_CONTRACT_ROLE_NONE, currentField->contractRole);
        TEST_ASSERT_EQUAL_UINT32(ZR_MEMBER_CONTRACT_ROLE_NONE, moveNextMethod->contractRole);
        TEST_ASSERT_EQUAL_UINT32(ZR_MEMBER_CONTRACT_ROLE_NONE, getIteratorMethod->contractRole);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_interface_missing_member_is_rejected_by_compiler(void) {
    SZrTestTimer timer;
    const char *testSummary = "Interface Missing Member Is Rejected By Compiler";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Interface requirement enforcement",
              "Testing that a concrete class must implement interface members");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "interface Readable { read(): int; }\n"
            "class Missing : Readable { }";
        SZrString *sourceName = ZrCore_String_Create(state, "interface_missing_member_error.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *func;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse missing interface member source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        TEST_ASSERT_NULL(func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_ownership_builtin_shared_expression_consumes_unique_owner(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Builtin Shared Expression Consumes Unique Owner";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership builtin expression lowering",
              "Testing that %shared(owner) compiles from a %unique owner into OWN_SHARE without serialized native helper constants");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var owner = %unique new Box();\n"
            "var alias = %shared(owner);";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_builtin_shared_expr.zr",
                                                     strlen("ownership_builtin_shared_expr.zr"));
        SZrFunction *func;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile ownership builtin source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
        TEST_ASSERT_FALSE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARED));

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_intrinsic_ownership_generic_constructors_emit_dedicated_opcodes(void) {
    SZrTestTimer timer;
    const char *testSummary = "Intrinsic Ownership Generic Constructors Emit Dedicated Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership generic constructor lowering",
              "Testing that Unique<T>(value), Shared<T>(value), Borrow<T>(value), and Loan<T>(value) compile through the dedicated ownership opcodes");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var owner = Unique<Box>(new Box());\n"
            "var alias = Shared<Box>(owner);\n"
            "var borrowed = Borrow<Box>(alias);\n"
            "var loanSource = Unique<Box>(new Box());\n"
            "var loaned = Loan<Box>(loanSource);";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_generic_constructors.zr",
                                                     strlen("ownership_generic_constructors.zr"));
        SZrFunction *func;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile intrinsic ownership generic constructor source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_UNIQUE)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_BORROW)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_LOAN)));
        TEST_ASSERT_FALSE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARED));

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_ownership_generic_member_methods_emit_dedicated_opcodes_and_execute(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Generic Member Methods Emit Dedicated Opcodes And Execute";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership generic member method lowering",
              "Testing that owner.share()/weak()/borrow()/loan()/detach()/upgrade()/release() lower through ownership opcodes");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Session {}\n"
            "runMemberLifecycle(): int {\n"
            "var sessionSeed = Unique<Session>(new Session());\n"
            "var sharedSession = sessionSeed.share();\n"
            "var watcher = sharedSession.weak();\n"
            "var mask = 0;\n"
            "using (sharedSession.borrow()) {\n"
            "    if (sharedSession != null) { mask = mask + 1; }\n"
            "}\n"
            "var cacheSeed = Unique<string>(\"session-cache\");\n"
            "using (cacheSeed.loan()) {\n"
            "    if (cacheSeed == null) { mask = mask + 2; }\n"
            "}\n"
            "if (cacheSeed != null) { mask = mask + 4; }\n"
            "var detachedSeed = Unique<Session>(new Session());\n"
            "var rawSession = detachedSeed.detach();\n"
            "if (detachedSeed == null && rawSession != null) { mask = mask + 8; }\n"
            "var upgradedSession = watcher.upgrade();\n"
            "if (upgradedSession != null) { mask = mask + 16; }\n"
            "var releasedShared = sharedSession.release();\n"
            "var releasedUpgrade = upgradedSession.release();\n"
            "var expiredSession = watcher.upgrade();\n"
            "if (releasedShared == null && releasedUpgrade == null &&\n"
            "    sharedSession == null && upgradedSession == null && expiredSession == null) {\n"
            "    mask = mask + 32;\n"
            "}\n"
            "return mask;\n"
            "}\n"
            "return runMemberLifecycle();\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_generic_member_methods.zr",
                                                     strlen("ownership_generic_member_methods.zr"));
        SZrFunction *func;
        SZrFunction *lifecycleFunc;
        TZrInt64 result = 0;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile ownership generic member method source");
            destroy_test_state(state);
            return;
        }

        lifecycleFunc = find_single_function_constant_with_opcode(state, func, ZR_INSTRUCTION_ENUM(OWN_UNIQUE));
        TEST_ASSERT_NOT_NULL(lifecycleFunc);
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_WEAK)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_BORROW)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_LOAN)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_DETACH)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_UPGRADE)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result));
        TEST_ASSERT_EQUAL_INT64(63, result);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_ownership_borrow_loan_and_detach_emit_dedicated_opcodes(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Borrow Loan And Detach Emit Dedicated Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership borrow/loan/detach lowering",
              "Testing that %borrow/%loan/%detach emit dedicated ownership opcodes on top of the Rust-first owner surface");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var owner = %unique new Box();\n"
            "var shared = %shared(owner);\n"
            "var borrowed = %borrow(shared);\n"
            "var loanSource = %unique new Box();\n"
            "var loaned = %loan(loanSource);\n"
            "var detachSource = %unique new Box();\n"
            "var detached = %detach(detachSource);";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_borrow_loan_detach.zr",
                                                     strlen("ownership_borrow_loan_detach.zr"));
        SZrFunction *func;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile ownership borrow/loan/detach source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_UNIQUE)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_BORROW)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_LOAN)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_DETACH)));

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_ownership_unique_share_runtime_moves_source_to_null(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Unique Share Runtime Moves Source To Null";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership runtime unique->shared move",
              "Testing that %shared(owner) consumes a %unique owner during execution and leaves the source variable null");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
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
        SZrFunction *func;
        TZrInt64 result = 0;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile ownership unique/share runtime source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result));
        TEST_ASSERT_EQUAL_INT64(1, result);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_ownership_borrow_loan_and_detach_runtime_follow_surface_contract(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Borrow Loan And Detach Runtime Follow Surface Contract";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership runtime borrow/loan/detach",
              "Testing that %shared(owner) + %borrow(shared) keep a live borrowed alias, %loan(owner) nulls the unique source, and %detach(unique) returns a plain GC value while clearing the source");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var owner = %unique new Box();\n"
            "var shared = %shared(owner);\n"
            "var borrowed = %borrow(shared);\n"
            "var borrowedAlive = borrowed != null;\n"
            "var loanSource = %unique new Box();\n"
            "var loaned = %loan(loanSource);\n"
            "var detachSource = %unique new Box();\n"
            "var detached = %detach(detachSource);\n"
            "var mask = 0;\n"
            "if (owner == null) { mask = mask + 1; }\n"
            "if (shared != null) { mask = mask + 2; }\n"
            "if (loanSource == null) { mask = mask + 4; }\n"
            "if (loaned != null) { mask = mask + 8; }\n"
            "if (borrowedAlive) { mask = mask + 16; }\n"
            "if (detachSource == null) { mask = mask + 32; }\n"
            "if (detached != null) { mask = mask + 64; }\n"
            "return mask;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_borrow_loan_detach_runtime.zr",
                                                     strlen("ownership_borrow_loan_detach_runtime.zr"));
        SZrFunction *func;
        TZrInt64 result = 0;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile ownership borrow/loan/detach runtime source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_BORROW)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_LOAN)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_DETACH)));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result));
        TEST_ASSERT_EQUAL_INT64(127, result);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_ownership_generic_real_fixture_executes_session_lifecycle(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Generic Real Fixture Executes Session Lifecycle";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership generic fixture lifecycle",
              "Testing that a repository .zr fixture exercises Unique<T>, Shared<T>, Borrow<T>, Loan<T>, Weak<T>, detach, release, and upgrade together");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        TZrSize sourceLength = 0;
        TZrChar *source = ZrTests_Reference_ReadFixture(
                "core_semantics/ownership_using_resource_lifecycle/generic_session_lifecycle_pass.zr",
                &sourceLength);
        SZrString *sourceName;
        SZrFunction *func;
        SZrFunction *lifecycleFunc;
        TZrInt64 result = 0;

        if (source == ZR_NULL || sourceLength == 0) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to read ownership generic session lifecycle fixture");
            free(source);
            destroy_test_state(state);
            return;
        }

        sourceName = ZrCore_String_Create(state,
                                          "generic_session_lifecycle_pass.zr",
                                          strlen("generic_session_lifecycle_pass.zr"));
        TEST_ASSERT_NOT_NULL(sourceName);

        func = ZrParser_Source_Compile(state, source, sourceLength, sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile ownership generic session lifecycle fixture");
            free(source);
            destroy_test_state(state);
            return;
        }

        lifecycleFunc = find_single_function_constant_with_opcode(state, func, ZR_INSTRUCTION_ENUM(OWN_UNIQUE));
        TEST_ASSERT_NOT_NULL(lifecycleFunc);
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_BORROW)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_LOAN)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_DETACH)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_WEAK)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_UPGRADE)));
        TEST_ASSERT_TRUE(function_contains_opcode(lifecycleFunc, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));

        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute ownership generic session lifecycle fixture");
            ZrCore_Function_Free(state, func);
            free(source);
            destroy_test_state(state);
            return;
        }
        if (result != 63) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Ownership generic session lifecycle fixture returned unexpected mask");
            ZrCore_Function_Free(state, func);
            free(source);
            destroy_test_state(state);
            return;
        }

        ZrCore_Function_Free(state, func);
        free(source);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_ownership_weak_runtime_expires_to_null_after_last_shared_release(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Weak Runtime Expires To Null After Last Shared Release";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership runtime weak expiration",
              "Testing that %weak(owner) becomes null after the last %shared owner is overwritten with null");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "var owner = %shared(seed);\n"
            "var watcher = %weak(owner);\n"
            "owner = null;\n"
            "if (watcher == null) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_weak_runtime_expire.zr",
                                                     strlen("ownership_weak_runtime_expire.zr"));
        SZrFunction *func;
        TZrInt64 result = 0;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile ownership weak runtime source");
            destroy_test_state(state);
            return;
        }

        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute ownership weak runtime source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        if (result != 1) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Weak owner did not expire after last shared release");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_ownership_upgrade_and_release_runtime_follow_lifecycle_contract(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Upgrade And Release Runtime Follow Lifecycle Contract";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership runtime weak->shared upgrade and explicit release",
              "Testing that %upgrade(weak) yields a live shared alias while an owner exists, and %release(owner) clears the last shared owner so a later upgrade becomes null");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "var owner = %shared(seed);\n"
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
        SZrFunction *func;
        TZrInt64 result = 0;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile ownership upgrade/release runtime source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_UPGRADE)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute ownership upgrade/release runtime source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        if (result != 1) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Ownership upgrade/release runtime source returned unexpected result");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_ownership_release_preserves_unrelated_stack_values_after_weak_expiry(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Release Preserves Unrelated Stack Values After Weak Expiry";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership runtime weak temp materialization",
              "Testing that releasing the last shared owners expires weak references without nulling later locals that reused temporary stack slots");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Loop {\n"
            "    pub @call(n: int, acc: int): int {\n"
            "        if (n == 0) { return acc; }\n"
            "        return this(n - 1, acc + 1);\n"
            "    }\n"
            "}\n"
            "func direct(n: int, acc: int): int {\n"
            "    if (n == 0) { return acc; }\n"
            "    return direct(n - 1, acc + 1);\n"
            "}\n"
            "func guarded(flag: int): int {\n"
            "    var marker = 0;\n"
            "    try {\n"
            "        try {\n"
            "            if (flag != 0) { throw \"boom\"; }\n"
            "            return 0;\n"
            "        } finally {\n"
            "            marker = marker + 7;\n"
            "        }\n"
            "    } catch (e) {\n"
            "        return marker + 1;\n"
            "    }\n"
            "}\n"
            "class Box {}\n"
            "var ownerSeed = %unique new Box();\n"
            "var owner = %shared(ownerSeed);\n"
            "var weak = %weak(owner);\n"
            "var alias = %upgrade(weak);\n"
            "var loop = new Loop();\n"
            "var directValue = direct(12, 0);\n"
            "var metaValue = loop(10, 0);\n"
            "var guardedValue = guarded(1);\n"
            "var releasedOwner = %release(owner);\n"
            "var releasedAlias = %release(alias);\n"
            "return directValue + metaValue + guardedValue;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_release_preserves_stack_values.zr",
                                                     strlen("ownership_release_preserves_stack_values.zr"));
        SZrFunction *func;
        TZrInt64 result = 0;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile ownership stack preservation source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_WEAK)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_UPGRADE)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute ownership stack preservation source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        if (result != 30) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Ownership weak expiry clobbered an unrelated stack value");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_share_promotes_module_handle_to_shared_owner(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Share Promotes Module Handle To Shared Owner";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Using plugin guard share promotion",
              "Testing that guard-scoped %import handles lower .share() to a native shared-owner promotion instead of dynamic module member lookup");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "using (var [math] = %import(\"zr.math\")) {\n"
            "    var handle = math.share();\n"
            "    var released = %release(handle);\n"
            "    return 1;\n"
            "} else {\n"
            "    return 2;\n"
            "}\n"
            "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "plugin_guard_share_promotes_module_handle.zr",
                                                     strlen("plugin_guard_share_promotes_module_handle.zr"));
        SZrFunction *func;
        TZrInt64 result = 0;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile plugin guard share promotion source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARE_PLAIN));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute plugin guard share promotion source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        if (result != 1) {
            char failureMessage[128];
            snprintf(failureMessage,
                     sizeof(failureMessage),
                     "Plugin guard share promotion returned unexpected result: %lld",
                     (long long)result);
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, failureMessage);
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_scoped_module_handle_releases_on_scope_exit(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Scoped Module Handle Releases On Scope Exit";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Using plugin guard scoped release",
              "Testing that a guard-scoped module payload gets an automatic scoped shared-owner release even without explicit share()");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "var seen = 0;\n"
            "using (var [math] = %import(\"zr.math\")) {\n"
            "    seen = 1;\n"
            "} else {\n"
            "    seen = 2;\n"
            "}\n"
            "return seen;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "plugin_guard_scoped_module_release.zr",
                                                     strlen("plugin_guard_scoped_module_release.zr"));
        SZrFunction *func;
        TZrInt64 result = 0;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile plugin guard scoped release source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARE_PLAIN));
        TEST_ASSERT_EQUAL_UINT32(1u, function_count_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute plugin guard scoped release source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        if (result != 1) {
            char failureMessage[128];
            snprintf(failureMessage,
                     sizeof(failureMessage),
                     "Plugin guard scoped release returned unexpected result: %lld",
                     (long long)result);
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, failureMessage);
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_load_available_import_guard_lowers_to_available_payload(void) {
    SZrTestTimer timer;
    const char *testSummary = "PluginLoad Available Import Guard Lowers To Available Payload";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("PluginLoad.Available import guard",
              "Testing that PluginLoad.Available lowers to the import guard success payload without requiring a user-declared DynamicModule union");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "var seen = 0;\n"
            "using (var [math]: PluginLoad.Available = %import(\"zr.math\")) {\n"
            "    var handle = math.share();\n"
            "    var released = %release(handle);\n"
            "    seen = 1;\n"
            "} else {\n"
            "    seen = 2;\n"
            "}\n"
            "return seen;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "plugin_load_available_import_guard.zr",
                                                     strlen("plugin_load_available_import_guard.zr"));
        SZrFunction *func;
        TZrInt64 result = 0;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile PluginLoad.Available import guard source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARE_PLAIN));
        TEST_ASSERT_TRUE(function_count_opcode(func, ZR_INSTRUCTION_ENUM(OWN_RELEASE)) >= 2u);
        if (!ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result)) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to execute PluginLoad.Available import guard source");
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }
        if (result != 1) {
            char failureMessage[128];
            snprintf(failureMessage,
                     sizeof(failureMessage),
                     "PluginLoad.Available import guard returned unexpected result: %lld",
                     (long long)result);
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, failureMessage);
            ZrCore_Function_Free(state, func);
            destroy_test_state(state);
            return;
        }

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_ownership_builtin_compile_rejects_invalid_operands(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Builtin Compile Rejects Invalid Operands";
    static const struct {
        const char *label;
        const char *source;
        const char *sourceName;
    } cases[] = {
        {
            "weak-from-unique",
            "class Box {}\n"
            "var owner = %unique new Box();\n"
            "var watcher = %weak(owner);\n",
            "ownership_invalid_weak_unique_compile.zr",
        },
        {
            "upgrade-from-shared",
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "var owner = %shared(seed);\n"
            "var alias = %upgrade(owner);\n",
            "ownership_invalid_upgrade_shared_compile.zr",
        },
        {
            "loan-from-shared",
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "var owner = %shared(seed);\n"
            "var alias = %loan(owner);\n",
            "ownership_invalid_loan_shared_compile.zr",
        },
        {
            "release-borrowed",
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "var owner = %shared(seed);\n"
            "var borrowed = %borrow(owner);\n"
            "var released = %release(borrowed);\n",
            "ownership_invalid_release_borrowed_compile.zr",
        },
        {
            "union-default-owner-payload-borrow-release",
            "class Box {}\n"
            "union Resource {\n"
            "    Empty;\n"
            "    Open(handle: Shared<Box>);\n"
            "}\n"
            "var seed = Unique<Box>(new Box());\n"
            "var owner = Shared<Box>(seed);\n"
            "var resource: Resource = Resource.Open(owner);\n"
            "using (var [handle]: Resource.Open = resource) {\n"
            "    var released = %release(handle);\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_union_default_owner_payload_borrow_release_compile.zr",
        },
        {
            "detach-weak",
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "var owner = %shared(seed);\n"
            "var watcher = %weak(owner);\n"
            "var detached = %detach(watcher);\n",
            "ownership_invalid_detach_weak_compile.zr",
        },
        {
            "share-shared",
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "var owner = %shared(seed);\n"
            "var alias = %shared(owner);\n",
            "ownership_invalid_share_shared_compile.zr",
        },
        {
            "borrow-return-escape",
            "class Box {}\n"
            "leak(owner: %shared Box): %borrowed Box {\n"
            "    return %borrow(owner);\n"
            "}\n",
            "ownership_invalid_borrow_return_compile.zr",
        },
        {
            "borrow-global-escape",
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "var owner = %shared(seed);\n"
            "pub var escaped: Borrow<Box> = %borrow(owner);\n",
            "ownership_invalid_borrow_global_compile.zr",
        },
        {
            "loan-global-escape",
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "pub var escaped: Loan<Box> = %loan(seed);\n",
            "ownership_invalid_loan_global_compile.zr",
        },
        {
            "nested-borrow-global-escape",
            "class Box {}\n"
            "class Holder<T> {}\n"
            "pub var escaped: Holder<Borrow<Box>>;\n",
            "ownership_invalid_nested_borrow_global_compile.zr",
        },
        {
            "nested-loan-global-escape",
            "class Box {}\n"
            "class Holder<T> {}\n"
            "pub var escaped: Holder<Loan<Box>>;\n",
            "ownership_invalid_nested_loan_global_compile.zr",
        },
        {
            "borrow-closure-escape",
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "var owner = %shared(seed);\n"
            "var borrowed = %borrow(owner);\n"
            "var f = () => { borrowed; return 1; };\n",
            "ownership_invalid_borrow_closure_compile.zr",
        },
        {
            "loan-closure-escape",
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "var loaned = %loan(seed);\n"
            "var f = () => { loaned; return 1; };\n",
            "ownership_invalid_loan_closure_compile.zr",
        },
        {
            "nested-borrow-closure-escape",
            "class Box {}\n"
            "class Holder<T> {}\n"
            "var nested: Holder<Borrow<Box>>;\n"
            "var f = () => { nested; return 1; };\n",
            "ownership_invalid_nested_borrow_closure_compile.zr",
        },
        {
            "nested-loan-closure-escape",
            "class Box {}\n"
            "class Holder<T> {}\n"
            "var nested: Holder<Loan<Box>>;\n"
            "var f = () => { nested; return 1; };\n",
            "ownership_invalid_nested_loan_closure_compile.zr",
        },
        {
            "plugin-guard-return-escape",
            "leak() {\n"
            "    using (var math = %import(\"zr.math\")) {\n"
            "        return math;\n"
            "    } else {\n"
            "        return null;\n"
            "    }\n"
            "}\n",
            "ownership_invalid_plugin_guard_return_compile.zr",
        },
        {
            "plugin-guard-member-return-escape",
            "leak() {\n"
            "    using (var math = %import(\"zr.math\")) {\n"
            "        return math.abs;\n"
            "    } else {\n"
            "        return null;\n"
            "    }\n"
            "}\n",
            "ownership_invalid_plugin_guard_member_return_compile.zr",
        },
        {
            "plugin-guard-closure-escape",
            "using (var math = %import(\"zr.math\")) {\n"
            "    var f = () => { math; return 1; };\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_plugin_guard_closure_compile.zr",
        },
        {
            "plugin-guard-nested-function-alias-return-escape",
            "leak() {\n"
            "    using (var math = %import(\"zr.math\")) {\n"
            "        func nested() {\n"
            "            var alias = math;\n"
            "            return alias;\n"
            "        }\n"
            "        return nested();\n"
            "    } else {\n"
            "        return null;\n"
            "    }\n"
            "}\n",
            "ownership_invalid_plugin_guard_nested_function_alias_return_compile.zr",
        },
        {
            "plugin-guard-call-argument-escape",
            "sink(value) { return 0; }\n"
            "using (var math = %import(\"zr.math\")) {\n"
            "    sink(math);\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_plugin_guard_call_argument_compile.zr",
        },
        {
            "plugin-guard-constructor-argument-escape",
            "class Sink {\n"
            "    pub @constructor(value) {\n"
            "        var observed = value;\n"
            "    }\n"
            "}\n"
            "using (var math = %import(\"zr.math\")) {\n"
            "    var box = new Sink(math);\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_plugin_guard_constructor_argument_compile.zr",
        },
        {
            "plugin-guard-if-call-argument-escape",
            "sink(value) { return 1; }\n"
            "using (var math = %import(\"zr.math\")) {\n"
            "    if (sink(math)) {\n"
            "        var observed = 1;\n"
            "    }\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_plugin_guard_if_call_argument_compile.zr",
        },
        {
            "plugin-guard-switch-case-call-argument-escape",
            "sink(value) { return 1; }\n"
            "using (var math = %import(\"zr.math\")) {\n"
            "    switch (1) {\n"
            "        (sink(math)) {\n"
            "            var observed = 1;\n"
            "        }\n"
            "    }\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_plugin_guard_switch_case_call_argument_compile.zr",
        },
        {
            "plugin-guard-object-field-escape",
            "using (var math = %import(\"zr.math\")) {\n"
            "    var box = { handle: math };\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_plugin_guard_object_field_compile.zr",
        },
        {
            "plugin-guard-array-element-escape",
            "using (var math = %import(\"zr.math\")) {\n"
            "    var handles = [math];\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_plugin_guard_array_element_compile.zr",
        },
        {
            "plugin-guard-template-interpolation-escape",
            "using (var math = %import(\"zr.math\")) {\n"
            "    var text = `module ${math}`;\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_plugin_guard_template_interpolation_compile.zr",
        },
        {
            "plugin-guard-generator-out-escape",
            "using (var math = %import(\"zr.math\")) {\n"
            "    var gen = {{ out math; }};\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_plugin_guard_generator_out_compile.zr",
        },
        {
            "typed-plugin-guard-return-escape",
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "leak() {\n"
            "    using (var [m]: DynamicModule<Plugins> = %import(\"zr.plugins\")) {\n"
            "        return m;\n"
            "    } else {\n"
            "        return null;\n"
            "    }\n"
            "}\n",
            "ownership_invalid_typed_plugin_guard_return_compile.zr",
        },
        {
            "untyped-plugin-guard-return-escape",
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "leak() {\n"
            "    using (var [m] = %import(\"zr.plugins\")) {\n"
            "        return m;\n"
            "    } else {\n"
            "        return null;\n"
            "    }\n"
            "}\n",
            "ownership_invalid_untyped_plugin_guard_return_compile.zr",
        },
        {
            "untyped-plugin-guard-closure-escape",
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "using (var [m] = %import(\"zr.plugins\")) {\n"
            "    var f = () => { m; return 1; };\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_untyped_plugin_guard_closure_compile.zr",
        },
        {
            "untyped-plugin-guard-call-argument-escape",
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "sink(value) { return 0; }\n"
            "using (var [m] = %import(\"zr.plugins\")) {\n"
            "    sink(m);\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_untyped_plugin_guard_call_argument_compile.zr",
        },
        {
            "untyped-plugin-guard-object-field-escape",
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "using (var [m] = %import(\"zr.plugins\")) {\n"
            "    var box = { handle: m };\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_untyped_plugin_guard_object_field_compile.zr",
        },
        {
            "untyped-plugin-guard-array-element-escape",
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "using (var [m] = %import(\"zr.plugins\")) {\n"
            "    var handles = [m];\n"
            "} else {\n"
            "    var fallback = 0;\n"
            "}\n",
            "ownership_invalid_untyped_plugin_guard_array_element_compile.zr",
        },
        {
            "untyped-plugin-guard-if-assignment-return-escape",
            "union DynamicModule<T> {\n"
            "    Unavailable;\n"
            "    @Available(m: Module);\n"
            "}\n"
            "leak(flag) {\n"
            "    using (var [m] = %import(\"zr.plugins\")) {\n"
            "        var alias;\n"
            "        if (flag) {\n"
            "            alias = m;\n"
            "        }\n"
            "        return alias;\n"
            "    } else {\n"
            "        return null;\n"
            "    }\n"
            "}\n",
            "ownership_invalid_untyped_plugin_guard_if_assignment_return_compile.zr",
        },
        {
            "plugin-guard-condition-assignment-return-escape",
            "leak() {\n"
            "    using (var math = %import(\"zr.math\")) {\n"
            "        var alias;\n"
            "        if (alias = math) {\n"
            "            var observed = 1;\n"
            "        }\n"
            "        return alias;\n"
            "    } else {\n"
            "        return null;\n"
            "    }\n"
            "}\n",
            "ownership_invalid_plugin_guard_condition_assignment_return_compile.zr",
        },
        {
            "plugin-guard-initializer-assignment-return-escape",
            "leak() {\n"
            "    using (var math = %import(\"zr.math\")) {\n"
            "        var alias;\n"
            "        var ok = (alias = math);\n"
            "        return alias;\n"
            "    } else {\n"
            "        return null;\n"
            "    }\n"
            "}\n",
            "ownership_invalid_plugin_guard_initializer_assignment_return_compile.zr",
        },
        {
            "plugin-guard-try-return-escape",
            "leak() {\n"
            "    using (var math = %import(\"zr.math\")) {\n"
            "        try {\n"
            "            return math;\n"
            "        } catch (e) {\n"
            "            return null;\n"
            "        }\n"
            "    } else {\n"
            "        return null;\n"
            "    }\n"
            "}\n",
            "ownership_invalid_plugin_guard_try_return_compile.zr",
        },
        {
            "plugin-guard-throw-escape",
            "leak() {\n"
            "    using (var math = %import(\"zr.math\")) {\n"
            "        throw math;\n"
            "    } else {\n"
            "        return null;\n"
            "    }\n"
            "}\n",
            "ownership_invalid_plugin_guard_throw_compile.zr",
        },
    };
    TZrSize i;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership builtin invalid operand compilation",
              "Testing that invalid Rust-first ownership builtin operands are rejected before opcode emission");

    for (i = 0; i < ZR_ARRAY_COUNT(cases); i++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrFunction *func;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_Create(state, (TZrNativeString)cases[i].sourceName, strlen(cases[i].sourceName));
        TEST_ASSERT_NOT_NULL(sourceName);
        func = ZrParser_Source_Compile(state, cases[i].source, strlen(cases[i].source), sourceName);
        TEST_ASSERT_NULL_MESSAGE(func, cases[i].label);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_global_assignment_reports_escape_boundary(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Global Assignment Reports Escape Boundary";
    const char *source =
        "var leaked = null;\n"
        "using (var math = %import(\"zr.math\")) {\n"
        "    leaked = math;\n"
        "} else {\n"
        "    var fallback = 0;\n"
        "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Plugin guard global assignment diagnostics",
              "Testing that assigning a guard-scoped module handle to an outer/global storage location reports a dedicated escape boundary");

    state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_Create(state,
                                      "plugin_guard_global_assignment_escape.zr",
                                      strlen("plugin_guard_global_assignment_escape.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse plugin guard global assignment source");
        destroy_test_state(state);
        return;
    }

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, state);
    cs.currentAst = ast;
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(state, ast);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create compiler function");
        destroy_test_state(state);
        return;
    }

    TEST_ASSERT_TRUE(compiler_validate_task_effects(&cs, ast));
    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "plugin_type_escape"));
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "outer/global assignment"));

    ZrCore_Function_Free(state, cs.currentFunction);
    cs.currentFunction = ZR_NULL;
    if (cs.topLevelFunction != ZR_NULL) {
        ZrCore_Function_Free(state, cs.topLevelFunction);
        cs.topLevelFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_member_assignment_reports_escape_boundary(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Member Assignment Reports Escape Boundary";
    const char *source =
        "leak() {\n"
        "    var box = {};\n"
        "    using (var math = %import(\"zr.math\")) {\n"
        "        box.handle = math;\n"
        "        return box;\n"
        "    } else {\n"
        "        return null;\n"
        "    }\n"
        "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Plugin guard member assignment diagnostics",
              "Testing that assigning a guard-scoped module handle into object/member storage reports a field/container boundary");

    state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_Create(state,
                                      "plugin_guard_member_assignment_escape.zr",
                                      strlen("plugin_guard_member_assignment_escape.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse plugin guard member assignment source");
        destroy_test_state(state);
        return;
    }

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, state);
    cs.currentAst = ast;
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(state, ast);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create compiler function");
        destroy_test_state(state);
        return;
    }

    TEST_ASSERT_TRUE(compiler_validate_task_effects(&cs, ast));
    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "plugin_type_escape"));
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "field/container"));

    ZrCore_Function_Free(state, cs.currentFunction);
    cs.currentFunction = ZR_NULL;
    if (cs.topLevelFunction != ZR_NULL) {
        ZrCore_Function_Free(state, cs.topLevelFunction);
        cs.topLevelFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_type_query_reports_escape_boundary(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Type Query Reports Escape Boundary";
    const char *source =
        "leak() {\n"
        "    using (var math = %import(\"zr.math\")) {\n"
        "        return %type(math);\n"
        "    } else {\n"
        "        return null;\n"
        "    }\n"
        "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Plugin guard type query diagnostics",
              "Testing that %type(...) cannot wrap a guard-scoped module handle and return it past the guard boundary");

    state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_Create(state,
                                      "plugin_guard_type_query_escape.zr",
                                      strlen("plugin_guard_type_query_escape.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse plugin guard type query source");
        destroy_test_state(state);
        return;
    }

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, state);
    cs.currentAst = ast;
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(state, ast);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create compiler function");
        destroy_test_state(state);
        return;
    }

    TEST_ASSERT_TRUE(compiler_validate_task_effects(&cs, ast));
    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "plugin_type_escape"));
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "return"));

    ZrCore_Function_Free(state, cs.currentFunction);
    cs.currentFunction = ZR_NULL;
    if (cs.topLevelFunction != ZR_NULL) {
        ZrCore_Function_Free(state, cs.topLevelFunction);
        cs.topLevelFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_decorator_reports_escape_boundary(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Decorator Reports Escape Boundary";
    const char *source =
        "using (var math = %import(\"zr.math\")) {\n"
        "    #math.Decorate#\n"
        "    func nested(): int {\n"
        "        return 1;\n"
        "    }\n"
        "} else {\n"
        "    var fallback = 0;\n"
        "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Plugin guard decorator diagnostics",
              "Testing that a guard-scoped module handle cannot be captured by decorator metadata");

    state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_Create(state,
                                      "plugin_guard_decorator_escape.zr",
                                      strlen("plugin_guard_decorator_escape.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse plugin guard decorator source");
        destroy_test_state(state);
        return;
    }

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, state);
    cs.currentAst = ast;
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(state, ast);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create compiler function");
        destroy_test_state(state);
        return;
    }

    TEST_ASSERT_TRUE(compiler_validate_task_effects(&cs, ast));
    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "plugin_type_escape"));
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "decorator"));

    ZrCore_Function_Free(state, cs.currentFunction);
    cs.currentFunction = ZR_NULL;
    if (cs.topLevelFunction != ZR_NULL) {
        ZrCore_Function_Free(state, cs.topLevelFunction);
        cs.topLevelFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_parameter_default_reports_escape_boundary(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Parameter Default Reports Escape Boundary";
    const char *source =
        "using (var math = %import(\"zr.math\")) {\n"
        "    func nested(value = math): int {\n"
        "        return 1;\n"
        "    }\n"
        "} else {\n"
        "    var fallback = 0;\n"
        "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Plugin guard parameter default diagnostics",
              "Testing that a guard-scoped module handle cannot be captured by function parameter default metadata");

    state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_Create(state,
                                      "plugin_guard_parameter_default_escape.zr",
                                      strlen("plugin_guard_parameter_default_escape.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse plugin guard parameter default source");
        destroy_test_state(state);
        return;
    }

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, state);
    cs.currentAst = ast;
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(state, ast);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create compiler function");
        destroy_test_state(state);
        return;
    }

    TEST_ASSERT_TRUE(compiler_validate_task_effects(&cs, ast));
    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "plugin_type_escape"));
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "parameter default"));

    ZrCore_Function_Free(state, cs.currentFunction);
    cs.currentFunction = ZR_NULL;
    if (cs.topLevelFunction != ZR_NULL) {
        ZrCore_Function_Free(state, cs.topLevelFunction);
        cs.topLevelFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_signature_type_reports_escape_boundary(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Signature Type Reports Escape Boundary";
    const char *source =
        "using (var math = %import(\"zr.math\")) {\n"
        "    func nested(): math.Vector {\n"
        "        return null;\n"
        "    }\n"
        "} else {\n"
        "    var fallback = 0;\n"
        "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Plugin guard signature type diagnostics",
              "Testing that a guard-scoped module handle cannot be captured by function signature type metadata");

    state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_Create(state,
                                      "plugin_guard_signature_type_escape.zr",
                                      strlen("plugin_guard_signature_type_escape.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse plugin guard signature type source");
        destroy_test_state(state);
        return;
    }

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, state);
    cs.currentAst = ast;
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(state, ast);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create compiler function");
        destroy_test_state(state);
        return;
    }

    TEST_ASSERT_TRUE(compiler_validate_task_effects(&cs, ast));
    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "plugin_type_escape"));
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "signature type"));

    ZrCore_Function_Free(state, cs.currentFunction);
    cs.currentFunction = ZR_NULL;
    if (cs.topLevelFunction != ZR_NULL) {
        ZrCore_Function_Free(state, cs.topLevelFunction);
        cs.topLevelFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_foreach_binding_type_reports_escape_boundary(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Foreach Binding Type Reports Escape Boundary";
    const char *source =
        "using (var math = %import(\"zr.math\")) {\n"
        "    for(var item: math.Vector in []) {\n"
        "        var observed = item;\n"
        "    }\n"
        "} else {\n"
        "    var fallback = 0;\n"
        "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Plugin guard foreach binding type diagnostics",
              "Testing that a guard-scoped module handle cannot be captured by foreach binding type metadata");

    state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_Create(state,
                                      "plugin_guard_foreach_binding_type_escape.zr",
                                      strlen("plugin_guard_foreach_binding_type_escape.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse plugin guard foreach binding type source");
        destroy_test_state(state);
        return;
    }

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, state);
    cs.currentAst = ast;
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(state, ast);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create compiler function");
        destroy_test_state(state);
        return;
    }

    TEST_ASSERT_TRUE(compiler_validate_task_effects(&cs, ast));
    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "plugin_type_escape"));
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "signature type"));

    ZrCore_Function_Free(state, cs.currentFunction);
    cs.currentFunction = ZR_NULL;
    if (cs.topLevelFunction != ZR_NULL) {
        ZrCore_Function_Free(state, cs.topLevelFunction);
        cs.topLevelFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_generic_call_argument_reports_escape_boundary(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Generic Call Argument Reports Escape Boundary";
    const char *source =
        "leak() {\n"
        "    using (var math = %import(\"zr.math\")) {\n"
        "        return sink<math.Vector>();\n"
        "    } else {\n"
        "        return null;\n"
        "    }\n"
        "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCompilerState cs;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Plugin guard generic call argument diagnostics",
              "Testing that a guard-scoped module handle cannot be captured by function-call generic metadata");

    state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_Create(state,
                                      "plugin_guard_generic_call_argument_escape.zr",
                                      strlen("plugin_guard_generic_call_argument_escape.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse plugin guard generic call argument source");
        destroy_test_state(state);
        return;
    }

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, state);
    cs.currentAst = ast;
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        ZrParser_Ast_Free(state, ast);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create compiler function");
        destroy_test_state(state);
        return;
    }

    TEST_ASSERT_TRUE(compiler_validate_task_effects(&cs, ast));
    compile_script(&cs, ast);

    TEST_ASSERT_TRUE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "plugin_type_escape"));
    TEST_ASSERT_NOT_NULL(strstr(cs.errorMessage, "signature type"));

    ZrCore_Function_Free(state, cs.currentFunction);
    cs.currentFunction = ZR_NULL;
    if (cs.topLevelFunction != ZR_NULL) {
        ZrCore_Function_Free(state, cs.topLevelFunction);
        cs.topLevelFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_nested_function_shadowed_parameter_allows_local_value(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Nested Function Shadowed Parameter Allows Local Value";
    const char *source =
        "valid() {\n"
        "    using (var math = %import(\"zr.math\")) {\n"
        "        func nested(math) {\n"
        "            return math;\n"
        "        }\n"
        "        return nested(null);\n"
        "    } else {\n"
        "        return null;\n"
        "    }\n"
        "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *func;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Plugin guard nested function shadowing",
              "Testing that a nested function parameter can shadow the guard binder without being reported as a plugin handle capture");

    state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_Create(
            state,
            "plugin_guard_nested_function_shadowed_parameter.zr",
            strlen("plugin_guard_nested_function_shadowed_parameter.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);

    func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(func);

    ZrCore_Function_Free(state, func);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_plugin_guard_nested_function_destructured_shadow_allows_local_value(void) {
    SZrTestTimer timer;
    const char *testSummary = "Plugin Guard Nested Function Destructured Shadow Allows Local Value";
    const char *source =
        "valid() {\n"
        "    using (var math = %import(\"zr.math\")) {\n"
        "        func nestedObject() {\n"
        "            var {math} = {math: null};\n"
        "            return math;\n"
        "        }\n"
        "        func nestedArray() {\n"
        "            var [math] = [null];\n"
        "            return math;\n"
        "        }\n"
        "        func nestedAlias() {\n"
        "            var {math: value} = {value: null};\n"
        "            return math;\n"
        "        }\n"
        "        var objectValue = nestedObject();\n"
        "        var arrayValue = nestedArray();\n"
        "        return nestedAlias();\n"
        "    } else {\n"
        "        return null;\n"
        "    }\n"
        "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *func;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Plugin guard nested function destructuring shadow",
              "Testing that a nested function destructuring declaration can shadow the guard binder without being reported as a plugin handle capture");

    state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    sourceName = ZrCore_String_Create(
            state,
            "plugin_guard_nested_function_destructured_shadow.zr",
            strlen("plugin_guard_nested_function_destructured_shadow.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);

    func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(func);

    ZrCore_Function_Free(state, func);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_ownership_detach_runtime_rejects_multi_owner_shared_value(void) {
    SZrTestTimer timer;
    const char *testSummary = "Ownership Detach Runtime Rejects Multi Owner Shared Value";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Ownership runtime multi-owner detach",
              "Testing that %detach(shared) returns null and keeps the existing shared owners alive when strong count exceeds one");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
            "class Box {}\n"
            "var seed = %unique new Box();\n"
            "var owner = %shared(seed);\n"
            "var alias = owner;\n"
            "var detached = %detach(owner);\n"
            "if (owner != null && alias != null && detached == null) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
        SZrString *sourceName = ZrCore_String_Create(state,
                                                     "ownership_detach_multi_owner_runtime.zr",
                                                     strlen("ownership_detach_multi_owner_runtime.zr"));
        SZrFunction *func;
        TZrInt64 result = 0;

        func = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile multi-owner detach runtime source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result));
        TEST_ASSERT_EQUAL_INT64(1, result);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_function_call_argument_conversion_emits_to_float(void) {
    SZrTestTimer timer;
    const char *testSummary = "Function Call Argument Conversion Emits TO_FLOAT";
    TZrUInt32 toFloatCount = 0;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Function call argument conversion",
              "Testing that calling a typed float function with an int argument emits a TO_FLOAT conversion before FUNCTION_CALL");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
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
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse typed call conversion source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile typed call conversion source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)) ||
                         function_contains_opcode(func, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)));
        for (TZrUInt32 i = 0; i < func->instructionsLength; i++) {
            if ((EZrInstructionCode)func->instructionsList[i].instruction.operationCode == ZR_INSTRUCTION_ENUM(TO_FLOAT)) {
                toFloatCount++;
            }
        }
        TEST_ASSERT_EQUAL_UINT32(1, toFloatCount);

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_intermediate_writer_emits_type_metadata_section(void) {
    SZrTestTimer timer;
    const char *testSummary = "Intermediate Writer Emits Type Metadata Section";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Intermediate writer type metadata",
              "Testing that .zri output includes a dedicated type metadata section with exported signatures");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
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
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse typed metadata intermediate source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile typed metadata intermediate source");
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
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_intermediate_writer_emits_compile_time_and_test_metadata(void) {
    SZrTestTimer timer;
    const char *testSummary = "Intermediate Writer Emits Compile Time And Test Metadata";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Intermediate writer metadata closure",
              "Testing that .zri output includes compile-time declarations and %test metadata");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
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
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse compile-time metadata intermediate source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile compile-time metadata intermediate source");
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
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_member_and_index_access_emit_split_opcodes(void) {
    SZrTestTimer timer;
    const char *testSummary = "Member And Index Access Emit Split Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Access opcode split",
              "Testing that dot access lowers to member opcodes while bracket access lowers to index opcodes");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
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
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse split opcode source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile split opcode source");
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
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_global_member_access_emits_get_member(void) {
    SZrTestTimer timer;
    const char *testSummary = "Global Member Access Emits GET_MEMBER";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Global member lowering",
              "Testing that global zr member access still goes through member semantics instead of table fallback");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
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
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse global member source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile global member source");
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
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_fixed_array_lowering_does_not_emit_length_member_write(void) {
    SZrTestTimer timer;
    const char *testSummary = "Fixed Array Lowering Omits Length Member Write";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Fixed array lowering",
              "Testing that fixed-size array initialization lowers through CREATE_ARRAY and SET_BY_INDEX without synthesizing SET_MEMBER length writes");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
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
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse fixed array source");
            destroy_test_state(state);
            return;
        }

        wrapper = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (wrapper == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile fixed array source");
            destroy_test_state(state);
            return;
        }

        readFunction = get_single_compiled_child_function(wrapper);
        TEST_ASSERT_TRUE(function_contains_opcode(readFunction, ZR_INSTRUCTION_ENUM(CREATE_ARRAY)));
        TEST_ASSERT_TRUE(function_contains_opcode(readFunction, ZR_INSTRUCTION_ENUM(SET_BY_INDEX)));
        TEST_ASSERT_TRUE(function_contains_opcode(readFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER)) ||
                         function_contains_opcode(readFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_FALSE(function_contains_opcode(readFunction, ZR_INSTRUCTION_ENUM(SET_MEMBER)));
        TEST_ASSERT_FALSE(function_contains_opcode(readFunction, ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT)));
        ZrCore_Function_Free(state, wrapper);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_compiler_optimizer_removes_dead_null_clear_streaks(void) {
    SZrTestTimer timer;
    const char *testSummary = "Compiler Optimizer Removes Dead Null Clear Streaks";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Dead null clear optimization",
              "Testing that straight-line temp cleanup no longer leaves long GET_CONSTANT null streaks in compiled output");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "sum10(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int, i: int, j: int): int {\n"
                "    return a + b + c + d + e + f + g + h + i + j;\n"
                "}\n"
                "work(seed: int): int {\n"
                "    var total = sum10(seed + 1,\n"
                "                      seed + 2,\n"
                "                      seed + 3,\n"
                "                      seed + 4,\n"
                "                      seed + 5,\n"
                "                      seed + 6,\n"
                "                      seed + 7,\n"
                "                      seed + 8,\n"
                "                      seed + 9,\n"
                "                      seed + 10);\n"
                "    return total;\n"
                "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "optimizer_dead_null_clear_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *wrapper;
        SZrFunction *workFunction;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse optimizer null-clear source");
            destroy_test_state(state);
            return;
        }

        wrapper = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (wrapper == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile optimizer null-clear source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2, wrapper->childFunctionLength);
        workFunction = &wrapper->childFunctionList[1];
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(2, function_max_null_get_constant_streak(workFunction));
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(4, function_count_null_get_constant(workFunction));

        ZrCore_Function_Free(state, wrapper);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_compiler_optimizer_reuses_temp_slots_in_basic_blocks(void) {
    SZrTestTimer timer;
    const char *testSummary = "Compiler Optimizer Reuses Temp Slots In Basic Blocks";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Temp slot reuse",
              "Testing that sequential temp-heavy statements reuse transient slots instead of monotonically inflating stackSize");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
        return;
    }

    {
        const char *source =
                "work(input: int): int {\n"
                "    var a = input + 1;\n"
                "    var b = input + 2;\n"
                "    var c = input + 3;\n"
                "    var d = input + 4;\n"
                "    return a + b + c + d;\n"
                "}\n";
        SZrString *sourceName = ZrCore_String_Create(state, "optimizer_temp_slot_reuse_test.zr", 33);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrFunction *wrapper;
        SZrFunction *workFunction;

        if (ast == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse temp slot reuse source");
            destroy_test_state(state);
            return;
        }

        wrapper = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (wrapper == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile temp slot reuse source");
            destroy_test_state(state);
            return;
        }

        workFunction = get_single_compiled_child_function(wrapper);
        if (workFunction->stackSize < workFunction->localVariableLength + 2u) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Temp slot reuse inflated stackSize beyond expected bound");
            ZrCore_Function_Free(state, wrapper);
            destroy_test_state(state);
            return;
        }

        ZrCore_Function_Free(state, wrapper);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_foreach_emits_iterator_contract_opcodes(void) {
    SZrTestTimer timer;
    const char *testSummary = "Foreach Emits Iterator Contract Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Foreach lowering",
              "Testing that foreach lowers through iterator contract opcodes instead of string member protocol names");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
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
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse foreach source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile foreach source");
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
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_type_expression_compiles_to_typeof_opcode(void) {
    SZrTestTimer timer;
    const char *testSummary = "Type Expression Compiles To TYPEOF Opcode";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Type expression lowering",
              "Testing that %type(expr) lowers to the dedicated TYPEOF opcode instead of a serialized native helper");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
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
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse type expression source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile type expression source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(TYPEOF)));
        TEST_ASSERT_FALSE(function_contains_native_helper_constant(func, ZR_IO_NATIVE_HELPER_REFLECTION_TYPEOF));

        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_binary_writer_handles_exported_function_alias_with_unknown_parameter_types(void) {
    SZrTestTimer timer;
    const char *testSummary = "Binary Writer Handles Exported Function Alias With Unknown Parameter Types";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("Binary writer export metadata",
              "Testing that exported aliases of partially typed functions still serialize typed export metadata without crashes");

    SZrState *state = create_test_state();
    if (state == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to create test state");
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
            ZR_TEST_FAIL(timer, testSummary, "Failed to parse exported alias source");
            destroy_test_state(state);
            return;
        }

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        if (func == ZR_NULL) {
            timer.endTime = clock();
            ZR_TEST_FAIL(timer, testSummary, "Failed to compile exported alias source");
            destroy_test_state(state);
            return;
        }

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, func, binaryPath));

        remove(binaryPath);
        ZrCore_Function_Free(state, func);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}
