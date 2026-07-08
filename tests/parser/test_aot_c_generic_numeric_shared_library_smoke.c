#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_UNIX)
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/writer.h"
#endif

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

static TZrInstruction create_instruction_1(EZrInstructionCode opcode, TZrUInt16 destinationSlot, TZrInt32 operand) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand2[0] = operand;
    return instruction;
}

static TZrInstruction create_instruction_2(EZrInstructionCode opcode,
                                           TZrUInt16 destinationSlot,
                                           TZrUInt16 leftSlot,
                                           TZrUInt16 rightSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = leftSlot;
    instruction.instruction.operand.operand1[1] = rightSlot;
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

static SZrFunction *create_generic_numeric_binary_float_function(SZrState *state, EZrInstructionCode opcode) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 4u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(opcode, 2u, 0u, 1u);
    function->instructionsList[3] = create_return_instruction(1u, 2u);
    function->instructionsLength = 4u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[0], 7.5);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.0);
    function->constantValueLength = 2u;

    function->stackSize = 3u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_binary_signed_int_function(SZrState *state, EZrInstructionCode opcode) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 4u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(opcode, 2u, 0u, 1u);
    function->instructionsList[3] = create_return_instruction(1u, 2u);
    function->instructionsLength = 4u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 21);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 8);
    function->constantValueLength = 2u;

    function->stackSize = 3u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_binary_unsigned_int_function(SZrState *state, EZrInstructionCode opcode) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 4u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(opcode, 2u, 0u, 1u);
    function->instructionsList[3] = create_return_instruction(1u, 2u);
    function->instructionsLength = 4u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[0], (TZrUInt64)21u);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[1], (TZrUInt64)8u);
    function->constantValueLength = 2u;

    function->stackSize = 3u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_binary_signed_unsigned_int_function(SZrState *state,
                                                                               EZrInstructionCode opcode) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 4u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(opcode, 2u, 0u, 1u);
    function->instructionsList[3] = create_return_instruction(1u, 2u);
    function->instructionsLength = 4u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 21);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[1], (TZrUInt64)8u);
    function->constantValueLength = 2u;

    function->stackSize = 3u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_binary_signed_int_float_function(SZrState *state,
                                                                            EZrInstructionCode opcode) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 4u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(opcode, 2u, 0u, 1u);
    function->instructionsList[3] = create_return_instruction(1u, 2u);
    function->instructionsLength = 4u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 21);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.5);
    function->constantValueLength = 2u;

    function->stackSize = 3u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_binary_unsigned_int_float_function(SZrState *state,
                                                                              EZrInstructionCode opcode) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 4u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(opcode, 2u, 0u, 1u);
    function->instructionsList[3] = create_return_instruction(1u, 2u);
    function->instructionsLength = 4u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[0], (TZrUInt64)21u);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.5);
    function->constantValueLength = 2u;

    function->stackSize = 3u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_neg_float_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(NEG), 1u, 0u, 0u);
    function->instructionsList[2] = create_return_instruction(1u, 1u);
    function->instructionsLength = 3u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[0], 7.5);
    function->constantValueLength = 1u;

    function->stackSize = 2u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_neg_signed_int_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(NEG), 1u, 0u, 0u);
    function->instructionsList[2] = create_return_instruction(1u, 1u);
    function->instructionsLength = 3u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 7);
    function->constantValueLength = 1u;

    function->stackSize = 2u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_neg_unsigned_int_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_2(ZR_INSTRUCTION_ENUM(NEG), 1u, 0u, 0u);
    function->instructionsList[2] = create_return_instruction(1u, 1u);
    function->instructionsLength = 3u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[0], (TZrUInt64)7u);
    function->constantValueLength = 1u;

    function->stackSize = 2u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_left_float_function(SZrState *state,
                                                                          EZrInstructionCode opcode) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 5u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 3u, 0);
    function->instructionsList[2] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[3] = create_instruction_2(opcode, 2u, 3u, 1u);
    function->instructionsList[4] = create_return_instruction(1u, 2u);
    function->instructionsLength = 5u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[0], 7.5);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.0);
    function->constantValueLength = 2u;

    function->stackSize = 4u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_float_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(DIV), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 5u, 4u, 3u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[0], 7.5);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.0);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[2], 1.5);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_div_float_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(DIV), 5u, 4u, 3u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[0], 7.5);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.0);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[2], 1.5);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_right_div_float_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(DIV), 5u, 3u, 4u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[0], 7.5);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.0);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[2], 19.0);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_guarded_float_function(SZrState *state,
                                                                                   EZrInstructionCode opcode,
                                                                                   TZrBool copiedResultIsRightOperand) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = copiedResultIsRightOperand
                                            ? create_instruction_2(opcode, 5u, 3u, 4u)
                                            : create_instruction_2(opcode, 5u, 4u, 3u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[0], 7.5);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.0);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[2], 19.0);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_right_float_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(DIV), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(SUB), 5u, 3u, 4u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[0], 7.5);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.0);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[2], 10.0);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_chained_stack_copy_result_float_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 8u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(DIV), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 6u, 4);
    function->instructionsList[5] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[6] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL), 7u, 6u, 3u);
    function->instructionsList[7] = create_return_instruction(1u, 7u);
    function->instructionsLength = 8u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[0], 7.5);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.0);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[2], 1.5);
    function->constantValueLength = 3u;

    function->stackSize = 8u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_signed_int_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL), 5u, 4u, 3u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 21);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 8);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[2], 3);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_mod_signed_int_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(MOD), 5u, 4u, 3u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 21);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 8);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[2], 3);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_right_mod_signed_int_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(MOD), 5u, 3u, 4u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 21);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 8);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[2], 64);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_guarded_signed_int_function(
        SZrState *state,
        EZrInstructionCode opcode,
        TZrBool copiedResultIsRightOperand) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = copiedResultIsRightOperand
                                            ? create_instruction_2(opcode, 5u, 3u, 4u)
                                            : create_instruction_2(opcode, 5u, 4u, 3u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 21);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], 8);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[2], 64);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_unsigned_int_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL), 5u, 4u, 3u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[0], (TZrUInt64)21u);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[1], (TZrUInt64)8u);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[2], (TZrUInt64)3u);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_signed_unsigned_int_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL), 5u, 4u, 3u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 21);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[1], (TZrUInt64)8u);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[2], (TZrUInt64)3u);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_signed_float_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL), 5u, 4u, 3u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 21);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.5);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[2], 1.5);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static SZrFunction *create_generic_numeric_stack_copy_result_unsigned_float_function(SZrState *state) {
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);

    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction) * 7u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    function->instructionsList[1] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    function->instructionsList[2] = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD), 2u, 0u, 1u);
    function->instructionsList[3] = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), 4u, 2);
    function->instructionsList[4] = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3u, 2);
    function->instructionsList[5] = create_instruction_2(ZR_INSTRUCTION_ENUM(MUL), 5u, 4u, 3u);
    function->instructionsList[6] = create_return_instruction(1u, 5u);
    function->instructionsLength = 7u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrTypeValue) * 3u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[0], (TZrUInt64)21u);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[1], 2.5);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[2], 1.5);
    function->constantValueLength = 3u;

    function->stackSize = 6u;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
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
#endif

