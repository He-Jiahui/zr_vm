#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"
#include "compiler/compiler_internal.h"

// 已知调用矩阵的局部计时状态；时间值不参与 opcode 正确性判断。
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrKnownCallPipelineTimer;

// 手工构造不同函数来源，检验 quickening 能否恢复 VM/native 调用目标。
typedef enum {
    ZR_KNOWN_CALL_PROVENANCE_GET_SUB_FUNCTION = 0,
    ZR_KNOWN_CALL_PROVENANCE_CREATE_CLOSURE = 1,
    ZR_KNOWN_CALL_PROVENANCE_FUNCTION_CONSTANT = 2,
    ZR_KNOWN_CALL_PROVENANCE_CLOSURE_CONSTANT = 3,
    ZR_KNOWN_CALL_PROVENANCE_NATIVE_FUNCTION_CONSTANT = 4,
    ZR_KNOWN_CALL_PROVENANCE_NATIVE_CLOSURE_CONSTANT = 5,
    ZR_KNOWN_CALL_PROVENANCE_NATIVE_POINTER_CONSTANT = 6
} EZrKnownCallQuickeningProvenanceKind;

// 模拟值经栈槽复制传播后的别名路径，约束来源追踪用例。
typedef enum {
    ZR_KNOWN_CALL_ALIAS_NONE = 0,
    ZR_KNOWN_CALL_ALIAS_GET_STACK = 1,
    ZR_KNOWN_CALL_ALIAS_SET_STACK = 2
} EZrKnownCallAliasKind;

// 一行编译期用例同时绑定来源、别名、尾调用和预期 opcode。
typedef struct {
    const char *name;
    EZrKnownCallQuickeningProvenanceKind provenance;
    EZrKnownCallAliasKind aliasKind;
    TZrBool isTailCall;
    TZrUInt16 argumentCount;
    EZrInstructionCode expectedOpcode;
} SZrKnownCallQuickeningCase;

// 一行运行期用例绑定手工 opcode 的目标类型与预期返回值。
typedef struct {
    const char *name;
    EZrInstructionCode opcode;
    TZrBool isNative;
    TZrBool isTailCall;
    TZrBool zeroArg;
    TZrInt64 expectedResult;
} SZrKnownCallRuntimeCase;

// 一行后端用例把 opcode 名称与目标类型绑定，供 C/LLVM 双后端生成检查。
typedef struct {
    const char *name;
    const char *opcodeName;
    EZrInstructionCode opcode;
    TZrBool isNative;
    TZrBool isTailCall;
    TZrBool zeroArg;
} SZrKnownCallAotCase;

// 作为已知 native 目标的稳定返回值，用来区分目标解析正确与误绑定。
static TZrInt64 known_call_native_return_73(struct SZrState *state) {
    SZrCallInfo *callInfo;
    SZrTypeValue *resultValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL) {
        return 0;
    }

    callInfo = state->callInfoList;
    resultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (resultValue == ZR_NULL) {
        return 0;
    }

    ZrCore_Value_InitAsInt(state, resultValue, 73);
    state->stackTop.valuePointer = callInfo->functionBase.valuePointer + 1;
    return 1;
}

// 提供第二个 native 返回值，令来源追踪用例能发现目标混淆。
static TZrInt64 known_call_native_return_91(struct SZrState *state) {
    SZrCallInfo *callInfo;
    SZrTypeValue *resultValue;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL) {
        return 0;
    }

    callInfo = state->callInfoList;
    resultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (resultValue == ZR_NULL) {
        return 0;
    }

    ZrCore_Value_InitAsInt(state, resultValue, 91);
    state->stackTop.valuePointer = callInfo->functionBase.valuePointer + 1;
    return 1;
}

// 读取 C/LLVM 工件供 opcode 支持性断言；返回缓冲由各测试释放。
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

// 识别 C 后端对某 opcode 生成的 unsupported 标记，供矩阵用例逐项拒绝回退。TODO: 文本扫描不能证明可执行语义。
static TZrBool aot_c_text_contains_unsupported_opcode(const char *text, TZrUInt32 opcode) {
    const char *unsupportedPrefix = "ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state,";
    const char *cursor = text;
    char opcodeSuffix[32];

    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    snprintf(opcodeSuffix, sizeof(opcodeSuffix), ", %u);", (unsigned)opcode);
    while ((cursor = strstr(cursor, unsupportedPrefix)) != ZR_NULL) {
        const char *lineEnd = strchr(cursor, '\n');
        const char *suffix = strstr(cursor, opcodeSuffix);
        if (suffix != ZR_NULL && (lineEnd == ZR_NULL || suffix < lineEnd)) {
            return ZR_TRUE;
        }
        cursor += strlen(unsupportedPrefix);
    }

    return ZR_FALSE;
}

// 识别 LLVM 后端 unsupported 标记，与 C 后端同一矩阵逐项比对。TODO: 文本扫描不能证明可执行语义。
static TZrBool aot_llvm_text_contains_unsupported_opcode(const char *text, TZrUInt32 opcode) {
    const char *unsupportedPrefix = "call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state";
    const char *cursor = text;
    char opcodeSuffix[32];

    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    snprintf(opcodeSuffix, sizeof(opcodeSuffix), ", i32 %u)", (unsigned)opcode);
    while ((cursor = strstr(cursor, unsupportedPrefix)) != ZR_NULL) {
        const char *lineEnd = strchr(cursor, '\n');
        const char *suffix = strstr(cursor, opcodeSuffix);
        if (suffix != ZR_NULL && (lineEnd == ZR_NULL || suffix < lineEnd)) {
            return ZR_TRUE;
        }
        cursor += strlen(unsupportedPrefix);
    }

    return ZR_FALSE;
}

// 以严格且独立的 AOT C 模式生成用例工件，禁止借助嵌入式执行替代 lowering。
static TZrBool write_standalone_strict_aot_c_file(SZrState *state,
                                                  SZrFunction *function,
                                                  const TZrChar *moduleName,
                                                  const TZrChar *filename) {
    SZrAotWriterOptions options;

    if (state == ZR_NULL || function == ZR_NULL || moduleName == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = moduleName;
    options.requireExecutableLowering = ZR_TRUE;
    return ZrParser_Writer_WriteAotCFileWithOptions(state, function, filename, &options);
}

// 以严格且独立的 LLVM 模式生成工件，与 C 后端保持同一输入边界。
static TZrBool write_standalone_strict_aot_llvm_file(SZrState *state,
                                                     SZrFunction *function,
                                                     const TZrChar *moduleName,
                                                     const TZrChar *filename) {
    SZrAotWriterOptions options;

    if (state == ZR_NULL || function == ZR_NULL || moduleName == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = moduleName;
    options.requireExecutableLowering = ZR_TRUE;
    return ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, filename, &options);
}

// 为手工 opcode 矩阵准备无槽操作数指令，绕过源编译器的自动选择。
static TZrInstruction make_instruction_no_operands(EZrInstructionCode opcode, TZrUInt16 operandExtra) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    return instruction;
}

// 为常量目标调用准备指令，隔离目标来源与常量索引编码。
static TZrInstruction make_instruction_constant_operand(EZrInstructionCode opcode,
                                                        TZrUInt16 operandExtra,
                                                        TZrInt32 operandA2) {
    TZrInstruction instruction = make_instruction_no_operands(opcode, operandExtra);

    instruction.instruction.operand.operand2[0] = operandA2;
    return instruction;
}

// 为调用操作数准备槽位指令，以覆盖别名传播和参数位置。
static TZrInstruction make_instruction_slot_operands(EZrInstructionCode opcode,
                                                     TZrUInt16 operandExtra,
                                                     TZrUInt16 operandA1,
                                                     TZrUInt16 operandB1) {
    TZrInstruction instruction = make_instruction_no_operands(opcode, operandExtra);

    instruction.instruction.operand.operand1[0] = operandA1;
    instruction.instruction.operand.operand1[1] = operandB1;
    return instruction;
}

// 为手工函数提供终止路径，确保运行期矩阵可观察结果。
static TZrInstruction make_instruction_return(TZrUInt16 sourceSlot) {
    return make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, sourceSlot, 0);
}

// 建立最小手工函数骨架，供 quickening、运行时和后端矩阵共用。
static void init_manual_test_function(SZrFunction *function,
                                      TZrInstruction *instructions,
                                      TZrUInt32 instructionCount,
                                      SZrTypeValue *constants,
                                      TZrUInt32 constantCount,
                                      SZrFunction *childFunctions,
                                      TZrUInt32 childFunctionCount,
                                      TZrUInt32 stackSize) {
    TEST_ASSERT_NOT_NULL(function);

    memset(function, 0, sizeof(*function));
    function->instructionsList = instructions;
    function->instructionsLength = instructionCount;
    function->constantValueList = constants;
    function->constantValueLength = constantCount;
    function->childFunctionList = childFunctions;
    function->childFunctionLength = childFunctionCount;
    function->stackSize = stackSize;
    function->lineInSourceStart = 1;
    function->lineInSourceEnd = 1;
}

// 在单一 ExecBC 函数中确认预期 opcode，用于检查 quickening 的最终选择。
static TZrBool function_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

