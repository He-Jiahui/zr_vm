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
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrKnownCallPipelineTimer;

typedef enum {
    ZR_KNOWN_CALL_PROVENANCE_GET_SUB_FUNCTION = 0,
    ZR_KNOWN_CALL_PROVENANCE_CREATE_CLOSURE = 1,
    ZR_KNOWN_CALL_PROVENANCE_FUNCTION_CONSTANT = 2,
    ZR_KNOWN_CALL_PROVENANCE_CLOSURE_CONSTANT = 3,
    ZR_KNOWN_CALL_PROVENANCE_NATIVE_FUNCTION_CONSTANT = 4,
    ZR_KNOWN_CALL_PROVENANCE_NATIVE_CLOSURE_CONSTANT = 5,
    ZR_KNOWN_CALL_PROVENANCE_NATIVE_POINTER_CONSTANT = 6
} EZrKnownCallQuickeningProvenanceKind;

typedef enum {
    ZR_KNOWN_CALL_ALIAS_NONE = 0,
    ZR_KNOWN_CALL_ALIAS_GET_STACK = 1,
    ZR_KNOWN_CALL_ALIAS_SET_STACK = 2
} EZrKnownCallAliasKind;

typedef struct {
    const char *name;
    EZrKnownCallQuickeningProvenanceKind provenance;
    EZrKnownCallAliasKind aliasKind;
    TZrBool isTailCall;
    TZrUInt16 argumentCount;
    EZrInstructionCode expectedOpcode;
} SZrKnownCallQuickeningCase;

typedef struct {
    const char *name;
    EZrInstructionCode opcode;
    TZrBool isNative;
    TZrBool isTailCall;
    TZrBool zeroArg;
    TZrInt64 expectedResult;
} SZrKnownCallRuntimeCase;

typedef struct {
    const char *name;
    const char *opcodeName;
    EZrInstructionCode opcode;
    TZrBool isNative;
    TZrBool isTailCall;
    TZrBool zeroArg;
} SZrKnownCallAotCase;

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

static TZrBool aot_c_text_contains_unsupported_opcode(const char *text, TZrUInt32 opcode) {
    const char *unsupportedPrefix = "return ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state,";
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

static TZrInstruction make_instruction_no_operands(EZrInstructionCode opcode, TZrUInt16 operandExtra) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    return instruction;
}

static TZrInstruction make_instruction_constant_operand(EZrInstructionCode opcode,
                                                        TZrUInt16 operandExtra,
                                                        TZrInt32 operandA2) {
    TZrInstruction instruction = make_instruction_no_operands(opcode, operandExtra);

    instruction.instruction.operand.operand2[0] = operandA2;
    return instruction;
}

static TZrInstruction make_instruction_slot_operands(EZrInstructionCode opcode,
                                                     TZrUInt16 operandExtra,
                                                     TZrUInt16 operandA1,
                                                     TZrUInt16 operandB1) {
    TZrInstruction instruction = make_instruction_no_operands(opcode, operandExtra);

    instruction.instruction.operand.operand1[0] = operandA1;
    instruction.instruction.operand.operand1[1] = operandB1;
    return instruction;
}

static TZrInstruction make_instruction_return(TZrUInt16 sourceSlot) {
    return make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, sourceSlot, 0);
}

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

static SZrFunction *compile_typed_member_known_call_fixture(SZrState *state) {
    const char *source =
            "class Counter {\n"
            "    pub var value: int;\n"
            "    pub step(delta: int): int {\n"
            "        this.value = this.value + delta;\n"
            "        return this.value;\n"
            "    }\n"
            "    pub read(): int {\n"
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

static void init_function_value(SZrState *state, SZrTypeValue *value, SZrFunction *function, TZrBool isNative) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    value->type = ZR_VALUE_TYPE_FUNCTION;
    value->isNative = isNative;
}

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