#if defined(ZR_PLATFORM_UNIX)
static void assert_generic_numeric_binary_float_local(EZrInstructionCode opcode,
                                                      const char *operationName,
                                                      const char *localMarker,
                                                      const char *localExpression,
                                                      const char *forbiddenRuntimeCall) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char moduleName[96];
    char hashText[96];
    char libraryName[128];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(operationName);
    TEST_ASSERT_NOT_NULL(localMarker);
    TEST_ASSERT_NOT_NULL(localExpression);
    TEST_ASSERT_NOT_NULL(forbiddenRuntimeCall);
    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_float_function(state, opcode);
    TEST_ASSERT_NOT_NULL(function);

    snprintf(moduleName, sizeof(moduleName), "aot_c_generic_numeric_%s_smoke", operationName);
    snprintf(hashText, sizeof(hashText), "generic-numeric-%s-smoke", operationName);
    snprintf(libraryName, sizeof(libraryName), "libaot_c_generic_numeric_%s_smoke", operationName);

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.sourceHash = hashText;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = hashText;
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       moduleName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       libraryName,
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, localMarker));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, localExpression));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, forbiddenRuntimeCall));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}
#endif

static void test_aot_c_generated_shared_library_compiles_generic_numeric_add_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric add shared-library smoke currently validates the Unix toolchain path");
#else
    assert_generic_numeric_binary_float_local(
            ZR_INSTRUCTION_ENUM(ADD),
            "add",
            "zr_aot_generic_numeric_f64_add_scalar_local",
            "zr_aot_f2 = zr_aot_f0 + zr_aot_f1;",
            "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)");
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_sub_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric sub shared-library smoke currently validates the Unix toolchain path");
#else
    assert_generic_numeric_binary_float_local(
            ZR_INSTRUCTION_ENUM(SUB),
            "sub",
            "zr_aot_generic_numeric_f64_sub_scalar_local",
            "zr_aot_f2 = zr_aot_f0 - zr_aot_f1;",
            "ZrLibrary_AotRuntime_GenericNumericSub(state, &frame, 2, 0, 1)");
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mul_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mul shared-library smoke currently validates the Unix toolchain path");
#else
    assert_generic_numeric_binary_float_local(
            ZR_INSTRUCTION_ENUM(MUL),
            "mul",
            "zr_aot_generic_numeric_f64_mul_scalar_local",
            "zr_aot_f2 = zr_aot_f0 * zr_aot_f1;",
            "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 2, 0, 1)");
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_div_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric div shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_float_function(state, ZR_INSTRUCTION_ENUM(DIV));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_div_smoke";
    options.sourceHash = "generic-numeric-div-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-div-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_div_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_div_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_div_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_f1 == (TZrFloat64)0.0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Debug_RunError(state, \"divide by zero\")"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = zr_aot_f0 / zr_aot_f1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mod_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric modulo shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_float_function(state, ZR_INSTRUCTION_ENUM(MOD));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mod_smoke";
    options.sourceHash = "generic-numeric-mod-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mod-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mod_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mod_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_mod_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_f1 == (TZrFloat64)0.0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Debug_RunError(state, \"modulo by zero\")"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = fmod(zr_aot_f0, zr_aot_f1);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_neg_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric neg shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_neg_float_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_neg_smoke";
    options.sourceHash = "generic-numeric-neg-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-neg-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_neg_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_neg_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_neg_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f1 = -zr_aot_f0;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_unary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericNeg(state, &frame, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 1, &zr_aot_f1)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_div_float_stack_copy_left_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric stack-copy smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_left_float_function(state, ZR_INSTRUCTION_ENUM(DIV));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_div_stack_copy_left_smoke";
    options.sourceHash = "generic-numeric-div-stack-copy-left-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-div-stack-copy-left-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_div_stack_copy_left_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_div_stack_copy_left_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_f64 dstSlot=3 srcSlot=0"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f3 = zr_aot_f0;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_div_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_f1 == (TZrFloat64)0.0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = zr_aot_f3 / zr_aot_f1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 2, 3, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 3, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[3].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZR_VALUE_IS_TYPE_FLOAT(zr_aot_left->type)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_add_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric result stack-copy smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_float_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_result_stack_copy_add_smoke";
    options.sourceHash = "generic-numeric-result-stack-copy-add-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-result-stack-copy-add-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_result_stack_copy_add_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_result_stack_copy_add_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_div_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = zr_aot_f0 / zr_aot_f1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f4 = zr_aot_f2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f5 = zr_aot_f4 + zr_aot_f3;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 5, 4, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 5, &zr_aot_f5)"));
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
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_div_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric result stack-copy div smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_div_float_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_result_stack_copy_div_smoke";
    options.sourceHash = "generic-numeric-result-stack-copy-div-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-result-stack-copy-div-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_result_stack_copy_div_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_result_stack_copy_div_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = zr_aot_f0 + zr_aot_f1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f4 = zr_aot_f2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_div_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_f3 == (TZrFloat64)0.0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f5 = zr_aot_f4 / zr_aot_f3;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 5, 4, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 5, &zr_aot_f5)"));
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
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_div_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric result stack-copy right div smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_right_div_float_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_result_stack_copy_right_div_smoke";
    options.sourceHash = "generic-numeric-result-stack-copy-right-div-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-result-stack-copy-right-div-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_result_stack_copy_right_div_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_result_stack_copy_right_div_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = zr_aot_f0 + zr_aot_f1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f4 = zr_aot_f2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_div_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_f4 == (TZrFloat64)0.0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f5 = zr_aot_f3 / zr_aot_f4;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 5, 3, 4)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 5, &zr_aot_f5)"));
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
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