// 递归扫描子函数，覆盖成员方法与闭包中的已知调用 opcode。
static TZrBool function_tree_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 childIndex;

    if (function_contains_opcode(function, opcode)) {
        return ZR_TRUE;
    }
    if (function == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (function_tree_contains_opcode(&function->childFunctionList[childIndex], opcode)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

// 从类型化成员调用源码生成真实编译器样例，与手工矩阵互相印证。
static SZrFunction *compile_typed_member_known_call_fixture(SZrState *state) {
    const char *source =
            "class Counter {\n"
            "    pub var value: int;\n"
            "    pub fn step(delta: int): int {\n"
            "        this.value = this.value + delta;\n"
            "        return this.value;\n"
            "    }\n"
            "    pub fn read(): int {\n"
            "        return this.value;\n"
            "    }\n"
            "}\n"
            "var counter = new Counter();\n"
            "counter.value = 3;\n"
            "var first = counter.step(4);\n"
            "var second = counter.read();\n"
            "return first + second;\n";
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZR_STRING_LITERAL(state, "typed_member_known_call_fixture.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

// 为运行时矩阵包装 GC 可追踪的函数引用；不建立显式 ownership control。
static void init_function_value(SZrState *state, SZrTypeValue *value, SZrFunction *function, TZrBool isNative) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    value->type = ZR_VALUE_TYPE_FUNCTION;
    value->isNative = isNative;
}

// 为手工矩阵借用已有函数对象；调用方必须保证函数活到断言完成。
static void init_borrowed_function_value(SZrTypeValue *value, SZrFunction *function, TZrBool isNative) {
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(function);

    memset(value, 0, sizeof(*value));
    value->type = ZR_VALUE_TYPE_FUNCTION;
    value->value.object = ZR_CAST_RAW_OBJECT_AS_SUPER(function);
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = isNative;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

// 构造带环境的函数目标，检验已知调用识别不丢失闭包来源。
static void init_closure_value(SZrState *state, SZrTypeValue *value, SZrClosure *closure, TZrBool isNative) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(closure);

    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    value->type = ZR_VALUE_TYPE_CLOSURE;
    value->isNative = isNative;
}

// 构造 native 闭包目标，区分它与普通 native 函数指针。
static void init_native_closure_value(SZrState *state,
                                      SZrTypeValue *value,
                                      EZrValueType valueType,
                                      FZrNativeFunction nativeFunction) {
    SZrClosureNative *nativeClosure;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(nativeFunction);

    nativeClosure = ZrCore_ClosureNative_New(state, 0);
    TEST_ASSERT_NOT_NULL(nativeClosure);
    nativeClosure->nativeFunction = nativeFunction;

    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));
    value->type = valueType;
    value->isNative = ZR_TRUE;
}

// 通过测试 state 分配手工指令数组，所有权与函数 fixture 生命周期一致。
static TZrInstruction *allocate_function_instructions(SZrState *state, TZrUInt32 count) {
    TZrInstruction *instructions;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(count > 0);
    instructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                     sizeof(*instructions) * count,
                                                                     ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(instructions);
    memset(instructions, 0, sizeof(*instructions) * count);
    return instructions;
}

// 为手工函数准备可追踪目标常量表；调用方需填满被索引项。
static SZrTypeValue *allocate_function_constants(SZrState *state, TZrUInt32 count) {
    SZrTypeValue *constants;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(count > 0);
    constants = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                sizeof(*constants) * count,
                                                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(constants);
    memset(constants, 0, sizeof(*constants) * count);
    return constants;
}

// 分配子函数表供闭包与成员来源矩阵使用，索引须与构造用例一致。
static SZrFunction *allocate_child_functions(SZrState *state, TZrUInt32 count) {
    SZrFunction *functions;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(count > 0);
    functions = (SZrFunction *)ZrCore_Memory_RawMallocWithType(state->global,
                                                               sizeof(*functions) * count,
                                                               ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(functions);
    memset(functions, 0, sizeof(*functions) * count);
    return functions;
}

// 构造可调用的叶子函数，让运行期矩阵通过返回值辨别错误目标。
static SZrFunction *build_heap_leaf_function_returning_constant(SZrState *state,
                                                                TZrUInt16 parameterCount,
                                                                TZrInt64 returnValue) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->parameterCount = parameterCount;
    function->stackSize = 1;
    function->instructionsLength = 2;
    function->instructionsList = allocate_function_instructions(state, 2);
    function->constantValueLength = 1;
    function->constantValueList = allocate_function_constants(state, 1);
    function->lineInSourceStart = 1;
    function->lineInSourceEnd = 1;

    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], returnValue);
    function->instructionsList[0] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    function->instructionsList[1] = make_instruction_return(0);
    return function;
}

// 按矩阵行拼装 VM/native 调用函数，覆盖零参、尾调用和非尾调用执行。
static SZrFunction *build_runtime_known_call_fixture(SZrState *state,
                                                     const SZrKnownCallRuntimeCase *testCase,
                                                     SZrFunction **ownedLeafFunction) {
    SZrFunction *rootFunction;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(testCase);
    TEST_ASSERT_NOT_NULL(ownedLeafFunction);

    *ownedLeafFunction = ZR_NULL;
    rootFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(rootFunction);

    rootFunction->parameterCount = 0;
    rootFunction->stackSize = testCase->zeroArg ? 2u : 3u;
    rootFunction->instructionsLength = testCase->zeroArg ? 3u : 4u;
    rootFunction->instructionsList = allocate_function_instructions(state, rootFunction->instructionsLength);
    rootFunction->constantValueLength = testCase->zeroArg ? 1u : 2u;
    rootFunction->constantValueList = allocate_function_constants(state, rootFunction->constantValueLength);
    rootFunction->lineInSourceStart = 1;
    rootFunction->lineInSourceEnd = 1;

    if (testCase->isNative) {
        init_native_closure_value(state,
                                  &rootFunction->constantValueList[0],
                                  ZR_VALUE_TYPE_CLOSURE,
                                  testCase->zeroArg ? known_call_native_return_91 : known_call_native_return_73);
    } else {
        *ownedLeafFunction = build_heap_leaf_function_returning_constant(state,
                                                                         testCase->zeroArg ? 0u : 1u,
                                                                         testCase->expectedResult);
        init_function_value(state, &rootFunction->constantValueList[0], *ownedLeafFunction, ZR_FALSE);
    }

    if (!testCase->zeroArg) {
        ZrCore_Value_InitAsInt(state, &rootFunction->constantValueList[1], 5);
    }

    rootFunction->instructionsList[0] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
    if (!testCase->zeroArg) {
        rootFunction->instructionsList[1] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
    }

    if (testCase->zeroArg) {
        rootFunction->instructionsList[1] = make_instruction_slot_operands(testCase->opcode, 1, 0, 0);
        rootFunction->instructionsList[2] = make_instruction_return(1);
    } else {
        rootFunction->instructionsList[2] = make_instruction_slot_operands(testCase->opcode,
                                                                           2,
                                                                           0,
                                                                           1);
        rootFunction->instructionsList[3] = make_instruction_return(2);
    }

    return rootFunction;
}

