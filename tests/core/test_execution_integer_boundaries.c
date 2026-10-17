#include <stdint.h>
#include <string.h>
#include "unity.h"
#include "tests/harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"

void setUp(void) {}
void tearDown(void) {}

typedef enum OperandForm { SLOTS, CONSTANT, LOAD_CONSTANT, LOAD_STACK_CONSTANT,
                           LOAD_STACK, LOAD_STACK_LOAD_CONSTANT, ADD_MOD } OperandForm;
typedef struct ArithmeticForm { EZrInstructionCode opcode; OperandForm form; } ArithmeticForm;
#define FORM(NAME, KIND) { ZR_INSTRUCTION_ENUM(NAME), KIND }
static const ArithmeticForm adds[] = {
    FORM(ADD, SLOTS), FORM(ADD_INT, SLOTS), FORM(ADD_INT_PLAIN_DEST, SLOTS),
    FORM(ADD_INT_CONST, CONSTANT), FORM(ADD_INT_CONST_PLAIN_DEST, CONSTANT),
    FORM(ADD_SIGNED, SLOTS), FORM(ADD_SIGNED_PLAIN_DEST, SLOTS),
    FORM(ADD_SIGNED_CONST, CONSTANT), FORM(ADD_SIGNED_CONST_PLAIN_DEST, CONSTANT),
    FORM(ADD_SIGNED_LOAD_CONST, LOAD_CONSTANT),
    FORM(ADD_SIGNED_LOAD_STACK_CONST, LOAD_STACK_CONSTANT),
    FORM(ADD_SIGNED_LOAD_STACK, LOAD_STACK),
    FORM(ADD_SIGNED_LOAD_STACK_LOAD_CONST, LOAD_STACK_LOAD_CONSTANT),
    FORM(ADD_SIGNED_MOD_CONST, ADD_MOD)
};
static const ArithmeticForm subtracts[] = {
    FORM(SUB, SLOTS), FORM(SUB_INT, SLOTS), FORM(SUB_INT_PLAIN_DEST, SLOTS),
    FORM(SUB_INT_CONST, CONSTANT), FORM(SUB_INT_CONST_PLAIN_DEST, CONSTANT),
    FORM(SUB_SIGNED, SLOTS), FORM(SUB_SIGNED_PLAIN_DEST, SLOTS),
    FORM(SUB_SIGNED_CONST, CONSTANT), FORM(SUB_SIGNED_CONST_PLAIN_DEST, CONSTANT),
    FORM(SUB_SIGNED_LOAD_CONST, LOAD_CONSTANT),
    FORM(SUB_SIGNED_LOAD_STACK_CONST, LOAD_STACK_CONSTANT)
};

static TZrInstruction instruction(EZrInstructionCode code, TZrUInt16 dest,
                                  TZrUInt16 left, TZrUInt16 right) {
    TZrInstruction inst = {0};
    inst.instruction.operationCode = code;
    inst.instruction.operandExtra = dest;
    inst.instruction.operand.operand1[0] = left;
    inst.instruction.operand.operand1[1] = right;
    return inst;
}

/* Execute real bytecode, including the packed operand layouts and aliased
 * destinations used by quickening. An error must occur before any host UB. */