static void init_closure_value(SZrState *state, SZrTypeValue *value, SZrClosure *closure, TZrBool isNative) {
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(closure);

    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    value->type = ZR_VALUE_TYPE_CLOSURE;
    value->isNative = isNative;
}

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

static void build_manual_quickening_fixture(SZrState *state,
                                            const SZrKnownCallQuickeningCase *testCase,
                                            SZrFunction *rootFunction,
                                            SZrFunction *childFunctions,
                                            TZrInstruction *rootInstructions,
                                            TZrInstruction *childInstructions,
                                            SZrTypeValue *rootConstants,
                                            SZrTypeValue *childConstants,
                                            TZrUInt32 *outCallInstructionIndex) {
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
    TEST_ASSERT_NOT_NULL(outCallInstructionIndex);

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
    *outCallInstructionIndex = instructionCount;
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
        SZrFunction childFunctions[1];
        TZrInstruction rootInstructions[8];
        TZrInstruction childInstructions[2];
        SZrTypeValue rootConstants[3];
        SZrTypeValue childConstants[1];
        TZrUInt32 callInstructionIndex = 0;

        TEST_ASSERT_NOT_NULL(state);
        build_manual_quickening_fixture(state,
                                        &cases[index],
                                        &rootFunction,
                                        childFunctions,
                                        rootInstructions,
                                        childInstructions,
                                        rootConstants,
                                        childConstants,
                                        &callInstructionIndex);

        TEST_ASSERT_TRUE_MESSAGE(compiler_quicken_execbc_function_shallow(state, &rootFunction), cases[index].name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(cases[index].expectedOpcode,
                                         (EZrInstructionCode)rootFunction.instructionsList[callInstructionIndex]
                                                 .instruction.operationCode,
                                         cases[index].name);

        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

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
        TZrInstruction rootInstructions[8];
        SZrTypeValue rootConstants[3];
        TZrUInt32 callInstructionIndex = 0;

        TEST_ASSERT_NOT_NULL(state);
        build_manual_quickening_fixture(state,
                                        &cases[index],
                                        &rootFunction,
                                        ZR_NULL,
                                        rootInstructions,
                                        ZR_NULL,
                                        rootConstants,
                                        ZR_NULL,
                                        &callInstructionIndex);

        TEST_ASSERT_TRUE_MESSAGE(compiler_quicken_execbc_function_shallow(state, &rootFunction), cases[index].name);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(cases[index].expectedOpcode,
                                         (EZrInstructionCode)rootFunction.instructionsList[callInstructionIndex]
                                                 .instruction.operationCode,
                                         cases[index].name);

        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

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

static void test_typed_member_calls_quicken_to_known_vm_call_family(void) {
    SZrKnownCallPipelineTimer timer = {0};
    const char *testSummary = "Typed Member Calls Quicken To Known VM Call Family";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("typed member known-call quickening",
                 "Testing that typed instance method calls stop staying on generic FUNCTION_CALL and instead lower into GET_MEMBER_SLOT plus the KNOWN_VM_CALL family, including zero-arg variants.");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_typed_member_known_call_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)));
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
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(cText, "ZrLibrary_AotRuntime_PrepareDirectCall"), cases[index].name);
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(llvmText, "@ZrLibrary_AotRuntime_PrepareDirectCall"),
                                         cases[index].name);
        } else {
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(cText, "ZrLibrary_AotRuntime_PrepareStaticDirectCall"),
                                         cases[index].name);
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(llvmText, "@ZrLibrary_AotRuntime_PrepareStaticDirectCall"),
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_known_call_quickening_recovers_vm_provenance_matrix);
    RUN_TEST(test_known_call_quickening_recovers_native_provenance_matrix);
    RUN_TEST(test_known_call_runtime_executes_manual_opcode_matrix);
    RUN_TEST(test_typed_member_calls_quicken_to_known_vm_call_family);
    RUN_TEST(test_known_call_aot_backends_support_full_opcode_matrix);
    return UNITY_END();
}