// 手工设置调用目标来源与别名路径，再由 quickening 恢复预期 opcode。
static void build_manual_quickening_fixture(SZrState *state,
                                            const SZrKnownCallQuickeningCase *testCase,
                                            SZrFunction *rootFunction,
                                            SZrFunction *childFunctions,
                                            TZrInstruction *rootInstructions,
                                            TZrInstruction *childInstructions,
                                            SZrTypeValue *rootConstants,
                                            SZrTypeValue *childConstants) {
    TZrUInt32 instructionCount = 0;
    TZrUInt32 constantCount = 0;
    TZrUInt32 childFunctionCount = 0;
    TZrUInt16 functionSlot = 0;
    TZrUInt16 resultSlot;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(testCase);
    TEST_ASSERT_NOT_NULL(rootFunction);
    TEST_ASSERT_NOT_NULL(rootInstructions);
    TEST_ASSERT_NOT_NULL(rootConstants);

    memset(rootInstructions, 0, sizeof(TZrInstruction) * 8u);
    memset(rootConstants, 0, sizeof(SZrTypeValue) * 3u);
    if (childFunctions != ZR_NULL) {
        memset(childFunctions, 0, sizeof(SZrFunction) * 1u);
    }
    if (childInstructions != ZR_NULL) {
        memset(childInstructions, 0, sizeof(TZrInstruction) * 2u);
    }
    if (childConstants != ZR_NULL) {
        memset(childConstants, 0, sizeof(SZrTypeValue) * 1u);
    }

    if (testCase->provenance != ZR_KNOWN_CALL_PROVENANCE_NATIVE_FUNCTION_CONSTANT &&
        testCase->provenance != ZR_KNOWN_CALL_PROVENANCE_NATIVE_CLOSURE_CONSTANT &&
        testCase->provenance != ZR_KNOWN_CALL_PROVENANCE_NATIVE_POINTER_CONSTANT) {
        childFunctionCount = 1;
        ZrCore_Value_InitAsInt(state, &childConstants[0], 41);
        childInstructions[0] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        childInstructions[1] = make_instruction_return(0);
        init_manual_test_function(&childFunctions[0], childInstructions, 2, childConstants, 1, ZR_NULL, 0, 1);
        childFunctions[0].parameterCount = testCase->argumentCount;
    }

    switch (testCase->provenance) {
        case ZR_KNOWN_CALL_PROVENANCE_GET_SUB_FUNCTION:
            rootInstructions[instructionCount++] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION),
                                                                                   functionSlot,
                                                                                   0,
                                                                                   0);
            break;
        case ZR_KNOWN_CALL_PROVENANCE_CREATE_CLOSURE:
            init_borrowed_function_value(&rootConstants[constantCount], &childFunctions[0], ZR_FALSE);
            rootInstructions[instructionCount++] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(CREATE_CLOSURE),
                                                                                   functionSlot,
                                                                                   (TZrUInt16)constantCount,
                                                                                   0);
            constantCount++;
            break;
        case ZR_KNOWN_CALL_PROVENANCE_FUNCTION_CONSTANT:
            init_borrowed_function_value(&rootConstants[constantCount], &childFunctions[0], ZR_FALSE);
            rootInstructions[instructionCount++] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                                      functionSlot,
                                                                                      (TZrInt32)constantCount);
            constantCount++;
            break;
        case ZR_KNOWN_CALL_PROVENANCE_CLOSURE_CONSTANT: {
            SZrClosure *closure = ZrCore_Closure_New(state, 0);

            TEST_ASSERT_NOT_NULL(closure);
            closure->function = &childFunctions[0];
            init_closure_value(state, &rootConstants[constantCount], closure, ZR_FALSE);
            rootInstructions[instructionCount++] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                                      functionSlot,
                                                                                      (TZrInt32)constantCount);
            constantCount++;
        } break;
        case ZR_KNOWN_CALL_PROVENANCE_NATIVE_FUNCTION_CONSTANT:
            init_native_closure_value(state,
                                      &rootConstants[constantCount],
                                      ZR_VALUE_TYPE_FUNCTION,
                                      known_call_native_return_73);
            rootInstructions[instructionCount++] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                                      functionSlot,
                                                                                      (TZrInt32)constantCount);
            constantCount++;
            break;
        case ZR_KNOWN_CALL_PROVENANCE_NATIVE_CLOSURE_CONSTANT:
            init_native_closure_value(state,
                                      &rootConstants[constantCount],
                                      ZR_VALUE_TYPE_CLOSURE,
                                      known_call_native_return_73);
            rootInstructions[instructionCount++] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                                      functionSlot,
                                                                                      (TZrInt32)constantCount);
            constantCount++;
            break;
        case ZR_KNOWN_CALL_PROVENANCE_NATIVE_POINTER_CONSTANT:
            ZrCore_Value_InitAsNativePointer(state, &rootConstants[constantCount], (TZrPtr)known_call_native_return_73);
            rootInstructions[instructionCount++] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                                      functionSlot,
                                                                                      (TZrInt32)constantCount);
            constantCount++;
            break;
    }

    if (testCase->aliasKind != ZR_KNOWN_CALL_ALIAS_NONE) {
        rootInstructions[instructionCount++] =
                make_instruction_constant_operand(testCase->aliasKind == ZR_KNOWN_CALL_ALIAS_GET_STACK
                                                          ? ZR_INSTRUCTION_ENUM(GET_STACK)
                                                          : ZR_INSTRUCTION_ENUM(SET_STACK),
                                                  1,
                                                  functionSlot);
        functionSlot = 1;
    }

    if (testCase->argumentCount > 0) {
        ZrCore_Value_InitAsInt(state, &rootConstants[constantCount], 5);
        rootInstructions[instructionCount++] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                                  functionSlot + 1,
                                                                                  (TZrInt32)constantCount);
        constantCount++;
    }

    resultSlot = (TZrUInt16)(functionSlot + testCase->argumentCount + 1u);
    rootInstructions[instructionCount++] =
            make_instruction_slot_operands(testCase->isTailCall ? ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL)
                                                                : ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
                                           resultSlot,
                                           functionSlot,
                                           testCase->argumentCount);
    if (!testCase->isTailCall) {
        rootInstructions[instructionCount++] = make_instruction_return(resultSlot);
    }

    init_manual_test_function(rootFunction,
                              rootInstructions,
                              instructionCount,
                              rootConstants,
                              constantCount,
                              childFunctionCount > 0 ? childFunctions : ZR_NULL,
                              childFunctionCount,
                              resultSlot + 1u);
}

// 按 opcode 矩阵构造严格 AOT 输入，避免源编译阶段跳过冷门指令。
static void build_manual_aot_fixture(SZrState *state,
                                     const SZrKnownCallAotCase *testCase,
                                     SZrFunction *rootFunction,
                                     SZrFunction *childFunctions,
                                     TZrInstruction *rootInstructions,
                                     TZrInstruction *childInstructions,
                                     SZrTypeValue *rootConstants,
                                     SZrTypeValue *childConstants) {
    TZrUInt32 instructionCount = 0;
    TZrUInt32 constantCount = 0;
    TZrUInt32 childFunctionCount = 0;
    TZrUInt16 functionSlot = 0;
    TZrUInt16 resultSlot;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(testCase);
    TEST_ASSERT_NOT_NULL(rootFunction);

    memset(rootInstructions, 0, sizeof(TZrInstruction) * 8u);
    memset(rootConstants, 0, sizeof(SZrTypeValue) * 3u);
    memset(childFunctions, 0, sizeof(SZrFunction) * 1u);
    memset(childInstructions, 0, sizeof(TZrInstruction) * 2u);
    memset(childConstants, 0, sizeof(SZrTypeValue) * 1u);

    if (!testCase->isNative) {
        childFunctionCount = 1;
        ZrCore_Value_InitAsInt(state, &childConstants[0], 41);
        childInstructions[0] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        childInstructions[1] = make_instruction_return(0);
        init_manual_test_function(&childFunctions[0], childInstructions, 2, childConstants, 1, ZR_NULL, 0, 1);
        childFunctions[0].parameterCount = testCase->zeroArg ? 0u : 1u;
        rootInstructions[instructionCount++] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION),
                                                                               functionSlot,
                                                                               0,
                                                                               0);
    } else {
        ZrCore_Value_InitAsNativePointer(state,
                                         &rootConstants[constantCount],
                                         testCase->zeroArg ? (TZrPtr)known_call_native_return_91
                                                           : (TZrPtr)known_call_native_return_73);
        rootInstructions[instructionCount++] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                                  functionSlot,
                                                                                  (TZrInt32)constantCount);
        constantCount++;
    }

    if (!testCase->zeroArg) {
        ZrCore_Value_InitAsInt(state, &rootConstants[constantCount], 5);
        rootInstructions[instructionCount++] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                                  1,
                                                                                  (TZrInt32)constantCount);
        constantCount++;
    }

    resultSlot = (TZrUInt16)(testCase->zeroArg ? 1u : 2u);
    rootInstructions[instructionCount++] = make_instruction_slot_operands(testCase->opcode,
                                                                          resultSlot,
                                                                          functionSlot,
                                                                          testCase->zeroArg ? 0u : 1u);
    if (!testCase->isTailCall) {
        rootInstructions[instructionCount++] = make_instruction_return(resultSlot);
    }

    init_manual_test_function(rootFunction,
                              rootInstructions,
                              instructionCount,
                              rootConstants,
                              constantCount,
                              childFunctionCount > 0 ? childFunctions : ZR_NULL,
                              childFunctionCount,
                              resultSlot + 1u);
}