static void check(ArithmeticForm form, TZrInt64 left, TZrInt64 right,
                  TZrBool succeeds, TZrInt64 expected, TZrUInt16 dest) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrTypeValue constants[3], result;
    TZrInstruction code[4];
    TZrBool success;
    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    ZrCore_Value_InitAsInt(state, &constants[0], left);
    ZrCore_Value_InitAsInt(state, &constants[1], right);
    ZrCore_Value_InitAsInt(state, &constants[2], 7);
    code[0] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0, 0);
    code[1] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1, 0);
    code[2] = instruction(form.opcode, dest, 0, 1);
    if (form.form == LOAD_CONSTANT || form.form == LOAD_STACK || form.form == ADD_MOD) {
        code[2].instruction.operand.operand0[0] = 0;
        code[2].instruction.operand.operand0[1] = 1;
        if (form.form != LOAD_STACK) code[2].instruction.operand.operand1[1] = form.form == ADD_MOD ? 2 : 1;
    } else if (form.form == LOAD_STACK_CONSTANT || form.form == LOAD_STACK_LOAD_CONSTANT) {
        code[2].instruction.operand.operand0[0] = 0;
        code[2].instruction.operand.operand0[1] = 3;
        code[2].instruction.operand.operand1[1] = 1;
    }
    code[3] = instruction(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, dest, 0);
    function->instructionsList = ZrCore_Memory_RawMallocWithType(state->global, sizeof(code), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->constantValueList = ZrCore_Memory_RawMallocWithType(state->global, sizeof(constants), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    memcpy(function->instructionsList, code, sizeof(code));
    memcpy(function->constantValueList, constants, sizeof(constants));
    function->instructionsLength = 4;
    function->constantValueLength = 3;
    function->stackSize = 6;
    success = ZrTests_Runtime_Function_Execute(state, function, &result);
    /* Release the VM before assertions, which can longjmp out of a test. */
    if (!success) ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    TEST_ASSERT_EQUAL_INT_MESSAGE(succeeds, success, "Unexpected arithmetic completion");
    if (success) {
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(result.type));
        TEST_ASSERT_EQUAL_INT64(expected, result.value.nativeObject.nativeInt64);
    }
}

static void test_checked_add_and_subtract_variants(void) {
    for (unsigned dest = 0; dest < 3; ++dest) {
        for (unsigned i = 0; i < ZR_ARRAY_COUNT(adds); ++i) {
            check(adds[i], INT64_MAX, 1, ZR_FALSE, 0, dest);
            check(adds[i], INT64_MIN, -1, ZR_FALSE, 0, dest);
            check(adds[i], 8, -3, ZR_TRUE, 5, dest);
        }
        for (unsigned i = 0; i < ZR_ARRAY_COUNT(subtracts); ++i) {
            check(subtracts[i], INT64_MIN, 1, ZR_FALSE, 0, dest);
            check(subtracts[i], INT64_MAX, -1, ZR_FALSE, 0, dest);
            check(subtracts[i], -8, -3, ZR_TRUE, -5, dest);
        }
    }
}

static void test_checked_negation(void) {
    const ArithmeticForm forms[] = { FORM(NEG, SLOTS), FORM(NEG_SIGNED, SLOTS) };
    for (unsigned i = 0; i < ZR_ARRAY_COUNT(forms); ++i) {
        check(forms[i], INT64_MIN, 0, ZR_FALSE, 0, 0);
        check(forms[i], -INT64_MAX, 0, ZR_TRUE, INT64_MAX, 0);
    }
}

static void test_masked_shifts(void) {
    const ArithmeticForm lefts[] = { FORM(SHIFT_LEFT_INT, SLOTS), FORM(BITWISE_SHIFT_LEFT, SLOTS) };
    const ArithmeticForm rights[] = { FORM(SHIFT_RIGHT_INT, SLOTS), FORM(BITWISE_SHIFT_RIGHT, SLOTS) };
    for (unsigned i = 0; i < ZR_ARRAY_COUNT(lefts); ++i) {
        check(lefts[i], 1, -1, ZR_TRUE, INT64_MIN, 0);
        check(lefts[i], -1, 64, ZR_TRUE, -1, 0);
        check(lefts[i], INT64_MIN, 1, ZR_TRUE, 0, 0);
        check(rights[i], INT64_MIN, -1, ZR_TRUE, -1, 0);
        check(rights[i], -2, 65, ZR_TRUE, -1, 0);
        check(rights[i], INT64_MIN, 64, ZR_TRUE, INT64_MIN, 0);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_checked_add_and_subtract_variants);
    RUN_TEST(test_checked_negation);
    RUN_TEST(test_masked_shifts);
    return UNITY_END();
}