#if defined(ZR_PLATFORM_UNIX)
static void assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(const char *generatedCText) {
    TEST_ASSERT_NOT_NULL(generatedCText);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "    .registerFrameBytes = 0u,"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "value SemIR lowering frameByteSize=0"));
    TEST_ASSERT_NULL(strstr(generatedCText, "/* zr_aot_generated_frame_setup */"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrAotGeneratedFrame frame = {0};"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase = zr_aot_slot_base;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Function_CheckStackAndGc("));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrCore_Value_ResetAsNull(&zr_aot_slot_base"));
}

static void assert_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_guarded_float_local(
        EZrInstructionCode opcode,
        TZrBool copiedResultIsRightOperand,
        const char *moduleName,
        const char *sourceHash,
        const char *artifactName,
        const char *sharedLibraryName,
        const char *operationMarkerNeedle,
        const char *zeroGuardNeedle,
        const char *expressionNeedle,
        const char *runtimeCallNeedle) {
#if !defined(ZR_PLATFORM_UNIX)
    (void)opcode;
    (void)copiedResultIsRightOperand;
    (void)moduleName;
    (void)sourceHash;
    (void)artifactName;
    (void)sharedLibraryName;
    (void)operationMarkerNeedle;
    (void)zeroGuardNeedle;
    (void)expressionNeedle;
    (void)runtimeCallNeedle;
    TEST_IGNORE_MESSAGE("AOT C generic numeric result stack-copy guarded f64 smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_guarded_float_function(
            state,
            opcode,
            copiedResultIsRightOperand);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.sourceHash = sourceHash;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = sourceHash;
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       artifactName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       sharedLibraryName,
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = zr_aot_f0 + zr_aot_f1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f4 = zr_aot_f2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, operationMarkerNeedle));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, zeroGuardNeedle));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, expressionNeedle));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, runtimeCallNeedle));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 5, &zr_aot_f5)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[4].value"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}
#endif

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mod_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric result stack-copy mod smoke currently validates the Unix toolchain path");
#else
    assert_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_guarded_float_local(
            ZR_INSTRUCTION_ENUM(MOD),
            ZR_FALSE,
            "aot_c_generic_numeric_result_stack_copy_mod_smoke",
            "generic-numeric-result-stack-copy-mod-smoke",
            "aot_c_generic_numeric_result_stack_copy_mod_smoke",
            "libaot_c_generic_numeric_result_stack_copy_mod_smoke",
            "zr_aot_generic_numeric_f64_mod_scalar_local",
            "if (zr_aot_f3 == (TZrFloat64)0.0)",
            "zr_aot_f5 = fmod(zr_aot_f4, zr_aot_f3);",
            "ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 5, 4, 3)");
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_mod_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric result stack-copy right mod smoke currently validates the Unix toolchain path");
#else
    assert_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_guarded_float_local(
            ZR_INSTRUCTION_ENUM(MOD),
            ZR_TRUE,
            "aot_c_generic_numeric_result_stack_copy_right_mod_smoke",
            "generic-numeric-result-stack-copy-right-mod-smoke",
            "aot_c_generic_numeric_result_stack_copy_right_mod_smoke",
            "libaot_c_generic_numeric_result_stack_copy_right_mod_smoke",
            "zr_aot_generic_numeric_f64_mod_scalar_local",
            "if (zr_aot_f4 == (TZrFloat64)0.0)",
            "zr_aot_f5 = fmod(zr_aot_f3, zr_aot_f4);",
            "ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 5, 3, 4)");
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_sub_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric result stack-copy right smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_right_float_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_result_stack_copy_right_sub_smoke";
    options.sourceHash = "generic-numeric-result-stack-copy-right-sub-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-result-stack-copy-right-sub-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_result_stack_copy_right_sub_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_result_stack_copy_right_sub_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_div_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = zr_aot_f0 / zr_aot_f1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f4 = zr_aot_f2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_sub_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f5 = zr_aot_f3 - zr_aot_f4;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericSub(state, &frame, 5, 3, 4)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 5, &zr_aot_f5)"));
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
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_chained_result_stack_copy_mul_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric chained result stack-copy smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_chained_stack_copy_result_float_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_chained_result_stack_copy_mul_smoke";
    options.sourceHash = "generic-numeric-chained-result-stack-copy-mul-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-chained-result-stack-copy-mul-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_chained_result_stack_copy_mul_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_chained_result_stack_copy_mul_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_div_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = zr_aot_f0 / zr_aot_f1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f4 = zr_aot_f2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_f64 dstSlot=6 srcSlot=4"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f6 = zr_aot_f4;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_mul_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f7 = zr_aot_f6 * zr_aot_f3;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 6, 4)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 7, 6, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 7, &zr_aot_f7)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[4].value"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[6].value"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_signed_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric signed result stack-copy smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_signed_int_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_i64_result_stack_copy_mul_smoke";
    options.sourceHash = "generic-numeric-i64-result-stack-copy-mul-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-i64-result-stack-copy-mul-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_i64_result_stack_copy_mul_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_i64_result_stack_copy_mul_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 + zr_aot_s1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_i64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s4 = zr_aot_s2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_mul_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s5 = zr_aot_s4 * zr_aot_s3;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 5, 4, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 5, &zr_aot_s5)"));
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
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mod_signed_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric signed result stack-copy mod smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_mod_signed_int_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_i64_result_stack_copy_mod_smoke";
    options.sourceHash = "generic-numeric-i64-result-stack-copy-mod-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-i64-result-stack-copy-mod-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_i64_result_stack_copy_mod_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_i64_result_stack_copy_mod_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 + zr_aot_s1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_i64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s4 = zr_aot_s2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_mod_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_s3 == (TZrInt64)0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s5 = zr_aot_s4 % zr_aot_s3;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 5, 4, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 5, &zr_aot_s5)"));
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
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_mod_signed_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric signed result stack-copy right mod smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_right_mod_signed_int_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_i64_result_stack_copy_right_mod_smoke";
    options.sourceHash = "generic-numeric-i64-result-stack-copy-right-mod-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-i64-result-stack-copy-right-mod-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_i64_result_stack_copy_right_mod_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_i64_result_stack_copy_right_mod_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 + zr_aot_s1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_i64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s4 = zr_aot_s2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_mod_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_s4 == (TZrInt64)0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s5 = zr_aot_s3 % zr_aot_s4;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 5, 3, 4)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 5, &zr_aot_s5)"));
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
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