// 逐项验证 VM 函数来源经别名传播仍 quicken 到正确调用族。
static void test_known_call_quickening_recovers_vm_provenance_matrix(void) {
    static const SZrKnownCallQuickeningCase cases[] = {
            {"get_sub_function_direct", ZR_KNOWN_CALL_PROVENANCE_GET_SUB_FUNCTION, ZR_KNOWN_CALL_ALIAS_NONE, ZR_FALSE, 1,
             ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)},
            {"get_sub_function_alias_get_stack",
             ZR_KNOWN_CALL_PROVENANCE_GET_SUB_FUNCTION,
             ZR_KNOWN_CALL_ALIAS_GET_STACK,
             ZR_FALSE,
             1,
             ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)},
            {"create_closure_direct", ZR_KNOWN_CALL_PROVENANCE_CREATE_CLOSURE, ZR_KNOWN_CALL_ALIAS_NONE, ZR_FALSE, 1,
             ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)},
            {"function_constant_alias_set_stack",
             ZR_KNOWN_CALL_PROVENANCE_FUNCTION_CONSTANT,
             ZR_KNOWN_CALL_ALIAS_SET_STACK,
             ZR_FALSE,
             1,
             ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)},
            {"closure_constant_tail", ZR_KNOWN_CALL_PROVENANCE_CLOSURE_CONSTANT, ZR_KNOWN_CALL_ALIAS_NONE, ZR_TRUE, 1,
             ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL)},
            {"function_constant_zero_arg",
             ZR_KNOWN_CALL_PROVENANCE_FUNCTION_CONSTANT,
             ZR_KNOWN_CALL_ALIAS_NONE,
             ZR_FALSE,
             0,
             ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS)},
            {"get_sub_function_zero_arg_tail",
             ZR_KNOWN_CALL_PROVENANCE_GET_SUB_FUNCTION,
             ZR_KNOWN_CALL_ALIAS_NONE,
             ZR_TRUE,
             0,
             ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS)}};
    SZrKnownCallPipelineTimer timer = {0};
    const char *testSummary = "Known VM Calls Quicken From Local Provenance Matrix";
    TZrSize index;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("known vm quickening",
                 "Testing that GET_SUB_FUNCTION, CREATE_CLOSURE, local function constants, closure constants, and stack-copy aliases quicken into the dedicated VM known-call family instead of staying on generic FUNCTION_CALL/FUNCTION_TAIL_CALL.");

    for (index = 0; index < (TZrSize)(sizeof(cases) / sizeof(cases[0])); index++) {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction rootFunction;
        SZrFunction *childFunctions;
        TZrInstruction *rootInstructions;
        TZrInstruction *childInstructions;
        SZrTypeValue *rootConstants;
        SZrTypeValue *childConstants;

        TEST_ASSERT_NOT_NULL(state);
        childFunctions = allocate_child_functions(state, 1);
        rootInstructions = allocate_function_instructions(state, 8);
        childInstructions = allocate_function_instructions(state, 2);
        rootConstants = allocate_function_constants(state, 3);
        childConstants = allocate_function_constants(state, 1);
        build_manual_quickening_fixture(state,
                                        &cases[index],
                                        &rootFunction,
                                        childFunctions,
                                        rootInstructions,
                                        childInstructions,
                                        rootConstants,
                                        childConstants);

        TEST_ASSERT_TRUE_MESSAGE(compiler_quicken_execbc_function_shallow(state, &rootFunction), cases[index].name);
        TEST_ASSERT_TRUE_MESSAGE(function_contains_opcode(&rootFunction, cases[index].expectedOpcode),
                                 cases[index].name);

        ZrCore_Function_Free(state, &rootFunction);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 逐项验证 native 函数、闭包与指针来源不会误归 VM 调用族。
static void test_known_call_quickening_recovers_native_provenance_matrix(void) {
    static const SZrKnownCallQuickeningCase cases[] = {
            {"native_function_constant", ZR_KNOWN_CALL_PROVENANCE_NATIVE_FUNCTION_CONSTANT, ZR_KNOWN_CALL_ALIAS_NONE,
             ZR_FALSE, 1, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL)},
            {"native_function_constant_alias_get_stack",
             ZR_KNOWN_CALL_PROVENANCE_NATIVE_FUNCTION_CONSTANT,
             ZR_KNOWN_CALL_ALIAS_GET_STACK,
             ZR_FALSE,
             1,
             ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL)},
            {"native_closure_constant", ZR_KNOWN_CALL_PROVENANCE_NATIVE_CLOSURE_CONSTANT, ZR_KNOWN_CALL_ALIAS_NONE,
             ZR_FALSE, 1, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL)},
            {"native_pointer_constant_tail",
             ZR_KNOWN_CALL_PROVENANCE_NATIVE_POINTER_CONSTANT,
             ZR_KNOWN_CALL_ALIAS_NONE,
             ZR_TRUE,
             1,
             ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL)},
            {"native_closure_zero_arg",
             ZR_KNOWN_CALL_PROVENANCE_NATIVE_CLOSURE_CONSTANT,
             ZR_KNOWN_CALL_ALIAS_NONE,
             ZR_FALSE,
             0,
             ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS)},
            {"native_pointer_zero_arg_tail",
             ZR_KNOWN_CALL_PROVENANCE_NATIVE_POINTER_CONSTANT,
             ZR_KNOWN_CALL_ALIAS_NONE,
             ZR_TRUE,
             0,
             ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS)}};
    SZrKnownCallPipelineTimer timer = {0};
    const char *testSummary = "Known Native Calls Quicken From Constant Provenance Matrix";
    TZrSize index;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("known native quickening",
                 "Testing that native function constants, native closure constants, native pointer constants, and stack-copy aliases quicken into the dedicated native known-call family instead of falling back to generic call dispatch.");

    for (index = 0; index < (TZrSize)(sizeof(cases) / sizeof(cases[0])); index++) {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction rootFunction;
        TZrInstruction *rootInstructions;
        SZrTypeValue *rootConstants;

        TEST_ASSERT_NOT_NULL(state);
        rootInstructions = allocate_function_instructions(state, 8);
        rootConstants = allocate_function_constants(state, 3);
        build_manual_quickening_fixture(state,
                                        &cases[index],
                                        &rootFunction,
                                        ZR_NULL,
                                        rootInstructions,
                                        ZR_NULL,
                                        rootConstants,
                                        ZR_NULL);

        TEST_ASSERT_TRUE_MESSAGE(compiler_quicken_execbc_function_shallow(state, &rootFunction), cases[index].name);
        TEST_ASSERT_TRUE_MESSAGE(function_contains_opcode(&rootFunction, cases[index].expectedOpcode),
                                 cases[index].name);

        ZrCore_Function_Free(state, &rootFunction);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 实际运行手工已知调用矩阵并对照返回值，覆盖后端文本断言之外的解释执行。
static void test_known_call_runtime_executes_manual_opcode_matrix(void) {
    static const SZrKnownCallRuntimeCase cases[] = {
            {"known_vm_call", ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), ZR_FALSE, ZR_FALSE, ZR_FALSE, 41},
            {"super_known_vm_call_no_args", ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS), ZR_FALSE, ZR_FALSE,
             ZR_TRUE, 91},
            {"known_vm_tail_call", ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL), ZR_FALSE, ZR_TRUE, ZR_FALSE, 41},
            {"super_known_vm_tail_call_no_args", ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS), ZR_FALSE,
             ZR_TRUE, ZR_TRUE, 91},
            {"known_native_call", ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL), ZR_TRUE, ZR_FALSE, ZR_FALSE, 73},
            {"super_known_native_call_no_args", ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS), ZR_TRUE,
             ZR_FALSE, ZR_TRUE, 91},
            {"known_native_tail_call", ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL), ZR_TRUE, ZR_TRUE, ZR_FALSE, 73},
            {"super_known_native_tail_call_no_args", ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS),
             ZR_TRUE, ZR_TRUE, ZR_TRUE, 91}};
    SZrKnownCallPipelineTimer timer = {0};
    const char *testSummary = "Known Call Runtime Executes Manual Opcode Matrix";
    TZrSize index;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("known call runtime execution",
                 "Testing that the full known-call opcode family executes under the interpreter for both VM and native callables, including zero-arg and tail-call variants.");

    for (index = 0; index < (TZrSize)(sizeof(cases) / sizeof(cases[0])); index++) {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *rootFunction;
        SZrFunction *leafFunction = ZR_NULL;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);
        rootFunction = build_runtime_known_call_fixture(state, &cases[index], &leafFunction);
        TEST_ASSERT_NOT_NULL(rootFunction);
        TEST_ASSERT_TRUE_MESSAGE(function_contains_opcode(rootFunction, cases[index].opcode), cases[index].name);
        TEST_ASSERT_TRUE_MESSAGE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, rootFunction, &result),
                                 cases[index].name);
        TEST_ASSERT_EQUAL_INT64_MESSAGE(cases[index].expectedResult, result, cases[index].name);

        ZrCore_Function_Free(state, rootFunction);
        if (leafFunction != ZR_NULL) {
            ZrCore_Function_Free(state, leafFunction);
        }
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 用真实类型化成员源码确认编译器产生已知 VM 成员调用族。
static void test_typed_member_calls_quicken_to_known_vm_member_call_family(void) {
    SZrKnownCallPipelineTimer timer = {0};
    const char *testSummary = "Typed Member Calls Quicken To Known VM Member Call Family";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("typed member known-call quickening",
                 "Testing that typed instance method calls lower into the KNOWN_VM_MEMBER_CALL family instead of separate member loads plus generic call dispatch.");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_typed_member_known_call_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS)));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(14, result);

        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 以手工 opcode 矩阵核对严格 C/LLVM 工件没有 unsupported 路径。TODO: 需补生成工件的执行验证。
