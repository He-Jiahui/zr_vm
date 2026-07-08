#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_UNIX)
#include <dlfcn.h>
#endif

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/writer.h"

#ifndef ZR_VM_TESTS_C_COMPILER
#define ZR_VM_TESTS_C_COMPILER "cc"
#endif

#ifndef ZR_VM_TESTS_REPO_ROOT
#define ZR_VM_TESTS_REPO_ROOT "."
#endif

#ifndef ZR_VM_TESTS_BUILD_LIB_DIR
#define ZR_VM_TESTS_BUILD_LIB_DIR "lib"
#endif

void setUp(void) {}

void tearDown(void) {}

#if defined(ZR_PLATFORM_UNIX)
static int run_command_expect_success(const char *command) {
    int result;

    TEST_ASSERT_NOT_NULL(command);
    result = system(command);
    if (result != 0) {
        printf("Command failed with status %d:\n%s\n", result, command);
    }
    return result;
}

static void write_text_file_or_fail(const TZrChar *path, const char *text) {
    FILE *file;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));

    file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static char *read_text_file_owned_or_fail(const TZrChar *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    TEST_ASSERT_NOT_NULL(path);
    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_END));
    fileSize = ftell(file);
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(0, fileSize);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_SET));

    buffer = (char *)malloc((size_t)fileSize + 1u);
    TEST_ASSERT_NOT_NULL(buffer);
    if (fileSize > 0) {
        TEST_ASSERT_EQUAL_size_t((size_t)fileSize, fread(buffer, 1, (size_t)fileSize, file));
    }
    buffer[fileSize] = '\0';
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    return buffer;
}

static void assert_reference_local_root_frame_for_slot2(const char *generatedCText) {
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static const SZrAotGcRootSlot zr_aot_ref_root_slots_0[] = {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, ".stackSlot = 2u,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                ".frameByteOffset = (TZrUInt32)offsetof(SZrAotReferenceLocals_0, o2),"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, ".typeLayoutId = 0u,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, ".fieldByteOffset = 0u,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                ".locationKind = (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "static const SZrAotGcRootMap zr_aot_ref_root_map_0 = {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_ref_root_slots_0,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "SZrAotGcRootFrame zr_aot_ref_gc_root_frame;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "TZrBool zr_aot_has_ref_gc_root_frame = ZR_FALSE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "/* zr_aot_reference_local_root_frame_push */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "&zr_aot_ref_gc_root_frame,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "(TZrStackValuePointer)(void *)&zr_aot_ref_locals,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "&zr_aot_ref_root_map_0"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Gc_AotRootFramePop(state, &zr_aot_ref_gc_root_frame);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_has_ref_gc_root_frame = ZR_FALSE;"));
    TEST_ASSERT_NULL(strstr(generatedCText, ".gcRootMap = &zr_aot_ref_root_map_0"));
}

static void assert_text_contains_after(const char *text, const char *firstNeedle, const char *secondNeedle) {
    const char *first;
    const char *second;

    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(firstNeedle);
    TEST_ASSERT_NOT_NULL(secondNeedle);
    first = strstr(text, firstNeedle);
    second = first != ZR_NULL ? strstr(first + strlen(firstNeedle), secondNeedle) : ZR_NULL;
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
}

static TZrInstruction create_get_constant_instruction(TZrUInt16 destinationSlot, TZrInt32 constantIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_CONSTANT);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand2[0] = constantIndex;
    return instruction;
}

static TZrInstruction create_get_global_instruction(TZrUInt16 destinationSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_GLOBAL);
    instruction.instruction.operandExtra = destinationSlot;
    return instruction;
}

static TZrInstruction create_reset_stack_null_instruction(TZrUInt16 destinationSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(RESET_STACK_NULL);
    instruction.instruction.operandExtra = destinationSlot;
    return instruction;
}

static TZrInstruction create_to_string_instruction(TZrUInt16 destinationSlot, TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(TO_STRING);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    return instruction;
}