#if defined(ZR_PLATFORM_UNIX)
static void assert_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_guarded_signed_int_local(
        EZrInstructionCode opcode,
        TZrBool copiedResultIsRightOperand,
        const char *moduleName,
        const char *sourceHash,
        const char *artifactName,
        const char *sharedLibraryName,
        const char *operationMarkerNeedle,
        const char *zeroGuardNeedle,
        const char *expressionNeedle,
        const char *runtimeCallNeedle) {
#if !defined(ZR_PLATFORM_UNIX)
    (void)opcode;
    (void)copiedResultIsRightOperand;
    (void)moduleName;
    (void)sourceHash;
    (void)artifactName;
    (void)sharedLibraryName;
    (void)operationMarkerNeedle;
    (void)zeroGuardNeedle;
    (void)expressionNeedle;
    (void)runtimeCallNeedle;
    TEST_IGNORE_MESSAGE("AOT C generic numeric result stack-copy guarded i64 smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_guarded_signed_int_function(
            state,
            opcode,
            copiedResultIsRightOperand);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.sourceHash = sourceHash;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = sourceHash;
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       artifactName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       sharedLibraryName,
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 + zr_aot_s1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_i64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s4 = zr_aot_s2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, operationMarkerNeedle));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, zeroGuardNeedle));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, expressionNeedle));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, runtimeCallNeedle));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 5, &zr_aot_s5)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[4].value"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}
#endif

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_div_signed_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric signed result stack-copy div smoke currently validates the Unix toolchain path");
#else
    assert_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_guarded_signed_int_local(
            ZR_INSTRUCTION_ENUM(DIV),
            ZR_FALSE,
            "aot_c_generic_numeric_i64_result_stack_copy_div_smoke",
            "generic-numeric-i64-result-stack-copy-div-smoke",
            "aot_c_generic_numeric_i64_result_stack_copy_div_smoke",
            "libaot_c_generic_numeric_i64_result_stack_copy_div_smoke",
            "zr_aot_generic_numeric_i64_div_scalar_local",
            "if (zr_aot_s3 == (TZrInt64)0)",
            "zr_aot_s5 = zr_aot_s4 / zr_aot_s3;",
            "ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 5, 4, 3)");
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_div_signed_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric signed result stack-copy right div smoke currently validates the Unix toolchain path");
#else
    assert_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_guarded_signed_int_local(
            ZR_INSTRUCTION_ENUM(DIV),
            ZR_TRUE,
            "aot_c_generic_numeric_i64_result_stack_copy_right_div_smoke",
            "generic-numeric-i64-result-stack-copy-right-div-smoke",
            "aot_c_generic_numeric_i64_result_stack_copy_right_div_smoke",
            "libaot_c_generic_numeric_i64_result_stack_copy_right_div_smoke",
            "zr_aot_generic_numeric_i64_div_scalar_local",
            "if (zr_aot_s4 == (TZrInt64)0)",
            "zr_aot_s5 = zr_aot_s3 / zr_aot_s4;",
            "ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 5, 3, 4)");
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric unsigned result stack-copy smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_unsigned_int_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_u64_result_stack_copy_mul_smoke";
    options.sourceHash = "generic-numeric-u64-result-stack-copy-mul-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-u64-result-stack-copy-mul-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_u64_result_stack_copy_mul_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_u64_result_stack_copy_mul_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_u64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_u2 = zr_aot_u0 + zr_aot_u1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_u64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_u4 = zr_aot_u2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_u64_mul_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_u5 = zr_aot_u4 * zr_aot_u3;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 5, 4, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_u64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, 5, &zr_aot_u5)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "TZrFloat64 zr_aot_f4"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[4].value"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_signed_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed i64/u64 result stack-copy smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_signed_unsigned_int_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_u64_result_stack_copy_mul_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-u64-result-stack-copy-mul-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-u64-result-stack-copy-mul-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_u64_result_stack_copy_mul_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_u64_result_stack_copy_mul_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_i64_u64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 + (TZrInt64)zr_aot_u1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_i64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s4 = zr_aot_s2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_i64_u64_mul_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s5 = zr_aot_s4 * (TZrInt64)zr_aot_u3;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 5, 4, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 5, &zr_aot_s5)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "TZrFloat64 zr_aot_f4"));
    TEST_ASSERT_NULL(strstr(generatedCText, "frame.slotBase[4].value"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_signed_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed i64/f64 result stack-copy smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_signed_float_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_f64_result_stack_copy_mul_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-f64-result-stack-copy-mul-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-f64-result-stack-copy-mul-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_f64_result_stack_copy_mul_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_f64_result_stack_copy_mul_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_f64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = (TZrFloat64)zr_aot_s0 + zr_aot_f1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f4 = zr_aot_f2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_mul_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f5 = zr_aot_f4 * zr_aot_f3;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 5, 4, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 5, &zr_aot_f5)"));
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
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_unsigned_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed u64/f64 result stack-copy smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_stack_copy_result_unsigned_float_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_u64_f64_result_stack_copy_mul_smoke";
    options.sourceHash = "generic-numeric-mixed-u64-f64-result-stack-copy-mul-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-u64-f64-result-stack-copy-mul-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_u64_f64_result_stack_copy_mul_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_u64_f64_result_stack_copy_mul_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_f64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = (TZrFloat64)zr_aot_u0 + zr_aot_f1;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_stack_copy_f64 dstSlot=4 srcSlot=2"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f4 = zr_aot_f2;"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_f64_mul_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f5 = zr_aot_f4 * zr_aot_f3;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_CopyStack(state, &frame, 4, 2)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 5, 4, 3)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 5, &zr_aot_f5)"));
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
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_add_signed_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric signed add shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_int_function(state, ZR_INSTRUCTION_ENUM(ADD));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_i64_add_smoke";
    options.sourceHash = "generic-numeric-i64-add-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-i64-add-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_i64_add_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_i64_add_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 + zr_aot_s1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 2, &zr_aot_s2)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_add_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric unsigned add shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_unsigned_int_function(state, ZR_INSTRUCTION_ENUM(ADD));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_u64_add_smoke";
    options.sourceHash = "generic-numeric-u64-add-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-u64-add-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_u64_add_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_u64_add_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_u64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_u2 = zr_aot_u0 + zr_aot_u1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_u64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, 2, &zr_aot_u2)"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_add_signed_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed i64/u64 add shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_unsigned_int_function(state, ZR_INSTRUCTION_ENUM(ADD));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_u64_add_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-u64-add-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-u64-add-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_u64_add_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_u64_add_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_i64_u64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 + (TZrInt64)zr_aot_u1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 2, &zr_aot_s2)"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_sub_signed_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed i64/u64 sub shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_unsigned_int_function(state, ZR_INSTRUCTION_ENUM(SUB));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_u64_sub_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-u64-sub-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-u64-sub-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_u64_sub_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_u64_sub_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_i64_u64_sub_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 - (TZrInt64)zr_aot_u1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericSub(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 2, &zr_aot_s2)"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mul_signed_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed i64/u64 mul shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_unsigned_int_function(state, ZR_INSTRUCTION_ENUM(MUL));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_u64_mul_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-u64-mul-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-u64-mul-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_u64_mul_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_u64_mul_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_i64_u64_mul_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 * (TZrInt64)zr_aot_u1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 2, &zr_aot_s2)"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_div_signed_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed i64/u64 div shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_unsigned_int_function(state, ZR_INSTRUCTION_ENUM(DIV));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_u64_div_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-u64-div-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-u64-div-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_u64_div_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_u64_div_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_i64_u64_div_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if ((TZrInt64)zr_aot_u1 == (TZrInt64)0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Debug_RunError(state, \"divide by zero\")"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 / (TZrInt64)zr_aot_u1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 2, &zr_aot_s2)"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mod_signed_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed i64/u64 mod shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_unsigned_int_function(state, ZR_INSTRUCTION_ENUM(MOD));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_u64_mod_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-u64-mod-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-u64-mod-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_u64_mod_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_u64_mod_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_i64_u64_mod_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if ((TZrInt64)zr_aot_u1 == (TZrInt64)0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Debug_RunError(state, \"modulo by zero\")"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 % (TZrInt64)zr_aot_u1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 2, &zr_aot_s2)"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