static void test_known_call_aot_backends_support_full_opcode_matrix(void) {
    static const SZrKnownCallAotCase cases[] = {
            {"known_vm_call", "KNOWN_VM_CALL", ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), ZR_FALSE, ZR_FALSE, ZR_FALSE},
            {"super_known_vm_call_no_args",
             "SUPER_KNOWN_VM_CALL_NO_ARGS",
             ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS),
             ZR_FALSE,
             ZR_FALSE,
             ZR_TRUE},
            {"known_vm_tail_call",
             "KNOWN_VM_TAIL_CALL",
             ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL),
             ZR_FALSE,
             ZR_TRUE,
             ZR_FALSE},
            {"super_known_vm_tail_call_no_args",
             "SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS",
             ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS),
             ZR_FALSE,
             ZR_TRUE,
             ZR_TRUE},
            {"known_native_call",
             "KNOWN_NATIVE_CALL",
             ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL),
             ZR_TRUE,
             ZR_FALSE,
             ZR_FALSE},
            {"super_known_native_call_no_args",
             "SUPER_KNOWN_NATIVE_CALL_NO_ARGS",
             ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS),
             ZR_TRUE,
             ZR_FALSE,
             ZR_TRUE},
            {"known_native_tail_call",
             "KNOWN_NATIVE_TAIL_CALL",
             ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL),
             ZR_TRUE,
             ZR_TRUE,
             ZR_FALSE},
            {"super_known_native_tail_call_no_args",
             "SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS",
             ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS),
             ZR_TRUE,
             ZR_TRUE,
             ZR_TRUE}};
    SZrKnownCallPipelineTimer timer = {0};
    const char *testSummary = "Known Call AOT Backends Support Full Opcode Matrix";
    TZrSize index;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("known call aot support",
                 "Testing that the full known-call opcode family survives intermediate emission and stays supported by strict AOT C and LLVM lowering without falling back to unsupported-opcode reporting.");

    for (index = 0; index < (TZrSize)(sizeof(cases) / sizeof(cases[0])); index++) {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction rootFunction;
        SZrFunction childFunctions[1];
        TZrInstruction rootInstructions[8];
        TZrInstruction childInstructions[2];
        SZrTypeValue rootConstants[3];
        SZrTypeValue childConstants[1];
        char intermediatePath[128];
        char cPath[128];
        char llvmPath[128];
        char moduleName[128];
        char *intermediateText;
        char *cText;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        build_manual_aot_fixture(state,
                                 &cases[index],
                                 &rootFunction,
                                 childFunctions,
                                 rootInstructions,
                                 childInstructions,
                                 rootConstants,
                                 childConstants);
        TEST_ASSERT_TRUE_MESSAGE(function_tree_contains_opcode(&rootFunction, cases[index].opcode), cases[index].name);

        snprintf(moduleName, sizeof(moduleName), "known_call_%s_fixture", cases[index].name);
        snprintf(intermediatePath, sizeof(intermediatePath), "%s.zri", moduleName);
        snprintf(cPath, sizeof(cPath), "%s.c", moduleName);
        snprintf(llvmPath, sizeof(llvmPath), "%s.ll", moduleName);
        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);

        TEST_ASSERT_TRUE_MESSAGE(ZrParser_Writer_WriteIntermediateFile(state, &rootFunction, intermediatePath),
                                 cases[index].name);
        TEST_ASSERT_TRUE_MESSAGE(write_standalone_strict_aot_c_file(state, &rootFunction, moduleName, cPath),
                                 cases[index].name);
        TEST_ASSERT_TRUE_MESSAGE(write_standalone_strict_aot_llvm_file(state, &rootFunction, moduleName, llvmPath),
                                 cases[index].name);

        intermediateText = read_text_file_owned(intermediatePath);
        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL_MESSAGE(intermediateText, cases[index].name);
        TEST_ASSERT_NOT_NULL_MESSAGE(cText, cases[index].name);
        TEST_ASSERT_NOT_NULL_MESSAGE(llvmText, cases[index].name);

        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(intermediateText, cases[index].opcodeName), cases[index].name);
        TEST_ASSERT_FALSE_MESSAGE(aot_c_text_contains_unsupported_opcode(cText, (TZrUInt32)cases[index].opcode),
                                  cases[index].name);
        TEST_ASSERT_FALSE_MESSAGE(aot_llvm_text_contains_unsupported_opcode(llvmText, (TZrUInt32)cases[index].opcode),
                                  cases[index].name);

        if (cases[index].isNative) {
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(cText, "ZrLibrary_AotRuntime_CallStackValue"), cases[index].name);
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(llvmText, "@ZrLibrary_AotRuntime_PrepareDirectCall"),
                                         cases[index].name);
        } else {
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(cText, "ZrLibrary_AotRuntime_PrepareStaticDirectCall"),
                                         cases[index].name);
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(cText, "zr_aot_static_direct_succeeded = (TZrBool)(zr_aot_fn_"),
                                         cases[index].name);
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(cText, "ZrLibrary_AotRuntime_CompletePreparedDirectCallWithResume"),
                                         cases[index].name);
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(cText, "goto zr_aot_fn_"),
                                         cases[index].name);
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(llvmText, "@ZrLibrary_AotRuntime_PrepareStaticDirectCall"),
                                         cases[index].name);
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(llvmText, "@ZrLibrary_AotRuntime_CompletePreparedDirectCallWithResume"),
                                         cases[index].name);
        }

        free(intermediateText);
        free(cText);
        free(llvmText);
        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void setUp(void) {}

void tearDown(void) {}

// Unity 入口依序覆盖 VM/native quickening、手工运行时、真实成员编译及双后端矩阵。
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_known_call_quickening_recovers_vm_provenance_matrix);
    RUN_TEST(test_known_call_quickening_recovers_native_provenance_matrix);
    RUN_TEST(test_known_call_runtime_executes_manual_opcode_matrix);
    RUN_TEST(test_typed_member_calls_quicken_to_known_vm_member_call_family);
    RUN_TEST(test_known_call_aot_backends_support_full_opcode_matrix);
    return UNITY_END();
}

/**
 * @unit AOTPIPE-011
 * @typedef SZrKnownCallPipelineTimer
 *
 * ## Summary
 * 保存已知调用各用例的 clock() 起止读数，只作性能诊断。
 *
 * ## Fields
 * - `clock_t startTime;`
 * - `clock_t endTime;`
 *
 * ## Value model
 * 两个 clock_t 字段均为用例栈变量。
 *
 * ## Invariants
 * 耗时不参与 opcode、运行值或后端文本的断言。
 *
 * ## Constraints and lifecycle
 * 本文件的局部测试记录，不形成公共 ABI 或跨线程共享协议。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:21
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:799
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:876
 */

/**
 * @unit AOTPIPE-012
 * @typedef EZrKnownCallQuickeningProvenanceKind
 *
 * ## Summary
 * 描述手工 quickening fixture 中调用目标的七种来源。
 *
 * ## Fields
 * - `ZR_KNOWN_CALL_PROVENANCE_GET_SUB_FUNCTION = 0,`
 * - `ZR_KNOWN_CALL_PROVENANCE_CREATE_CLOSURE = 1,`
 * - `ZR_KNOWN_CALL_PROVENANCE_FUNCTION_CONSTANT = 2,`
 * - `ZR_KNOWN_CALL_PROVENANCE_CLOSURE_CONSTANT = 3,`
 * - `ZR_KNOWN_CALL_PROVENANCE_NATIVE_FUNCTION_CONSTANT = 4,`
 * - `ZR_KNOWN_CALL_PROVENANCE_NATIVE_CLOSURE_CONSTANT = 5,`
 * - `ZR_KNOWN_CALL_PROVENANCE_NATIVE_POINTER_CONSTANT = 6`
 *
 * ## Value model
 * 0–6 分别表示子函数、创建闭包、函数常量、闭包常量、native 函数常量、native 闭包常量和 native 指针常量。
 *
 * ## Invariants
 * 取值由静态矩阵给出，必须与 build_manual_quickening_fixture 的 switch 分支一致；不是公共 wire enum。
 *
 * ## Constraints and lifecycle
 * 本文件的局部测试记录，不形成公共 ABI 或跨线程共享协议。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:27
 */

/**
 * @unit AOTPIPE-013
 * @typedef EZrKnownCallAliasKind
 *
 * ## Summary
 * 为目标来源矩阵选择不复制、GET_STACK 或 SET_STACK 的别名传播路径。
 *
 * ## Fields
 * - `ZR_KNOWN_CALL_ALIAS_NONE = 0,`
 * - `ZR_KNOWN_CALL_ALIAS_GET_STACK = 1,`
 * - `ZR_KNOWN_CALL_ALIAS_SET_STACK = 2`
 *
 * ## Value model
 * NONE=0、GET_STACK=1、SET_STACK=2；存储在本地用例行。
 *
 * ## Invariants
 * 只控制手工生成的槽位复制；不表示运行时任意别名分析结果。
 *
 * ## Constraints and lifecycle
 * 本文件的局部测试记录，不形成公共 ABI 或跨线程共享协议。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:38
 */

/**
 * @unit AOTPIPE-014
 * @typedef SZrKnownCallQuickeningCase
 *
 * ## Summary
 * 绑定 quickening 用例的来源、别名、参数数、尾调用及预期 opcode。
 *
 * ## Fields
 * - `const char *name;`
 * - `EZrKnownCallQuickeningProvenanceKind provenance;`
 * - `EZrKnownCallAliasKind aliasKind;`
 * - `TZrBool isTailCall;`
 * - `TZrUInt16 argumentCount;`
 * - `EZrInstructionCode expectedOpcode;`
 *
 * ## Value model
 * name 为静态诊断名；provenance/aliasKind 选择构造分支；isTailCall/argumentCount 决定通用调用形态；expectedOpcode 是 quickening 后的断言目标。
 *
 * ## Invariants
 * 当前表仅使用 0 或 1 个参数；行与构造器及固定缓冲容量配套，不是任意用户输入。
 *
 * ## Constraints and lifecycle
 * 本文件的局部测试记录，不形成公共 ABI 或跨线程共享协议。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:45
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:549
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:768
 */

/**
 * @unit AOTPIPE-015
 * @typedef SZrKnownCallRuntimeCase
 *
 * ## Summary
 * 绑定手工已知调用解释执行的 opcode 与预期整数。
 *
 * ## Fields
 * - `const char *name;`
 * - `EZrInstructionCode opcode;`
 * - `TZrBool isNative;`
 * - `TZrBool isTailCall;`
 * - `TZrBool zeroArg;`
 * - `TZrInt64 expectedResult;`
 *
 * ## Value model
 * name/opcode/isNative/isTailCall/zeroArg 选择 VM 或 native 执行路径；expectedResult 为 41、73 或 91。
 *
 * ## Invariants
 * 八行只覆盖当前组合；native 回调返回的结果个数与 expectedResult 的数值不同。
 *
 * ## Constraints and lifecycle
 * 本文件的局部测试记录，不形成公共 ABI 或跨线程共享协议。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:55
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:491
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:918
 */

/**
 * @unit AOTPIPE-016
 * @typedef SZrKnownCallAotCase
 *
 * ## Summary
 * 描述严格 C/LLVM 文本生成矩阵的八种已知调用形态。
 *
 * ## Fields
 * - `const char *name;`
 * - `const char *opcodeName;`
 * - `EZrInstructionCode opcode;`
 * - `TZrBool isNative;`
 * - `TZrBool isTailCall;`
 * - `TZrBool zeroArg;`
 *
 * ## Value model
 * name 形成相对工件名；opcodeName 供 .zri 文本搜索；opcode/isNative/isTailCall/zeroArg 决定手工函数。
 *
 * ## Invariants
 * opcodeName 必须与 opcode 对应；此表的成功只证明生成文本约束，不代表生成物已执行。
 *
 * ## Constraints and lifecycle
 * 本文件的局部测试记录，不形成公共 ABI 或跨线程共享协议。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:65
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:694
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1004
 */

