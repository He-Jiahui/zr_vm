#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"

static TZrInstruction make_instruction_1(EZrInstructionCode opcode,
                                         TZrUInt16 operandExtra,
                                         TZrInt32 operand) {
    TZrInstruction instruction = {0};
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand2[0] = operand;
    return instruction;
}

static TZrInstruction make_instruction_2(EZrInstructionCode opcode,
                                         TZrUInt16 operandExtra,
                                         TZrUInt16 operand1,
                                         TZrUInt16 operand2) {
    TZrInstruction instruction = {0};
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operand1;
    instruction.instruction.operand.operand1[1] = operand2;
    return instruction;
}

static SZrFunction *create_test_function(SZrState *state,
                                         const TZrInstruction *instructions,
                                         TZrUInt32 instructionCount,
                                         const SZrTypeValue *constants,
                                         TZrUInt32 constantCount,
                                         TZrUInt32 stackSize) {
    SZrFunction *function;
    TZrSize instructionsSize;
    TZrSize constantsSize;

    if (state == ZR_NULL || state->global == ZR_NULL || instructions == ZR_NULL) {
        return ZR_NULL;
    }

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    instructionsSize = (TZrSize)instructionCount * sizeof(*instructions);
    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            instructionsSize,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->instructionsList == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }
    memcpy(function->instructionsList, instructions, instructionsSize);
    function->instructionsLength = instructionCount;

    if (constantCount > 0u) {
        if (constants == ZR_NULL) {
            ZrCore_Function_Free(state, function);
            return ZR_NULL;
        }
        constantsSize = (TZrSize)constantCount * sizeof(*constants);
        function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
                state->global,
                constantsSize,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->constantValueList == ZR_NULL) {
            ZrCore_Function_Free(state, function);
            return ZR_NULL;
        }
        memcpy(function->constantValueList, constants, constantsSize);
        function->constantValueLength = constantCount;
    }

    function->stackSize = stackSize;
    function->parameterCount = 0u;
    function->hasVariableArguments = ZR_FALSE;
    return function;
}

static void assert_unsigned_binary_bitwise_result(EZrInstructionCode opcode,
                                                  TZrUInt64 left,
                                                  TZrUInt64 right,
                                                  TZrUInt64 expected) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue constants[2];
    TZrInstruction instructions[4];
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsUInt(state, &constants[0], left);
    ZrCore_Value_InitAsUInt(state, &constants[1], right);
    instructions[0] = make_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    instructions[1] = make_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    instructions[2] = make_instruction_2(opcode, 2u, 0u, 1u);
    instructions[3] = make_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1u, 2u, 0u);

    function = create_test_function(state, instructions, 4u, constants, 2u, 3u);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, result.type);
    TEST_ASSERT_EQUAL_UINT64(expected, result.value.nativeObject.nativeUInt64);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_bitwise_and_uint_preserves_uint64_result(void) {
    assert_unsigned_binary_bitwise_result(ZR_INSTRUCTION_ENUM(BITWISE_AND), 0xFu, 0x6u, 0x6u);
}

static void test_bitwise_or_uint_preserves_uint64_result(void) {
    assert_unsigned_binary_bitwise_result(ZR_INSTRUCTION_ENUM(BITWISE_OR), 0x9u, 0x6u, 0xFu);
}

static void test_bitwise_xor_uint_preserves_uint64_result(void) {
    assert_unsigned_binary_bitwise_result(ZR_INSTRUCTION_ENUM(BITWISE_XOR), 0xFu, 0x6u, 0x9u);
}

static void assert_unsigned_shift_result(EZrInstructionCode opcode,
                                         TZrUInt64 left,
                                         TZrInt64 shift,
                                         TZrUInt64 expected) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue constants[2];
    TZrInstruction instructions[4];
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsUInt(state, &constants[0], left);
    ZrCore_Value_InitAsInt(state, &constants[1], shift);
    instructions[0] = make_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    instructions[1] = make_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    instructions[2] = make_instruction_2(opcode, 2u, 0u, 1u);
    instructions[3] = make_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1u, 2u, 0u);

    function = create_test_function(state, instructions, 4u, constants, 2u, 3u);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, result.type);
    TEST_ASSERT_EQUAL_UINT64(expected, result.value.nativeObject.nativeUInt64);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_bitwise_shift_left_uint_preserves_uint64_result(void) {
    assert_unsigned_shift_result(ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT), 0x10u, 2, 0x40u);
}

static void test_bitwise_shift_right_uint_preserves_uint64_result(void) {
    assert_unsigned_shift_result(ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT), 0x40u, 2, 0x10u);
}

static void test_bitwise_not_uint_preserves_uint64_result(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue constant;
    TZrInstruction instructions[3];
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsUInt(state, &constant, 0xFu);
    instructions[0] = make_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    instructions[1] = make_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_NOT), 1u, 0u, 0u);
    instructions[2] = make_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1u, 1u, 0u);

    function = create_test_function(state, instructions, 3u, &constant, 1u, 2u);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, result.type);
    TEST_ASSERT_EQUAL_UINT64(~(TZrUInt64)0xFu, result.value.nativeObject.nativeUInt64);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_uint_bitwise_result_can_cast_to_signed_add_operand(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue constants[3];
    TZrInstruction instructions[7];
    SZrFunction *function;
    SZrTypeValue result;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsUInt(state, &constants[0], 12u);
    ZrCore_Value_InitAsUInt(state, &constants[1], 4u);
    ZrCore_Value_InitAsInt(state, &constants[2], 40);
    instructions[0] = make_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0u, 0);
    instructions[1] = make_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1u, 1);
    instructions[2] = make_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_AND), 2u, 0u, 1u);
    instructions[3] = make_instruction_2(ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED), 3u, 2u, 0u);
    instructions[4] = make_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 4u, 2);
    instructions[5] = make_instruction_2(ZR_INSTRUCTION_ENUM(ADD_SIGNED), 5u, 4u, 3u);
    instructions[6] = make_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1u, 5u, 0u);

    function = create_test_function(state, instructions, 7u, constants, 3u, 6u);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, function, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(44, result.value.nativeObject.nativeInt64);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_bitwise_and_uint_preserves_uint64_result);
    RUN_TEST(test_bitwise_or_uint_preserves_uint64_result);
    RUN_TEST(test_bitwise_xor_uint_preserves_uint64_result);
    RUN_TEST(test_bitwise_shift_left_uint_preserves_uint64_result);
    RUN_TEST(test_bitwise_shift_right_uint_preserves_uint64_result);
    RUN_TEST(test_bitwise_not_uint_preserves_uint64_result);
    RUN_TEST(test_uint_bitwise_result_can_cast_to_signed_add_operand);

    return UNITY_END();
}