static TZrInstruction create_to_object_instruction(TZrUInt16 destinationSlot,
                                                   TZrUInt16 sourceSlot,
                                                   TZrUInt16 typeNameConstantIndex) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(TO_OBJECT);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    instruction.instruction.operand.operand1[1] = typeNameConstantIndex;
    return instruction;
}

static TZrInstruction create_generic_logical_not_instruction(TZrUInt16 destinationSlot, TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(LOGICAL_NOT);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    return instruction;
}

static TZrInstruction create_stack_copy_instruction(TZrUInt16 destinationSlot, TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SET_STACK);
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand2[0] = (TZrInt32)sourceSlot;
    return instruction;
}

static TZrInstruction create_jump_if_bool_false_instruction(TZrUInt16 conditionSlot, TZrInt32 offset) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE);
    instruction.instruction.operandExtra = conditionSlot;
    instruction.instruction.operand.operand2[0] = offset;
    return instruction;
}

static TZrInstruction create_return_instruction(TZrUInt16 returnCount, TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instruction.instruction.operandExtra = returnCount;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    return instruction;
}

static void init_string_constant(SZrState *state, SZrTypeValue *value, const char *text) {
    SZrString *string;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(text);

    string = ZrCore_String_CreateFromNative(state, text);
    TEST_ASSERT_NOT_NULL(string);
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(string));
    value->type = ZR_VALUE_TYPE_STRING;
}