/**
 * @unit AOTPIPE-017
 * @fn TZrInt64 known_call_native_return_73(struct SZrState *state)
 *
 * ## Summary
 * 作为 native 测试目标，把整数 73 写入当前调用结果槽。
 *
 * ## Parameters
 * @param[in,out] state native dispatch 的当前调用状态；无有效结果槽时返回 0，成功写结果并更新 stackTop。
 *
 * ## Returns
 * @return 有效槽写入一个结果并返回结果个数 1；state/callInfo/base/结果槽无效时返回 0。
 *
 * ## Constraints and lifecycle
 * 不分配目标函数；将 stackTop 推到结果槽后一格。返回值不是整数 73 本身；由 native dispatch 同步调用。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:75
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:516
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:628
 */

/**
 * @unit AOTPIPE-018
 * @fn TZrInt64 known_call_native_return_91(struct SZrState *state)
 *
 * ## Summary
 * 为零参 native 路径提供与 73 可区分的整数结果 91。
 *
 * ## Parameters
 * @param[in,out] state native dispatch 的当前调用状态；无有效结果槽时返回 0，成功写结果并更新 stackTop。
 *
 * ## Returns
 * @return 有效槽写入一个结果并返回结果个数 1；state/callInfo/base/结果槽无效时返回 0。
 *
 * ## Constraints and lifecycle
 * 写结果后更新 stackTop；不分配目标函数。回调返回个数与写入的整数值分别检查。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:95
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:516
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:731
 */

/**
 * @unit AOTPIPE-019
 * @fn char *read_text_file_owned(const TZrChar *path)
 *
 * ## Summary
 * 把生成工件按字节读入独立的 NUL 结尾缓冲，供文本断言使用。
 *
 * ## Parameters
 * @param[in] path 待只读打开的 NUL 结尾路径；为空返回 NULL。
 *
 * ## Returns
 * @return 成功返回 malloc 缓冲，调用方负责 free；打开、定位、分配或短读失败返回 NULL。
 *
 * ## Constraints and lifecycle
 * 失败分支关闭已打开的 FILE，并释放本函数已分配的缓冲；不删除源文件，不把工件文本当作可执行验证。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:115
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1099
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1100
 */

/**
 * @unit AOTPIPE-020
 * @fn TZrBool aot_c_text_contains_unsupported_opcode(const char *text, TZrUInt32 opcode)
 *
 * ## Summary
 * 识别当前 C emitter 为指定 opcode 生成的 unsupported 返回片段。
 *
 * ## Parameters
 * @param[in] text 借用 NUL 结尾的生成文本；为空按未命中处理。
 * @param[in] opcode 需要构造或查找的 opcode；本 helper 不验证其全部语义。
 *
 * ## Returns
 * @return 同一行同时命中固定前缀和 opcode 后缀返回 ZR_TRUE；NULL 或未命中返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 依赖 emitter 的文本格式与换行边界，属于文本 oracle。TODO: AOTPIPE-AOT-EXEC-01：不能据未命中推导生成物运行语义。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:158
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1107
 */

/**
 * @unit AOTPIPE-021
 * @fn TZrBool aot_llvm_text_contains_unsupported_opcode(const char *text, TZrUInt32 opcode)
 *
 * ## Summary
 * 识别 LLVM 文本中指定 opcode 的 unsupported helper 调用。
 *
 * ## Parameters
 * @param[in] text 借用 NUL 结尾的生成文本；为空按未命中处理。
 * @param[in] opcode 需要构造或查找的 opcode；本 helper 不验证其全部语义。
 *
 * ## Returns
 * @return 同一行同时命中固定调用前缀与 opcode 后缀返回 ZR_TRUE；NULL 或未命中返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 仅检索字符串，不解析 LLVM IR 或证明路径不可达。TODO: AOTPIPE-AOT-EXEC-01：生成物可执行语义未由本 helper 验证。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:181
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1109
 */

/**
 * @unit AOTPIPE-022
 * @fn TZrBool write_standalone_strict_aot_c_file(SZrState *state, SZrFunction *function, const TZrChar *moduleName, const TZrChar *filename)
 *
 * ## Summary
 * 用固定 source 身份和 requireExecutableLowering 选项生成 C 工件。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[in] function 借用已构造的函数；入口为空返回 FALSE，不接管其数组。
 * @param[in] moduleName 本次生成的模块身份；同时借作 inputHash，不在此计算源码摘要。
 * @param[in] filename 本次 writer 的目标路径，可能在失败时留下部分工件。
 *
 * ## Returns
 * @return 任一入口指针为 NULL 时返回 ZR_FALSE；否则原样传播 C writer 的布尔结果。
 *
 * ## Constraints and lifecycle
 * options 在栈上零初始化，inputHash 借用 moduleName，requireFullAot 等其余选项保持零；同步调用结束后不交付 options 指针。返回真不等于生成 C 已被编译或执行。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:204
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1094
 */

/**
 * @unit AOTPIPE-023
 * @fn TZrBool write_standalone_strict_aot_llvm_file(SZrState *state, SZrFunction *function, const TZrChar *moduleName, const TZrChar *filename)
 *
 * ## Summary
 * 按与 C 包装器相同的身份和严格 lowering 选项生成 LLVM 文本。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[in] function 借用已构造的函数；入口为空返回 FALSE，不接管其数组。
 * @param[in] moduleName 本次生成的模块身份；同时借作 inputHash，不在此计算源码摘要。
 * @param[in] filename 本次 writer 的目标路径，可能在失败时留下部分工件。
 *
 * ## Returns
 * @return 任一入口指针为 NULL 时返回 ZR_FALSE；否则传播 LLVM writer 的布尔结果。
 *
 * ## Constraints and lifecycle
 * 只在本次同步调用中借用 moduleName 与 filename；没有事务回滚或部分文件清除承诺，也不调用 LLVM 工具链。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:223
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1096
 */

/**
 * @unit AOTPIPE-024
 * @fn TZrInstruction make_instruction_no_operands(EZrInstructionCode opcode, TZrUInt16 operandExtra)
 *
 * ## Summary
 * 构造清零后仅填 opcode 和 extra 的手工指令值。
 *
 * ## Parameters
 * @param[in] opcode 需要构造或查找的 opcode；本 helper 不验证其全部语义。
 * @param[in] operandExtra 写入指令的 16 位 extra 字段。
 *
 * ## Returns
 * @return 按值返回 TZrInstruction。
 *
 * ## Constraints and lifecycle
 * opcode 转为 16 位，不校验枚举或 extra 合法性；供受控矩阵构造，不能当一般指令验证器。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:242
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:255
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:266
 */

/**
 * @unit AOTPIPE-025
 * @fn TZrInstruction make_instruction_constant_operand(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrInt32 operandA2)
 *
 * ## Summary
 * 在无槽指令基础上写入第一个有符号 32 位常量操作数。
 *
 * ## Parameters
 * @param[in] opcode 需要构造或查找的 opcode；本 helper 不验证其全部语义。
 * @param[in] operandExtra 写入指令的 16 位 extra 字段。
 * @param[in] operandA2 写入 operand2[0] 的 32 位常量操作数。
 *
 * ## Returns
 * @return 按值返回构造指令。
 *
 * ## Constraints and lifecycle
 * operandA2 写入 operand2[0]；常量索引及数组边界由 fixture 保证。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:252
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:484
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:528
 */

/**
 * @unit AOTPIPE-026
 * @fn TZrInstruction make_instruction_slot_operands(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt16 operandA1, TZrUInt16 operandB1)
 *
 * ## Summary
 * 在基础指令中填入两个 16 位槽操作数。
 *
 * ## Parameters
 * @param[in] opcode 需要构造或查找的 opcode；本 helper 不验证其全部语义。
 * @param[in] operandExtra 写入指令的 16 位 extra 字段。
 * @param[in] operandA1 写入 operand1[0] 的槽编号。
 * @param[in] operandB1 写入 operand1[1] 的槽编号。
 *
 * ## Returns
 * @return 按值返回构造指令。
 *
 * ## Constraints and lifecycle
 * operandA1/B1 写入 operand1[0]/[1]，不校验槽是否落在 VM 栈内。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:262
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:275
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:534
 */

/**
 * @unit AOTPIPE-027
 * @fn TZrInstruction make_instruction_return(TZrUInt16 sourceSlot)
 *
 * ## Summary
 * 生成从给定槽返回一个值的 FUNCTION_RETURN 指令。
 *
 * ## Parameters
 * @param[in] sourceSlot 单值返回指令读取的源槽。
 *
 * ## Returns
 * @return 按值返回 extra=1、第二槽操作数为 0 的指令。
 *
 * ## Constraints and lifecycle
 * sourceSlot 必须与 fixture 的 stackSize 配套；不分配或读写运行时栈。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:274
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:485
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:535
 */