#if defined(ZR_PLATFORM_UNIX)
static void assert_generic_numeric_binary_unsigned_int_float_local(EZrInstructionCode opcode,
                                                                   const char *operationName,
                                                                   const char *localExpression,
                                                                   const char *forbiddenRuntimeCall,
                                                                   const char *zeroGuard,
                                                                   const char *errorMessage) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char moduleName[128];
    char hashText[128];
    char libraryName[160];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(operationName);
    TEST_ASSERT_NOT_NULL(localExpression);
    TEST_ASSERT_NOT_NULL(forbiddenRuntimeCall);
    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_unsigned_int_float_function(state, opcode);
    TEST_ASSERT_NOT_NULL(function);

    snprintf(moduleName, sizeof(moduleName), "aot_c_generic_numeric_mixed_u64_f64_%s_smoke", operationName);
    snprintf(hashText, sizeof(hashText), "generic-numeric-mixed-u64-f64-%s-smoke", operationName);
    snprintf(libraryName, sizeof(libraryName), "libaot_c_generic_numeric_mixed_u64_f64_%s_smoke", operationName);

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.sourceHash = hashText;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = hashText;
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       moduleName,
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       libraryName,
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_f64_"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, localExpression));
    if (zeroGuard != ZR_NULL) {
        TEST_ASSERT_NOT_NULL(strstr(generatedCText, zeroGuard));
    }
    if (errorMessage != ZR_NULL) {
        TEST_ASSERT_NOT_NULL(strstr(generatedCText, errorMessage));
    }
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, forbiddenRuntimeCall));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}
#endif