static SZrFunction *create_generic_logical_not_numeric_source_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 15u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_constant_instruction(0u, 0);
    function->instructionsList[1] = create_generic_logical_not_instruction(1u, 0u);
    function->instructionsList[2] = create_jump_if_bool_false_instruction(1u, 10);
    function->instructionsList[3] = create_get_constant_instruction(2u, 1);
    function->instructionsList[4] = create_generic_logical_not_instruction(3u, 2u);
    function->instructionsList[5] = create_jump_if_bool_false_instruction(3u, 2);
    function->instructionsList[6] = create_get_constant_instruction(6u, 4);
    function->instructionsList[7] = create_return_instruction(1u, 6u);
    function->instructionsList[8] = create_get_constant_instruction(4u, 2);
    function->instructionsList[9] = create_generic_logical_not_instruction(5u, 4u);
    function->instructionsList[10] = create_jump_if_bool_false_instruction(5u, 2);
    function->instructionsList[11] = create_get_constant_instruction(6u, 3);
    function->instructionsList[12] = create_return_instruction(1u, 6u);
    function->instructionsList[13] = create_get_constant_instruction(6u, 4);
    function->instructionsList[14] = create_return_instruction(1u, 6u);
    function->instructionsLength = 15u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 5u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    function->constantValueLength = 5u;
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 0);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[1], 7u);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[2], 0.0);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[3], 17);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[4], 91);

    function->stackSize = 7u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_logical_not_reset_null_source_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 11u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_reset_stack_null_instruction(0u);
    function->instructionsList[1] = create_generic_logical_not_instruction(1u, 0u);
    function->instructionsList[2] = create_jump_if_bool_false_instruction(1u, 6);
    function->instructionsList[3] = create_reset_stack_null_instruction(3u);
    function->instructionsList[4] = create_stack_copy_instruction(4u, 3u);
    function->instructionsList[5] = create_generic_logical_not_instruction(5u, 4u);
    function->instructionsList[6] = create_jump_if_bool_false_instruction(5u, 2);
    function->instructionsList[7] = create_get_constant_instruction(2u, 0);
    function->instructionsList[8] = create_return_instruction(1u, 2u);
    function->instructionsList[9] = create_get_constant_instruction(2u, 1);
    function->instructionsList[10] = create_return_instruction(1u, 2u);
    function->instructionsLength = 11u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    function->constantValueLength = 2u;
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 17);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 91);

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_logical_not_null_constant_local_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 11u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_constant_instruction(0u, 0);
    function->instructionsList[1] = create_generic_logical_not_instruction(1u, 0u);
    function->instructionsList[2] = create_jump_if_bool_false_instruction(1u, 6);
    function->instructionsList[3] = create_get_constant_instruction(3u, 0);
    function->instructionsList[4] = create_stack_copy_instruction(4u, 3u);
    function->instructionsList[5] = create_generic_logical_not_instruction(5u, 4u);
    function->instructionsList[6] = create_jump_if_bool_false_instruction(5u, 2);
    function->instructionsList[7] = create_get_constant_instruction(2u, 1);
    function->instructionsList[8] = create_return_instruction(1u, 2u);
    function->instructionsList[9] = create_get_constant_instruction(2u, 2);
    function->instructionsList[10] = create_return_instruction(1u, 2u);
    function->instructionsLength = 11u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    function->constantValueLength = 3u;
    ZrCore_Value_ResetAsNull(&function->constantValueList[0]);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 17);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[2], 91);

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_logical_not_bool_constant_local_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_constant_instruction(0u, 0);
    function->instructionsList[1] = create_generic_logical_not_instruction(1u, 0u);
    function->instructionsList[2] = create_jump_if_bool_false_instruction(1u, 2);
    function->instructionsList[3] = create_get_constant_instruction(2u, 1);
    function->instructionsList[4] = create_return_instruction(1u, 2u);
    function->instructionsList[5] = create_get_constant_instruction(2u, 2);
    function->instructionsList[6] = create_return_instruction(1u, 2u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    function->constantValueLength = 3u;
    ZrCore_Value_InitAsBool(state, &function->constantValueList[0], ZR_TRUE);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 91);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[2], 17);

    function->stackSize = 3u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_logical_not_bool_stack_copy_source_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 8u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_constant_instruction(0u, 0);
    function->instructionsList[1] = create_stack_copy_instruction(5u, 0u);
    function->instructionsList[2] = create_generic_logical_not_instruction(1u, 5u);
    function->instructionsList[3] = create_jump_if_bool_false_instruction(1u, 2);
    function->instructionsList[4] = create_get_constant_instruction(2u, 1);
    function->instructionsList[5] = create_return_instruction(1u, 2u);
    function->instructionsList[6] = create_get_constant_instruction(2u, 2);
    function->instructionsList[7] = create_return_instruction(1u, 2u);
    function->instructionsLength = 8u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    function->constantValueLength = 3u;
    ZrCore_Value_InitAsBool(state, &function->constantValueList[0], ZR_TRUE);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 91);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[2], 17);

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_logical_not_string_constant_local_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 22u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_constant_instruction(0u, 0);
    function->instructionsList[1] = create_generic_logical_not_instruction(1u, 0u);
    function->instructionsList[2] = create_jump_if_bool_false_instruction(1u, 2);
    function->instructionsList[3] = create_get_constant_instruction(4u, 1);
    function->instructionsList[4] = create_return_instruction(1u, 4u);
    function->instructionsList[5] = create_get_constant_instruction(2u, 2);
    function->instructionsList[6] = create_generic_logical_not_instruction(3u, 2u);
    function->instructionsList[7] = create_jump_if_bool_false_instruction(3u, 2);
    function->instructionsList[8] = create_get_constant_instruction(4u, 0);
    function->instructionsList[9] = create_stack_copy_instruction(5u, 4u);
    function->instructionsList[10] = create_generic_logical_not_instruction(6u, 5u);
    function->instructionsList[11] = create_jump_if_bool_false_instruction(6u, 2);
    function->instructionsList[12] = create_get_constant_instruction(4u, 1);
    function->instructionsList[13] = create_return_instruction(1u, 4u);
    function->instructionsList[14] = create_get_constant_instruction(7u, 2);
    function->instructionsList[15] = create_stack_copy_instruction(8u, 7u);
    function->instructionsList[16] = create_generic_logical_not_instruction(9u, 8u);
    function->instructionsList[17] = create_jump_if_bool_false_instruction(9u, 2);
    function->instructionsList[18] = create_get_constant_instruction(4u, 3);
    function->instructionsList[19] = create_return_instruction(1u, 4u);
    function->instructionsList[20] = create_get_constant_instruction(4u, 1);
    function->instructionsList[21] = create_return_instruction(1u, 4u);
    function->instructionsLength = 22u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 4u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    function->constantValueLength = 4u;
    init_string_constant(state, &function->constantValueList[0], "zr");
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 91);
    init_string_constant(state, &function->constantValueList[2], "");
    ZrCore_Value_InitAsInt(state, &function->constantValueList[3], 17);

    function->stackSize = 10u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_logical_not_dynamic_string_slot_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 8u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_constant_instruction(0u, 0);
    function->instructionsList[1] = create_to_string_instruction(2u, 0u);
    function->instructionsList[2] = create_generic_logical_not_instruction(1u, 2u);
    function->instructionsList[3] = create_jump_if_bool_false_instruction(1u, 2);
    function->instructionsList[4] = create_get_constant_instruction(3u, 1);
    function->instructionsList[5] = create_return_instruction(1u, 3u);
    function->instructionsList[6] = create_get_constant_instruction(3u, 2);
    function->instructionsList[7] = create_return_instruction(1u, 3u);
    function->instructionsLength = 8u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    function->constantValueLength = 3u;
    init_string_constant(state, &function->constantValueList[0], "");
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 17);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[2], 91);

    function->stackSize = 4u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_logical_not_dynamic_object_slot_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 8u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_get_global_instruction(0u);
    function->instructionsList[1] = create_to_object_instruction(2u, 0u, 0u);
    function->instructionsList[2] = create_generic_logical_not_instruction(1u, 2u);
    function->instructionsList[3] = create_jump_if_bool_false_instruction(1u, 2);
    function->instructionsList[4] = create_get_constant_instruction(3u, 1);
    function->instructionsList[5] = create_return_instruction(1u, 3u);
    function->instructionsList[6] = create_get_constant_instruction(3u, 2);
    function->instructionsList[7] = create_return_instruction(1u, 3u);
    function->instructionsLength = 8u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    function->constantValueLength = 3u;
    init_string_constant(state, &function->constantValueList[0], "AotMissingType");
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 17);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[2], 91);

    function->stackSize = 4u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static void hash_file_or_fail(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, bufferSize);

    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    while ((readSize = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        TZrSize index;
        for (index = 0; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
}
#endif

static void test_aot_c_generated_shared_library_executes_generic_logical_not_numeric_source_local_branch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic LOGICAL_NOT numeric-source shared-library smoke validates the Unix dlopen toolchain path");
#else
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-generic-logical-not-numeric-source-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_logical_not_numeric_source_function(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_numeric_source_project",
                                                       "runtime_generic_logical_not_numeric_source_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_numeric_source_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_numeric_source_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_numeric_source_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_numeric_source_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, "return 17;\n");

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_i64_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b1 = (TZrBool)(zr_aot_s0 == (TZrInt64)0);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_u64_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b3 = (TZrBool)(zr_aot_u2 == (TZrUInt64)0u);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_f64_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b5 = (TZrBool)(zr_aot_f4 == (TZrFloat64)0.0);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_jump_if_bool_false_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b1) {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b3) {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b5) {"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 3, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 5, 4)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 1"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 3"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 5"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(17, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C, ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_generic_logical_not_reset_null_source_local_bool_branch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic LOGICAL_NOT reset-null-source local shared-library smoke validates the Unix dlopen toolchain path");
#else
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-generic-logical-not-reset-null-source-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_logical_not_reset_null_source_function(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_reset_null_source_project",
                                                       "runtime_generic_logical_not_reset_null_source_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_reset_null_source_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_reset_null_source_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_reset_null_source_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_reset_null_source_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, "return 17;\n");

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_reset_stack_null_local_logical_not_skip slot=0"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_reset_null_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b1 = ZR_TRUE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_jump_if_bool_false_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b1) {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_reset_null_stack_copy_local_logical_not_reset_skip slot=3"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_reset_null_stack_copy_local_logical_not_source_skip dstSlot=4 srcSlot=3"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_reset_null_stack_copy_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b5 = ZR_TRUE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b5) {"));
    TEST_ASSERT_NULL(strstr(generatedCText, "const SZrTypeValue *zr_aot_condition = ZR_NULL;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_condition = &frame.slotBase[1].value;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_condition_bool"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ResetStackNull(state, &frame, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 1"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ResetStackNull(state, &frame, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 5, 4)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 5"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[0].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[3].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[4].value"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(17, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C, ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_generic_logical_not_null_constant_local_branch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic LOGICAL_NOT null-constant shared-library smoke validates the Unix dlopen toolchain path");
#else
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-generic-logical-not-null-constant-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_logical_not_null_constant_local_function(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_null_constant_project",
                                                       "runtime_generic_logical_not_null_constant_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_null_constant_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_null_constant_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_null_constant_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_null_constant_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, "return 17;\n");

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_null_constant_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b1 = ZR_TRUE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_jump_if_bool_false_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b1) {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_null_constant_stack_copy_local_logical_not_constant_skip slot=3"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_null_constant_stack_copy_local_logical_not_source_skip dstSlot=4 srcSlot=3"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_null_stack_copy_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b5 = ZR_TRUE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b5) {"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 5, 4)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 1"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 5"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyConstant(state, &frame, 3"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[0].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[3].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[4].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Value_ResetAsNull(zr_aot_destination);"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(17, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C, ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_generic_logical_not_bool_constant_local_branch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic LOGICAL_NOT bool-constant shared-library smoke validates the Unix dlopen toolchain path");
#else
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-generic-logical-not-bool-constant-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_logical_not_bool_constant_local_function(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_bool_constant_project",
                                                       "runtime_generic_logical_not_bool_constant_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_bool_constant_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_bool_constant_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_bool_constant_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_bool_constant_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, "return 17;\n");

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_bool_constant_local_logical_not_source_skip"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_bool_constant_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b1 = ZR_FALSE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_jump_if_bool_false_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b1) {"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_bool_local"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 1"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[0].value"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(17, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C, ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_generic_logical_not_string_constant_local_branch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic LOGICAL_NOT string-constant shared-library smoke validates the Unix dlopen toolchain path");
#else
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-generic-logical-not-string-constant-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_logical_not_string_constant_local_function(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_string_constant_project",
                                                       "runtime_generic_logical_not_string_constant_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_string_constant_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_string_constant_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_string_constant_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_string_constant_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, "return 17;\n");

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_string_constant_local_logical_not_source_skip slot=0"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_string_constant_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b1 = ZR_FALSE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_string_constant_local_logical_not_source_skip slot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b3 = ZR_TRUE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_string_constant_stack_copy_local_logical_not_constant_skip slot=4"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_string_constant_stack_copy_local_logical_not_source_skip dstSlot=5 srcSlot=4"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_generic_logical_not_string_stack_copy_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b6 = ZR_FALSE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_string_constant_stack_copy_local_logical_not_constant_skip slot=7"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_string_constant_stack_copy_local_logical_not_source_skip dstSlot=8 srcSlot=7"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b9 = ZR_TRUE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_jump_if_bool_false_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b1) {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b3) {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b6) {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b9) {"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 3, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 6, 5)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 9, 8)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 1"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 3"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 6"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 9"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyConstant(state, &frame, 0"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyConstant(state, &frame, 2"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyConstant(state, &frame, 4"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyConstant(state, &frame, 7"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 5, 4"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 8, 7"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(17, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C, ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_generic_logical_not_dynamic_string_slot_branch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic LOGICAL_NOT dynamic string-slot shared-library smoke validates the Unix dlopen toolchain path");
#else
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-generic-logical-not-dynamic-string-slot-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_logical_not_dynamic_string_slot_function(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_dynamic_string_slot_project",
                                                       "runtime_generic_logical_not_dynamic_string_slot_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_dynamic_string_slot_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_dynamic_string_slot_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_dynamic_string_slot_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_dynamic_string_slot_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, "return 17;\n");

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_to_string"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ToString(state, &frame, 2, 0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_string_slot_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "typedef struct SZrAotReferenceLocals_0 {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "SZrRawObject *o2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "} SZrAotReferenceLocals_0;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "SZrAotReferenceLocals_0 zr_aot_ref_locals = { ZR_NULL };"));
    assert_reference_local_root_frame_for_slot2(generatedCText);
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrRawObject *zr_aot_o2 = ZR_NULL;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "const SZrTypeValue *zr_aot_to_string_value = ZrCore_Stack_GetValue(frame.slotBase + 2);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_ref_locals.o2 = zr_aot_to_string_value->value.object;"));
    assert_text_contains_after(generatedCText,
                               "zr_aot_ref_locals.o2 = zr_aot_to_string_value->value.object;",
                               "/* zr_aot_gc_safepoint_reference_local */");
    assert_text_contains_after(generatedCText,
                               "/* zr_aot_gc_safepoint_reference_local */",
                               "const SZrString *zr_aot_string_slot_string = ZR_CAST_STRING(state, zr_aot_ref_locals.o2);");
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "const SZrString *zr_aot_string_slot_string = ZR_CAST_STRING(state, zr_aot_ref_locals.o2);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "ZrCore_String_GetByteLength(zr_aot_string_slot_string) > 0u"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "const SZrTypeValue *zr_aot_string_slot_value = ZrCore_Stack_GetValue(frame.slotBase + 2);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrRawObject *zr_aot_string_slot_object"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_CAST_STRING(state, zr_aot_string_slot_value->value.object)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b1 = (TZrBool)(!zr_aot_string_slot_truthy);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b1) {"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 1, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 1"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);
    state->global->garbageCollector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    state->global->garbageCollector->gcDebtSize = 4096;
    state->global->garbageCollector->gcLastStepWork = 0;

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_GREATER_THAN_UINT64(0u, state->global->garbageCollector->gcLastStepWork);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(17, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C, ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_generic_logical_not_dynamic_object_slot_branch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic LOGICAL_NOT dynamic object-slot shared-library smoke validates the Unix dlopen toolchain path");
#else
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-generic-logical-not-dynamic-object-slot-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_logical_not_dynamic_object_slot_function(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_dynamic_object_slot_project",
                                                       "runtime_generic_logical_not_dynamic_object_slot_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_dynamic_object_slot_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_dynamic_object_slot_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_dynamic_object_slot_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_dynamic_object_slot_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, "return 17;\n");

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_get_global"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GetGlobal(state, &frame, 0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_value_exec_to_object"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_ToObject(state, &frame, 2, 0, 0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_object_slot_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "typedef struct SZrAotReferenceLocals_0 {"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "SZrRawObject *o2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "} SZrAotReferenceLocals_0;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "SZrAotReferenceLocals_0 zr_aot_ref_locals = { ZR_NULL };"));
    assert_reference_local_root_frame_for_slot2(generatedCText);
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrRawObject *zr_aot_o2 = ZR_NULL;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "const SZrTypeValue *zr_aot_to_object_value = ZrCore_Stack_GetValue(frame.slotBase + 2);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZR_VALUE_IS_TYPE_NULL(zr_aot_to_object_value->type)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZR_VALUE_IS_TYPE_OBJECT(zr_aot_to_object_value->type)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "zr_aot_ref_locals.o2 = zr_aot_to_object_value->value.object;"));
    assert_text_contains_after(generatedCText,
                               "zr_aot_ref_locals.o2 = zr_aot_to_object_value->value.object;",
                               "/* zr_aot_gc_safepoint_reference_local */");
    assert_text_contains_after(generatedCText,
                               "/* zr_aot_gc_safepoint_reference_local */",
                               "TZrBool zr_aot_object_slot_truthy = (TZrBool)(zr_aot_ref_locals.o2 != ZR_NULL);");
    TEST_ASSERT_NOT_NULL(strstr(generatedCText,
                                "TZrBool zr_aot_object_slot_truthy = (TZrBool)(zr_aot_ref_locals.o2 != ZR_NULL);"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "const SZrTypeValue *zr_aot_object_slot_value = ZrCore_Stack_GetValue(frame.slotBase + 2);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "SZrRawObject *zr_aot_object_slot_object = ZR_NULL;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_object_slot_object = zr_aot_object_slot_value->value.object;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_object_slot_truthy = ZR_TRUE;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b1 = (TZrBool)(!zr_aot_object_slot_truthy);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b1) {"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 1, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 1"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));
    TEST_ASSERT_NOT_NULL(state->global->garbageCollector);
    state->global->garbageCollector->gcMode = ZR_GARBAGE_COLLECT_MODE_GENERATIONAL;
    state->global->garbageCollector->gcDebtSize = 4096;
    state->global->garbageCollector->gcLastStepWork = 0;

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_GREATER_THAN_UINT64(0u, state->global->garbageCollector->gcLastStepWork);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(91, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C, ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_executes_generic_logical_not_bool_stack_copy_source_local_branch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic LOGICAL_NOT bool stack-copy source shared-library smoke validates the Unix dlopen toolchain path");
#else
    const char *projectJson =
            "{"
            "\"name\":\"aot-runtime-generic-logical-not-bool-stack-copy-source-smoke\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_logical_not_bool_stack_copy_source_function(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_bool_stack_copy_source_project",
                                                       "runtime_generic_logical_not_bool_stack_copy_source_smoke",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_bool_stack_copy_source_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_bool_stack_copy_source_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_bool_stack_copy_source_project/bin/aot_c/src",
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_shared_library",
                                                       "generic_logical_not_bool_stack_copy_source_project/bin/aot_c/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, "return 17;\n");

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_bool dstSlot=5 srcSlot=0"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b5 = (TZrBool)(zr_aot_b0 != 0u);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_logical_not_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_b1 = (TZrBool)(!zr_aot_b5);"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_jump_if_bool_false_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (!zr_aot_b1) {"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_CopyStack(state, &frame, 5, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText,
                            "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot(state, &frame, 1, 5)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, 1"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core "
             "-o \"%s\"",
             ZR_VM_TESTS_C_COMPILER,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             ZR_VM_TESTS_REPO_ROOT,
             generatedCPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_C, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(17, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_C, ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_executes_generic_logical_not_numeric_source_local_branch);
    RUN_TEST(test_aot_c_generated_shared_library_executes_generic_logical_not_reset_null_source_local_bool_branch);
    RUN_TEST(test_aot_c_generated_shared_library_executes_generic_logical_not_null_constant_local_branch);
    RUN_TEST(test_aot_c_generated_shared_library_executes_generic_logical_not_bool_constant_local_branch);
    RUN_TEST(test_aot_c_generated_shared_library_executes_generic_logical_not_string_constant_local_branch);
    RUN_TEST(test_aot_c_generated_shared_library_executes_generic_logical_not_dynamic_string_slot_branch);
    RUN_TEST(test_aot_c_generated_shared_library_executes_generic_logical_not_dynamic_object_slot_branch);
    RUN_TEST(test_aot_c_generated_shared_library_executes_generic_logical_not_bool_stack_copy_source_local_branch);
    return UNITY_END();
}