/**
 * @unit AOTPIPE-028
 * @fn void init_manual_test_function(SZrFunction *function, TZrInstruction *instructions, TZrUInt32 instructionCount, SZrTypeValue *constants, TZrUInt32 constantCount, SZrFunction *childFunctions, TZrUInt32 childFunctionCount, TZrUInt32 stackSize)
 *
 * ## Summary
 * 把调用方提供的数组挂接到清零的函数记录上，构造最小手工 prototype。
 *
 * ## Parameters
 * @param[out] function 清零后写入借用数组及长度的函数记录；必须非 NULL。
 * @param[in] instructions 借用指令数组；instructionCount 为可用项数。
 * @param[in] instructionCount 挂接到函数记录的指令项数，须与数组匹配。
 * @param[in] constants 借用常量数组，后续索引项须已初始化。
 * @param[in] constantCount 借用常量数组项数。
 * @param[in] childFunctions 借用连续子函数数组；适用分支的容量与 childFunctionCount 或固定 fixture 配套。
 * @param[in] childFunctionCount 借用子函数记录数。
 * @param[in] stackSize 为手工 prototype 声明的栈槽容量。
 *
 * ## Constraints and lifecycle
 * 仅断言 function 非 NULL；数组不复制，指针和长度须有效且活到所有消费者结束。堆数组模式可由 Function_Free 释放；栈数组 AOT 模式不能调用该释放入口。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:279
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:587
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:682
 */

/**
 * @unit AOTPIPE-029
 * @fn TZrBool function_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode)
 *
 * ## Summary
 * 检查当前函数的 ExecBC 表是否含指定 opcode，供本文件断言使用。
 *
 * ## Parameters
 * @param[in] function 借用待检查的函数及其数组；为空时按本函数的返回规则处理。
 * @param[in] opcode 需要构造或查找的 opcode；本 helper 不验证其全部语义。
 *
 * ## Returns
 * @return 找到返回 ZR_TRUE；函数或指令表为空、或未找到时返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 只读当前 instructionList，不递归子函数，也不验证操作数或执行行为。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:302
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:322
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:833
 */

/**
 * @unit AOTPIPE-030
 * @fn TZrBool function_tree_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode)
 *
 * ## Summary
 * 在当前函数及递归子函数表中查找指定 ExecBC opcode。
 *
 * ## Parameters
 * @param[in] function 借用待检查的函数及其数组；为空时按本函数的返回规则处理。
 * @param[in] opcode 需要构造或查找的 opcode；本 helper 不验证其全部语义。
 *
 * ## Returns
 * @return 任一层找到即返回 ZR_TRUE；空函数或整棵树无匹配返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 只读受控无环 fixture 树；不扫描独立 SemIR/deopt 结构，也不执行函数。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:319
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:330
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1082
 */

/**
 * @unit AOTPIPE-031
 * @fn SZrFunction *compile_typed_member_known_call_fixture(SZrState *state)
 *
 * ## Summary
 * 编译 Counter 的 step/read 成员调用源码，供真实编译器 quickening 与执行断言使用。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 *
 * ## Returns
 * @return 成功返回编译函数，编译普通失败返回 NULL；state/sourceName 的断言失败由 Unity 中止本例。
 *
 * ## Constraints and lifecycle
 * Counter.value 先设 3，step(4) 改为 7，read() 得 7，顶层相加为 14；调用方释放函数并销毁 state。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:339
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:983
 */

/**
 * @unit AOTPIPE-032
 * @fn void init_function_value(SZrState *state, SZrTypeValue *value, SZrFunction *function, TZrBool isNative)
 *
 * ## Summary
 * 为运行期矩阵包装 GC 可追踪的函数引用，并设置 FUNCTION/native 标签。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[out] value 要重新初始化的值记录；调用前须满足此受控 fixture 的可覆盖前提。
 * @param[in] function 借用待包装的函数对象，消费者结束前必须存活。
 * @param[in] isNative 写入值标签的 native 标志。
 *
 * ## Constraints and lifecycle
 * state/value/function 必须非 NULL；InitAsRawObject 的 ownershipKind 为 NONE、control/weakRef 为 NULL，不建立显式 ownership control。函数和 VM 必须在消费者结束前存活。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:366
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:521
 * - zr_vm_core/src/zr_vm_core/value/value.c:52
 */

/**
 * @unit AOTPIPE-033
 * @fn void init_borrowed_function_value(SZrTypeValue *value, SZrFunction *function, TZrBool isNative)
 *
 * ## Summary
 * 把现有手工函数装入不参与 GC 追踪的借用值。
 *
 * ## Parameters
 * @param[out] value 要重新初始化的值记录；调用前须满足此受控 fixture 的可覆盖前提。
 * @param[in] function 借用待包装的函数对象，消费者结束前必须存活。
 * @param[in] isNative 写入值标签的 native 标志。
 *
 * ## Constraints and lifecycle
 * value/function 必须非 NULL；整个值先清零，ownershipKind=NONE，control/weakRef=NULL，isGarbageCollectable=FALSE。调用方保证函数及其借用数组的寿命。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:377
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:599
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:607
 */

/**
 * @unit AOTPIPE-034
 * @fn void init_closure_value(SZrState *state, SZrTypeValue *value, SZrClosure *closure, TZrBool isNative)
 *
 * ## Summary
 * 把现有闭包装入 GC 可追踪的 CLOSURE 值，供闭包来源 quickening 使用。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[out] value 要重新初始化的值记录；调用前须满足此受控 fixture 的可覆盖前提。
 * @param[in] closure 借用已创建的闭包对象。
 * @param[in] isNative 写入值标签的 native 标志。
 *
 * ## Constraints and lifecycle
 * state/value/closure 必须非 NULL；包装不复制闭包或捕获内容，闭包由同一 VM 的生命周期承载。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:392
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:618
 */

/**
 * @unit AOTPIPE-035
 * @fn void init_native_closure_value(SZrState *state, SZrTypeValue *value, EZrValueType valueType, FZrNativeFunction nativeFunction)
 *
 * ## Summary
 * 新建零捕获 native 闭包并写入指定回调，再按用例标签包装。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[out] value 要重新初始化的值记录；调用前须满足此受控 fixture 的可覆盖前提。
 * @param[in] valueType 当前矩阵指定的 FUNCTION 或 CLOSURE 标签。
 * @param[in] nativeFunction 同步 native 调用 ABI 回调地址；须活到函数消费结束。
 *
 * ## Constraints and lifecycle
 * 入口 state/value 非 NULL；分配失败通过 Unity 断言中止。valueType 在当前矩阵为 FUNCTION 或 CLOSURE，isNative 固定真；回调只在后续运行期矩阵执行，quickening/AOT 文本矩阵不执行它。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:403
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:513
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:625
 */

/**
 * @unit AOTPIPE-036
 * @fn TZrInstruction *allocate_function_instructions(SZrState *state, TZrUInt32 count)
 *
 * ## Summary
 * 为手工函数分配并清零 state allocator 的指令数组。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[in] count 正的数组项数；固定用例控制其大小。
 *
 * ## Returns
 * @return 返回 count 项数组；count==0 或分配为空触发 Unity 断言，不以 NULL 返回处理失败。
 *
 * ## Constraints and lifecycle
 * 使用 FUNCTION 分配类别；调用者负责挂接、填充有效指令和正常 Function_Free/VM 收尾。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:423
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:477
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:506
 */

/**
 * @unit AOTPIPE-037
 * @fn SZrTypeValue *allocate_function_constants(SZrState *state, TZrUInt32 count)
 *
 * ## Summary
 * 为手工函数分配并清零 state allocator 的常量数组。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[in] count 正的数组项数；固定用例控制其大小。
 *
 * ## Returns
 * @return 返回 count 项数组；零 count 或分配为空由 Unity 断言中止。
 *
 * ## Constraints and lifecycle
 * 所有会被指令索引的项须在执行/生成前初始化；不是独立 malloc 缓冲，按 VM 函数生命周期释放。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:437
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:479
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:508
 */

/**
 * @unit AOTPIPE-038
 * @fn SZrFunction *allocate_child_functions(SZrState *state, TZrUInt32 count)
 *
 * ## Summary
 * 为 VM 来源矩阵分配并清零连续子函数记录。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[in] count 正的数组项数；固定用例控制其大小。
 *
 * ## Returns
 * @return 返回 count 项 SZrFunction 数组；零 count 或分配为空由 Unity 断言中止。
 *
 * ## Constraints and lifecycle
 * 当前只有 VM quickening 表使用；子函数数组及内部数组配套交给根函数的正常释放路径。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:451
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:818
 */

/**
 * @unit AOTPIPE-039
 * @fn SZrFunction *build_heap_leaf_function_returning_constant(SZrState *state, TZrUInt16 parameterCount, TZrInt64 returnValue)
 *
 * ## Summary
 * 创建返回给定整数的两指令堆叶子函数。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[in] parameterCount 叶子函数声明的参数数，当前用例为 0 或 1。
 * @param[in] returnValue 叶子函数返回的固定整数。
 *
 * ## Returns
 * @return 成功返回叶子函数；入口或分配断言失败中止当前 Unity 用例。
 *
 * ## Constraints and lifecycle
 * parameterCount 决定参数数量，stackSize 固定 1；GET_CONSTANT 后 FUNCTION_RETURN。返回对象由调用者在运行矩阵收尾时单独 Function_Free。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:465
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:518
 */