static void test_aot_c_generated_shared_library_compiles_generic_numeric_add_unsigned_int_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed u64/f64 add shared-library smoke currently validates the Unix toolchain path");
#else
    assert_generic_numeric_binary_unsigned_int_float_local(
            ZR_INSTRUCTION_ENUM(ADD),
            "add",
            "zr_aot_f2 = (TZrFloat64)zr_aot_u0 + zr_aot_f1;",
            "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)",
            ZR_NULL,
            ZR_NULL);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_sub_unsigned_int_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed u64/f64 sub shared-library smoke currently validates the Unix toolchain path");
#else
    assert_generic_numeric_binary_unsigned_int_float_local(
            ZR_INSTRUCTION_ENUM(SUB),
            "sub",
            "zr_aot_f2 = (TZrFloat64)zr_aot_u0 - zr_aot_f1;",
            "ZrLibrary_AotRuntime_GenericNumericSub(state, &frame, 2, 0, 1)",
            ZR_NULL,
            ZR_NULL);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mul_unsigned_int_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed u64/f64 mul shared-library smoke currently validates the Unix toolchain path");
#else
    assert_generic_numeric_binary_unsigned_int_float_local(
            ZR_INSTRUCTION_ENUM(MUL),
            "mul",
            "zr_aot_f2 = (TZrFloat64)zr_aot_u0 * zr_aot_f1;",
            "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 2, 0, 1)",
            ZR_NULL,
            ZR_NULL);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_div_unsigned_int_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed u64/f64 div shared-library smoke currently validates the Unix toolchain path");