/**
 * @unit AOTPIPE-040
 * @fn SZrFunction *build_runtime_known_call_fixture(SZrState *state, const SZrKnownCallRuntimeCase *testCase, SZrFunction **ownedLeafFunction)
 *
 * ## Summary
 * 按一行运行矩阵建立根函数及 VM 叶子或 native 闭包目标。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[in] testCase 本文件静态矩阵的一行，选择构造形态及预期结果。
 * @param[out] ownedLeafFunction 入口先置 NULL；VM 行随后发布需单独释放的叶子，native 行保持 NULL。
 *
 * ## Returns
 * @return 返回根函数；通过 ownedLeafFunction 发布需要单独释放的 VM 叶子，native 行保持该输出 NULL。
 *
 * ## Constraints and lifecycle
 * 输出先置 NULL 再逐步分配，失败断言不提供回滚。根常量表仅引用叶子，根和叶子的 Function_Free 在正常路径分别调用。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:490
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:947
 */

/**
 * @unit AOTPIPE-041
 * @fn void build_manual_quickening_fixture(SZrState *state, const SZrKnownCallQuickeningCase *testCase, SZrFunction *rootFunction, SZrFunction *childFunctions, TZrInstruction *rootInstructions, TZrInstruction *childInstructions, SZrTypeValue *rootConstants, SZrTypeValue *childConstants)
 *
 * ## Summary
 * 按来源与别名行构造通用调用指令，留给浅层 quickening 选择已知调用族。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[in] testCase 本文件静态矩阵的一行，选择构造形态及预期结果。
 * @param[out] rootFunction 写入根函数记录并借用调用方数组。
 * @param[out] childFunctions VM 来源至少 1 项，native 来源当前传 NULL；写入记录后借用子数组。
 * @param[out] rootInstructions 至少 8 项的根指令数组，构造过程中清零并填充。
 * @param[out] childInstructions VM 来源至少 2 项的子函数指令数组；适用的 native quickening 分支可为 NULL。
 * @param[out] rootConstants 至少 3 项的根常量数组，构造过程中清零并填充。
 * @param[out] childConstants VM 来源至少 1 项的子函数常量数组；适用的 native quickening 分支可为 NULL。
 *
 * ## Constraints and lifecycle
 * rootInstructions 至少 8 项、rootConstants 至少 3 项；VM 来源另需 childFunctions[1]/childInstructions[2]/childConstants[1]，native 来源允许这些 child 指针为 NULL。函数记录借用数组；当前调用者传 heap 数组并由根 Function_Free 收尾。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:548
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:823
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:894
 */

/**
 * @unit AOTPIPE-042
 * @fn void build_manual_aot_fixture(SZrState *state, const SZrKnownCallAotCase *testCase, SZrFunction *rootFunction, SZrFunction *childFunctions, TZrInstruction *rootInstructions, TZrInstruction *childInstructions, SZrTypeValue *rootConstants, SZrTypeValue *childConstants)
 *
 * ## Summary
 * 直接构造已知调用 opcode 的栈借用函数，隔离后端生成检查。
 *
 * ## Parameters
 * @param[in,out] state 本用例的 VM 状态；分配、编译、包装或执行期间须保持有效，不跨线程共享。
 * @param[in] testCase 本文件静态矩阵的一行，选择构造形态及预期结果。
 * @param[out] rootFunction 写入根函数记录并借用调用方数组。
 * @param[out] childFunctions 测试栈上的至少 1 项子函数记录，不能经 Function_Free 释放。
 * @param[out] rootInstructions 至少 8 项的根指令数组，构造过程中清零并填充。
 * @param[out] childInstructions 测试栈上的至少 2 项子指令数组。
 * @param[out] rootConstants 至少 3 项的根常量数组，构造过程中清零并填充。
 * @param[out] childConstants 测试栈上的至少 1 项子常量数组。
 *
 * ## Constraints and lifecycle
 * 根/子函数及指令、常量均来自测试体栈数组并被借用，不可交给 Function_Free。VM 目标返回 41，native 指针为 73/91；本矩阵只交 writer，不执行函数。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:693
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1074
 */

/**
 * @unit AOTPIPE-043
 * @fn void test_known_call_quickening_recovers_vm_provenance_matrix(void)
 *
 * ## Summary
 * 用七行 VM 来源/别名/尾调用组合核对浅层 quickening 产出的 opcode。
 *
 * ## Constraints and lifecycle
 * 每行独立 VM 与 heap 数组，先 quicken 再只检查根 opcode；不执行这些行。正常路径 Function_Free 根后销毁 VM；计时不作 oracle。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:767
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1152
 */

/**
 * @unit AOTPIPE-044
 * @fn void test_known_call_quickening_recovers_native_provenance_matrix(void)
 *
 * ## Summary
 * 用六行 native 函数、闭包、指针来源核对 quickening 调用族。
 *
 * ## Constraints and lifecycle
 * native 行不提供 child 数组；只检查生成 opcode，不调用 native 回调。正常路径释放根挂接的 heap 数组并销毁 VM。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:846
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1153
 */

/**
 * @unit AOTPIPE-045
 * @fn void test_known_call_runtime_executes_manual_opcode_matrix(void)
 *
 * ## Summary
 * 解释执行八行直接已知 VM/native、尾调用与零参指令，核对 41/73/91。
 *
 * ## Constraints and lifecycle
 * 不是通过 quickening 生成 opcode；每行从手工 builder 建立根与可选叶子。正常路径分别 Function_Free 根/叶子并销毁 VM，执行 helper 的 TryRun 保护不包围整个测试体的断言。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:917
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1154
 */

/**
 * @unit AOTPIPE-046
 * @fn void test_typed_member_calls_quicken_to_known_vm_member_call_family(void)
 *
 * ## Summary
 * 从 Counter 源码核对 KNOWN_VM_MEMBER_CALL 及通用调用族缺席，再解释执行得到 14。
 *
 * ## Constraints and lifecycle
 * 检查根函数 opcode，不声称任意成员程序覆盖；Counter 两方法在这一个实际执行路径可达。正常路径释放编译函数并销毁 VM。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:967
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1155
 */

/**
 * @unit AOTPIPE-047
 * @fn void test_known_call_aot_backends_support_full_opcode_matrix(void)
 *
 * ## Summary
 * 对八行手工 opcode 生成 .zri 和严格 C/LLVM 文本，拒绝对应 unsupported 片段。
 *
 * ## Constraints and lifecycle
 * 全矩阵指当前八行，不等于所有指令或所有后端运行语义；栈借用数组不能 Function_Free。BUG: AOTPIPE-CLEANUP-01：.zri fopen 普通失败后 longjmp 跳过 State_Destroy。TODO: AOTPIPE-AOT-EXEC-01：没有编译/运行生成 C/LLVM。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1003
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1156
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c:1391
 * - tests/third_party/zr_unity/Unity/src/unity_internals.h:891
 * - tests/harness/runtime_support.c:403
 */

/**
 * @unit AOTPIPE-050
 * @fn int main(void)
 *
 * ## Summary
 * 按 Unity 固定顺序运行五个已知调用测试，分别覆盖 quickening、解释执行与后端文本。
 *
 * ## Returns
 * @return 返回 UNITY_END() 的失败计数。
 *
 * ## Constraints and lifecycle
 * 无 selector；7+6+8+8 是各测试内部数据行，不计为 Unity 用例数。两个 hook 为空，相对工件名要求同模块串行独占 cwd。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:1150
 */

/**
 * @unit AOTPIPE-L02
 * @brief 内嵌 Zr fixture::Counter；定义位于本文件字符串，未另建 C 实体。
 *
 * ## Summary
 * 为类型化成员 quickening 提供具有可变整数状态的 Counter 类。
 *
 * ## Fields
 * - `value: int`：同一 Counter 的可变整数状态。
 *
 * ## Value model
 * 显式字段 value:int；显式方法 step(delta:int):int 与 read():int。测试在 new Counter() 后先将 value 写为 3。
 *
 * ## Invariants
 * step(4) 把同一对象更新为 7，read() 读取 7，顶层相加为 14；不声称覆盖任意构造器、继承或成员形态。
 *
 * ## Constraints and lifecycle
 * 类定义和方法作为固定字符串被编译；typed-member 用例随后解释执行。VM 承载实例寿命，C 测试仅释放编译函数和 state，不交付实例引用。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:341
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:342
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:990
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:991
 */

/**
 * @unit AOTPIPE-L03
 * @brief 内嵌 Zr fixture::Counter.step；定义位于本文件字符串，未另建 C 实体。
 *
 * ## Summary
 * 在同一个 Counter 上累加 delta 并返回更新后的值。
 *
 * ## Parameters
 * @param[in,out] this 隐式 Counter 接收者；读取并更新同一 value 字段。
 * @param[in] delta 要加到 value 的 int，当前源码调用传 4。
 *
 * ## Returns
 * @return 返回更新后的 value:int；当前初始化为 3、传 4 时为 7。
 *
 * ## Constraints and lifecycle
 * 该语言方法在 typed-member 用例的解释执行中可达；不将 quickening 表的手工 opcode 当作对此方法的执行。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:343
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:344
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:990
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:991
 */

/**
 * @unit AOTPIPE-L04
 * @brief 内嵌 Zr fixture::Counter.read；定义位于本文件字符串，未另建 C 实体。
 *
 * ## Summary
 * 返回 Counter 当前的 value，作为前一次成员写入的观察点。
 *
 * ## Parameters
 * @param[in] this 隐式 Counter 接收者，只读取当前 value。
 *
 * ## Returns
 * @return 返回 value:int；当前 step(4) 后为 7。
 *
 * ## Constraints and lifecycle
 * 显式参数为空；当前测试通过对象成员调用并由顶层和式核对结果 14。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:347
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:348
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:990
 * - zr_vm_aot/tests/parser/test_known_call_pipeline.c:991
 */