#else
    assert_generic_numeric_binary_unsigned_int_float_local(
            ZR_INSTRUCTION_ENUM(DIV),
            "div",
            "zr_aot_f2 = (TZrFloat64)zr_aot_u0 / zr_aot_f1;",
            "ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 2, 0, 1)",
            "if (zr_aot_f1 == (TZrFloat64)0.0)",
            "ZrCore_Debug_RunError(state, \"divide by zero\")");
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mod_unsigned_int_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed u64/f64 mod shared-library smoke currently validates the Unix toolchain path");
#else
    assert_generic_numeric_binary_unsigned_int_float_local(
            ZR_INSTRUCTION_ENUM(MOD),
            "mod",
            "zr_aot_f2 = fmod((TZrFloat64)zr_aot_u0, zr_aot_f1);",
            "ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 2, 0, 1)",
            "if (zr_aot_f1 == (TZrFloat64)0.0)",
            "ZrCore_Debug_RunError(state, \"modulo by zero\")");
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_add_signed_int_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed add shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_int_float_function(state, ZR_INSTRUCTION_ENUM(ADD));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_f64_add_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-f64-add-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-f64-add-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_f64_add_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_f64_add_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_f64_add_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = (TZrFloat64)zr_aot_s0 + zr_aot_f1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericAdd(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_sub_signed_int_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed sub shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_int_float_function(state, ZR_INSTRUCTION_ENUM(SUB));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_f64_sub_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-f64-sub-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-f64-sub-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_f64_sub_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_f64_sub_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_f64_sub_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = (TZrFloat64)zr_aot_s0 - zr_aot_f1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericSub(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mul_signed_int_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed mul shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_int_float_function(state, ZR_INSTRUCTION_ENUM(MUL));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_f64_mul_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-f64-mul-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-f64-mul-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_f64_mul_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_f64_mul_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_f64_mul_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = (TZrFloat64)zr_aot_s0 * zr_aot_f1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_div_signed_int_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed div shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_int_float_function(state, ZR_INSTRUCTION_ENUM(DIV));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_f64_div_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-f64-div-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-f64-div-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_f64_div_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_f64_div_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_f64_div_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_f1 == (TZrFloat64)0.0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Debug_RunError(state, \"divide by zero\")"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = (TZrFloat64)zr_aot_s0 / zr_aot_f1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mod_signed_int_float_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric mixed mod shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_int_float_function(state, ZR_INSTRUCTION_ENUM(MOD));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_mixed_i64_f64_mod_smoke";
    options.sourceHash = "generic-numeric-mixed-i64-f64-mod-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-mixed-i64-f64-mod-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_mixed_i64_f64_mod_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_mixed_i64_f64_mod_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_f64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_mixed_f64_mod_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_f1 == (TZrFloat64)0.0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Debug_RunError(state, \"modulo by zero\")"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_f2 = fmod((TZrFloat64)zr_aot_s0, zr_aot_f1);"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_f64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 2, &zr_aot_f2)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_sub_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric unsigned sub shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_unsigned_int_function(state, ZR_INSTRUCTION_ENUM(SUB));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_u64_sub_smoke";
    options.sourceHash = "generic-numeric-u64-sub-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-u64-sub-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_u64_sub_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_u64_sub_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_u64_sub_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_u2 = zr_aot_u0 - zr_aot_u1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericSub(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_u64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, 2, &zr_aot_u2)"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mul_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric unsigned mul shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_unsigned_int_function(state, ZR_INSTRUCTION_ENUM(MUL));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_u64_mul_smoke";
    options.sourceHash = "generic-numeric-u64-mul-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-u64-mul-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_u64_mul_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_u64_mul_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_u64_mul_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_u2 = zr_aot_u0 * zr_aot_u1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_u64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, 2, &zr_aot_u2)"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_div_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric unsigned div shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_unsigned_int_function(state, ZR_INSTRUCTION_ENUM(DIV));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_u64_div_smoke";
    options.sourceHash = "generic-numeric-u64-div-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-u64-div-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_u64_div_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_u64_div_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_u64_div_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_u1 == (TZrUInt64)0u)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Debug_RunError(state, \"divide by zero\")"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_u2 = zr_aot_u0 / zr_aot_u1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_u64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, 2, &zr_aot_u2)"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mod_unsigned_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric unsigned mod shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_unsigned_int_function(state, ZR_INSTRUCTION_ENUM(MOD));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_u64_mod_smoke";
    options.sourceHash = "generic-numeric-u64-mod-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-u64-mod-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_u64_mod_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_u64_mod_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_u64_mod_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_u1 == (TZrUInt64)0u)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Debug_RunError(state, \"modulo by zero\")"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_u2 = zr_aot_u0 % zr_aot_u1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_u64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, 2, &zr_aot_u2)"));
    assert_generic_numeric_generated_c_uses_zero_frame_scalar_body(generatedCText);
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_sub_signed_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric signed sub shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_int_function(state, ZR_INSTRUCTION_ENUM(SUB));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_i64_sub_smoke";
    options.sourceHash = "generic-numeric-i64-sub-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-i64-sub-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_i64_sub_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_i64_sub_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_sub_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 - zr_aot_s1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericSub(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 2, &zr_aot_s2)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mul_signed_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric signed mul shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_int_function(state, ZR_INSTRUCTION_ENUM(MUL));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_i64_mul_smoke";
    options.sourceHash = "generic-numeric-i64-mul-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-i64-mul-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_i64_mul_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_i64_mul_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_mul_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 * zr_aot_s1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMul(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 2, &zr_aot_s2)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_div_signed_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric signed div shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_int_function(state, ZR_INSTRUCTION_ENUM(DIV));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_i64_div_smoke";
    options.sourceHash = "generic-numeric-i64-div-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-i64-div-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_i64_div_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_i64_div_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_div_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_s1 == (TZrInt64)0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Debug_RunError(state, \"divide by zero\")"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 / zr_aot_s1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericDiv(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 2, &zr_aot_s2)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_mod_signed_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric signed mod shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_binary_signed_int_function(state, ZR_INSTRUCTION_ENUM(MOD));
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_i64_mod_smoke";
    options.sourceHash = "generic-numeric-i64-mod-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-i64-mod-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_i64_mod_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_i64_mod_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_mod_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "if (zr_aot_s1 == (TZrInt64)0)"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "ZrCore_Debug_RunError(state, \"modulo by zero\")"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s2 = zr_aot_s0 % zr_aot_s1;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_binary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericMod(state, &frame, 2, 0, 1)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 2, &zr_aot_s2)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_neg_signed_int_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric signed neg shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_neg_signed_int_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_i64_neg_smoke";
    options.sourceHash = "generic-numeric-i64-neg-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-i64-neg-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_i64_neg_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_i64_neg_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_i64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_i64_neg_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s1 = -zr_aot_s0;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_unary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericNeg(state, &frame, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 1, &zr_aot_s1)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_c_generated_shared_library_compiles_generic_numeric_neg_unsigned_int_to_signed_local(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE("AOT C generic numeric unsigned neg shared-library smoke currently validates the Unix toolchain path");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char command[4096];

    TEST_ASSERT_NOT_NULL(state);
    function = create_generic_numeric_neg_unsigned_int_function(state);
    TEST_ASSERT_NOT_NULL(function);

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_generic_numeric_u64_neg_to_i64_smoke";
    options.sourceHash = "generic-numeric-u64-neg-to-i64-smoke";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "generic-numeric-u64-neg-to-i64-smoke";
    options.requireExecutableLowering = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "src",
                                                       "aot_c_generic_numeric_u64_neg_to_i64_smoke",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_generic_numeric_shared_library",
                                                       "lib",
                                                       "libaot_c_generic_numeric_u64_neg_to_i64_smoke",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_scalar_constant_u64_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_u64_neg_to_i64_scalar_local"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, "zr_aot_s1 = -(TZrInt64)zr_aot_u0;"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_arith_exec_generic_numeric_unary_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_GenericNumericNeg(state, &frame, 1, 0)"));
    TEST_ASSERT_NULL(strstr(generatedCText, "zr_aot_generic_numeric_sync_i64_local_boundary"));
    TEST_ASSERT_NULL(strstr(generatedCText, "ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, 1, &zr_aot_s1)"));
    free(generatedCText);

    snprintf(command,
             sizeof(command),
             "\"%s\" -std=c11 -fPIC -shared -DZR_PLATFORM_UNIX -DZR_DEBUG "
             "-I\"%s/zr_vm_common/include\" "
             "-I\"%s/zr_vm_core/include\" "
             "-I\"%s/zr_vm_library/include\" "
             "\"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             "-lzr_vm_library -lzr_vm_core -lm "
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

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_add_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_sub_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mul_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_div_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mod_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_neg_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_div_float_stack_copy_left_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_add_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_div_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_div_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mod_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_mod_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_sub_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_chained_result_stack_copy_mul_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_signed_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_div_signed_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_div_signed_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mod_signed_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_right_mod_signed_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_signed_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_signed_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_result_stack_copy_mul_unsigned_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_add_signed_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_add_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_add_signed_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_sub_signed_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mul_signed_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_div_signed_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mod_signed_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_add_unsigned_int_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_sub_unsigned_int_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mul_unsigned_int_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_div_unsigned_int_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mod_unsigned_int_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_add_signed_int_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_sub_signed_int_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mul_signed_int_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_div_signed_int_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mod_signed_int_float_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_sub_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mul_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_div_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mod_unsigned_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_sub_signed_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mul_signed_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_div_signed_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_mod_signed_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_neg_signed_int_local);
    RUN_TEST(test_aot_c_generated_shared_library_compiles_generic_numeric_neg_unsigned_int_to_signed_local);
    return UNITY_END();
}
