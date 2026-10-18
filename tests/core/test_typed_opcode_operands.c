#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "tests/harness/runtime_support.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"

void setUp(void) {}
void tearDown(void) {}

typedef enum Layout { SLOTS, UNARY, CONSTANT, LOAD_CONST, LOAD_STACK_CONST,
                      LOAD_STACK, LOAD_STACK_LOAD_CONST, ADD_MOD, BRANCH, CONST_BRANCH } Layout;
typedef enum Kind { SIGNED, UNSIGNED, FLOATING, BOOLEAN, STRING } Kind;
typedef struct Case { EZrInstructionCode opcode; Layout layout; Kind kind; const char *name; } Case;
#define CASE(NAME, LAYOUT, KIND) { ZR_INSTRUCTION_ENUM(NAME), LAYOUT, KIND, #NAME }
#define SIGNED_FORMS(OP) \
    CASE(OP##_INT, SLOTS, SIGNED), CASE(OP##_INT_PLAIN_DEST, SLOTS, SIGNED), \
    CASE(OP##_INT_CONST, CONSTANT, SIGNED), CASE(OP##_INT_CONST_PLAIN_DEST, CONSTANT, SIGNED), \
    CASE(OP##_SIGNED, SLOTS, SIGNED), CASE(OP##_SIGNED_PLAIN_DEST, SLOTS, SIGNED), \
    CASE(OP##_SIGNED_CONST, CONSTANT, SIGNED), CASE(OP##_SIGNED_CONST_PLAIN_DEST, CONSTANT, SIGNED), \
    CASE(OP##_SIGNED_LOAD_CONST, LOAD_CONST, SIGNED), \
    CASE(OP##_SIGNED_LOAD_STACK_CONST, LOAD_STACK_CONST, SIGNED)
#define UNSIGNED_FORMS(OP) \
    CASE(OP##_UNSIGNED, SLOTS, UNSIGNED), CASE(OP##_UNSIGNED_PLAIN_DEST, SLOTS, UNSIGNED), \
    CASE(OP##_UNSIGNED_CONST, CONSTANT, UNSIGNED), CASE(OP##_UNSIGNED_CONST_PLAIN_DEST, CONSTANT, UNSIGNED)
#define COMPARE(OP) \
    CASE(OP##_SIGNED, SLOTS, SIGNED), CASE(OP##_UNSIGNED, SLOTS, UNSIGNED)
static const Case cases[] = {
    SIGNED_FORMS(ADD), SIGNED_FORMS(SUB),
    CASE(ADD_SIGNED_LOAD_STACK, LOAD_STACK, SIGNED),
    CASE(ADD_SIGNED_LOAD_STACK_LOAD_CONST, LOAD_STACK_LOAD_CONST, SIGNED),
    CASE(ADD_SIGNED_MOD_CONST, ADD_MOD, SIGNED),
    UNSIGNED_FORMS(ADD), UNSIGNED_FORMS(SUB), UNSIGNED_FORMS(MUL),
    CASE(MUL_SIGNED, SLOTS, SIGNED), CASE(MUL_SIGNED_PLAIN_DEST, SLOTS, SIGNED),
    CASE(MUL_SIGNED_CONST, CONSTANT, SIGNED), CASE(MUL_SIGNED_CONST_PLAIN_DEST, CONSTANT, SIGNED),
    CASE(MUL_SIGNED_LOAD_CONST, LOAD_CONST, SIGNED),
    CASE(MUL_SIGNED_LOAD_STACK_CONST, LOAD_STACK_CONST, SIGNED),
    CASE(MUL_SIGNED_LOAD_STACK, LOAD_STACK, SIGNED),
    CASE(DIV_SIGNED_CONST, CONSTANT, SIGNED), CASE(DIV_SIGNED_CONST_PLAIN_DEST, CONSTANT, SIGNED),
    CASE(DIV_SIGNED_LOAD_CONST, LOAD_CONST, SIGNED),
    CASE(DIV_SIGNED_LOAD_STACK_CONST, LOAD_STACK_CONST, SIGNED),
    CASE(MOD_SIGNED_CONST, CONSTANT, SIGNED), CASE(MOD_SIGNED_CONST_PLAIN_DEST, CONSTANT, SIGNED),
    CASE(MOD_SIGNED_LOAD_CONST, LOAD_CONST, SIGNED),
    CASE(MOD_SIGNED_LOAD_STACK_CONST, LOAD_STACK_CONST, SIGNED),
    CASE(DIV_UNSIGNED_CONST, CONSTANT, UNSIGNED), CASE(DIV_UNSIGNED_CONST_PLAIN_DEST, CONSTANT, UNSIGNED),
    CASE(MOD_UNSIGNED_CONST, CONSTANT, UNSIGNED), CASE(MOD_UNSIGNED_CONST_PLAIN_DEST, CONSTANT, UNSIGNED),
    CASE(NEG_SIGNED, UNARY, SIGNED), CASE(NEG_FLOAT, UNARY, FLOATING),
    CASE(TO_FLOAT_SIGNED, UNARY, SIGNED), CASE(TO_FLOAT_UNSIGNED, UNARY, UNSIGNED),
    CASE(TO_INT_FLOAT, UNARY, FLOATING), CASE(TO_INT_UNSIGNED, UNARY, UNSIGNED),
    CASE(TO_UINT_FLOAT, UNARY, FLOATING), CASE(TO_UINT_SIGNED, UNARY, SIGNED),
    CASE(LOGICAL_NOT_BOOL, UNARY, BOOLEAN),
    CASE(LOGICAL_AND, SLOTS, BOOLEAN), CASE(LOGICAL_OR, SLOTS, BOOLEAN),
    CASE(LOGICAL_EQUAL_BOOL, SLOTS, BOOLEAN), CASE(LOGICAL_NOT_EQUAL_BOOL, SLOTS, BOOLEAN),
    COMPARE(LOGICAL_EQUAL), COMPARE(LOGICAL_NOT_EQUAL),
    COMPARE(LOGICAL_GREATER), COMPARE(LOGICAL_LESS),
    COMPARE(LOGICAL_GREATER_EQUAL), COMPARE(LOGICAL_LESS_EQUAL),
    CASE(LOGICAL_EQUAL_SIGNED_CONST, CONSTANT, SIGNED),
    CASE(LOGICAL_EQUAL_FLOAT, SLOTS, FLOATING), CASE(LOGICAL_NOT_EQUAL_FLOAT, SLOTS, FLOATING),
    CASE(LOGICAL_EQUAL_STRING, SLOTS, STRING), CASE(LOGICAL_NOT_EQUAL_STRING, SLOTS, STRING),
    CASE(ADD_STRING, SLOTS, STRING),
    CASE(BITWISE_NOT, UNARY, SIGNED), CASE(BITWISE_AND, SLOTS, SIGNED),
    CASE(BITWISE_OR, SLOTS, SIGNED), CASE(BITWISE_XOR, SLOTS, SIGNED),
    CASE(SHIFT_LEFT_INT, SLOTS, SIGNED), CASE(SHIFT_RIGHT_INT, SLOTS, SIGNED),
    CASE(BITWISE_SHIFT_LEFT, SLOTS, SIGNED), CASE(BITWISE_SHIFT_RIGHT, SLOTS, SIGNED),
    CASE(JUMP_IF_BOOL_FALSE, BRANCH, BOOLEAN),
    CASE(JUMP_IF_GREATER_SIGNED, BRANCH, SIGNED),
    CASE(JUMP_IF_LESS_EQUAL_SIGNED, BRANCH, SIGNED),
    CASE(JUMP_IF_NOT_EQUAL_SIGNED, BRANCH, SIGNED),
    CASE(JUMP_IF_NOT_EQUAL_SIGNED_CONST, CONST_BRANCH, SIGNED)
};

static TZrInstruction instruction(EZrInstructionCode op, TZrUInt16 dest,
                                  TZrUInt16 left, TZrUInt16 right) {
    TZrInstruction value = {0};
    value.instruction.operationCode = op;
    value.instruction.operandExtra = dest;
    value.instruction.operand.operand1[0] = left;
    value.instruction.operand.operand1[1] = right;
    return value;
}

/* A trace observer forces the shared dispatcher as well as the normal fast path. */
static TZrDebugSignal observe(SZrState *state, SZrFunction *function,
        const TZrInstruction *pc, TZrUInt32 offset, TZrUInt32 line, TZrPtr data) {
    (void)state; (void)function; (void)pc; (void)offset; (void)line;
    ++*(unsigned *)data;
    return ZR_DEBUG_SIGNAL_NONE;
}

static TZrBool execute_and_destroy(SZrState *state, const TZrInstruction *code,
        TZrSize codeCount, const SZrTypeValue *values, TZrSize valueCount, TZrBool observed) {
    SZrFunction *function = ZrCore_Function_New(state);
    SZrTypeValue result;
    TZrBool success;
    EZrThreadStatus status;
    unsigned observations = 0;
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrCore_Function_AcquireHostRoot(state, function));
    function->instructionsList = ZrCore_Memory_RawMallocWithType(state->global,
            codeCount * sizeof(*code), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    function->constantValueList = ZrCore_Memory_RawMallocWithType(state->global,
            valueCount * sizeof(*values), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    memcpy(function->instructionsList, code, codeCount * sizeof(*code));
    memcpy(function->constantValueList, values, valueCount * sizeof(*values));
    function->instructionsLength = codeCount;
    function->constantValueLength = valueCount;
    function->stackSize = 8;
    if (observed) ZrCore_Debug_SetTraceObserver(state, observe, &observations);
    success = ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &result);
    status = state->threadStatus;
    ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    if (!success) ZrCore_State_ResetThread(state, ZR_THREAD_STATUS_FINE);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    if (!success) TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, status);
    if (observed) TEST_ASSERT_GREATER_THAN_UINT(0, observations);
    return success;
}

static void check(const Case *test, unsigned badOperand, TZrBool observed) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeValue values[3];
    TZrInstruction code[4];
    TZrBool success;
    char context[96];
    TEST_ASSERT_NOT_NULL(state);
    for (unsigned i = 0; i < 3; ++i) {
        switch (test->kind) {
            case SIGNED: ZrCore_Value_InitAsInt(state, &values[i], 2); break;
            case UNSIGNED: ZrCore_Value_InitAsUInt(state, &values[i], 2); break;
            case FLOATING: ZrCore_Value_InitAsFloat(state, &values[i], 2.0); break;
            case BOOLEAN: ZrCore_Value_InitAsBool(state, &values[i], ZR_TRUE); break;
            case STRING: ZrCore_Value_InitAsRawObject(state, &values[i],
                    ZR_CAST_RAW_OBJECT_AS_SUPER(ZrCore_String_CreateFromNative(state, "text"))); break;
        }
    }
    if (badOperand < 3) ZrCore_Value_ResetAsNull(&values[badOperand]);
    code[0] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0, 0);
    code[1] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1, 0);
    code[2] = instruction(test->opcode, 2, 0, 1);
    if (test->layout == LOAD_CONST || test->layout == LOAD_STACK || test->layout == ADD_MOD) {
        code[2].instruction.operand.operand0[0] = 0;
        code[2].instruction.operand.operand0[1] = 1;
        code[2].instruction.operand.operand1[1] = test->layout == ADD_MOD ? 2 : 1;
    } else if (test->layout == LOAD_STACK_CONST || test->layout == LOAD_STACK_LOAD_CONST) {
        code[2].instruction.operand.operand0[0] = 0;
        code[2].instruction.operand.operand0[1] = 3;
        code[2].instruction.operand.operand0[2] = 4;
        code[2].instruction.operand.operand1[1] = 1;
    } else if (test->layout == BRANCH || test->layout == CONST_BRANCH) {
        code[2] = instruction(test->opcode, 0, 1, 0);
    }
    code[3] = instruction(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, 2, 0);
    success = execute_and_destroy(state, code, ZR_ARRAY_COUNT(code), values, ZR_ARRAY_COUNT(values), observed);
    snprintf(context, sizeof(context), "%s, bad operand %u", test->name, badOperand);
    TEST_ASSERT_EQUAL_INT_MESSAGE(badOperand == 3, success, context);
}

static void test_typed_opcodes_reject_invalid_runtime_values(void) {
    for (unsigned observed = 0; observed < 2; ++observed) {
        for (unsigned i = 0; i < ZR_ARRAY_COUNT(cases); ++i) {
            check(&cases[i], 0, observed);
            if (cases[i].layout != UNARY && cases[i].opcode != ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE))
                check(&cases[i], 1, observed);
            if (cases[i].layout == ADD_MOD) check(&cases[i], 2, observed);
            check(&cases[i], 3, observed);
        }
    }
}

static void test_signed_comparisons_reject_unsigned_operands(void) {
    static const EZrInstructionCode opcodes[] = {
        ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED),
        ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED),
        ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED)
    };
    for (unsigned observed = 0u; observed < 2u; ++observed) {
        for (unsigned op = 0u; op < ZR_ARRAY_COUNT(opcodes); ++op) {
            for (unsigned operand = 0u; operand < 2u; ++operand) {
                SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
                SZrTypeValue values[2];
                TZrInstruction code[4];
                TEST_ASSERT_NOT_NULL(state);
                ZrCore_Value_InitAsInt(state, &values[0], 2);
                ZrCore_Value_InitAsInt(state, &values[1], 3);
                ZrCore_Value_InitAsUInt(state, &values[operand], UINT64_MAX);
                code[0] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0, 0);
                code[1] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1, 0);
                code[2] = instruction(opcodes[op], 2, 0, 1);
                code[3] = instruction(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, 2, 0);
                TEST_ASSERT_FALSE(execute_and_destroy(state, code, 4u, values, 2u, observed));
            }
        }
    }
}

static SZrObject *array_receiver(SZrState *state, SZrObject **outItems, TZrBool wrongElement) {
    SZrObject *receiver = ZrCore_Object_New(state, ZR_NULL);
    SZrObject *items = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    SZrTypeValue key, value, receiverValue, result;
    TEST_ASSERT_NOT_NULL(receiver);
    TEST_ASSERT_NOT_NULL(items);
    receiver->prototype = state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_ARRAY];
    ZrCore_Object_Init(state, receiver);
    ZrCore_Object_Init(state, items);
    ZrCore_Value_InitAsRawObject(state, &key,
            ZR_CAST_RAW_OBJECT_AS_SUPER(ZrCore_String_CreateFromNative(state, "__zr_items")));
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(items));
    value.type = ZR_VALUE_TYPE_ARRAY;
    ZrCore_Object_SetValue(state, receiver, &key, &value);
    receiver->cachedHiddenItemsObject = items;
    ZrCore_Value_InitAsRawObject(state, &receiverValue, ZR_CAST_RAW_OBJECT_AS_SUPER(receiver));
    ZrCore_Value_InitAsInt(state, &value, 2);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayAddInt(state, &receiverValue, &value, &result));
    /* Exercise the node-backed path too, where element types are runtime data. */
    ZrCore_Value_InitAsInt(state, &key, 0);
    TEST_ASSERT_NOT_NULL(ZrCore_Object_GetValue(state, items, &key));
    if (wrongElement) {
        ZrCore_Value_InitAsRawObject(state, &value,
                ZR_CAST_RAW_OBJECT_AS_SUPER(ZrCore_String_CreateFromNative(state, "wrong element")));
        ZrCore_Object_SetValue(state, items, &key, &value);
    }
    *outItems = items;
    return receiver;
}

typedef struct ArrayCase {
    EZrInstructionCode opcode;
    TZrBool boundItems, store, plain;
    const char *name;
} ArrayCase;
#define ARRAY_CASE(NAME, BOUND, STORE, PLAIN) {ZR_INSTRUCTION_ENUM(NAME), BOUND, STORE, PLAIN, #NAME}
static const ArrayCase arrayCases[] = {
    ARRAY_CASE(SUPER_ARRAY_GET_INT, 0, 0, 0),
    ARRAY_CASE(SUPER_ARRAY_GET_INT_ITEMS, 1, 0, 0),
    ARRAY_CASE(SUPER_ARRAY_GET_INT_PLAIN_DEST, 0, 0, 1),
    ARRAY_CASE(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST, 1, 0, 1),
    ARRAY_CASE(SUPER_ARRAY_SET_INT, 0, 1, 0),
    ARRAY_CASE(SUPER_ARRAY_SET_INT_ITEMS, 1, 1, 0)
};

/* Bad values: receiver, index, stored value, element type, plain destination.
 * All offsets are in bounds, so the loader's structural verifier cannot reject them. */
static void check_array(const ArrayCase *test, unsigned badOperand, TZrBool observed) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrObject *receiver, *items;
    SZrTypeValue values[4];
    TZrInstruction code[6];
    TZrBool success;
    char context[100];
    TEST_ASSERT_NOT_NULL(state);
    receiver = array_receiver(state, &items, badOperand == 3);
    ZrCore_Value_InitAsRawObject(state, &values[0], ZR_CAST_RAW_OBJECT_AS_SUPER(test->boundItems ? items : receiver));
    ZrCore_Value_InitAsInt(state, &values[1], 0);
    ZrCore_Value_InitAsInt(state, &values[2], 2);
    ZrCore_Value_ResetAsNull(&values[3]);
    if (badOperand < 3) ZrCore_Value_ResetAsNull(&values[badOperand]);
    if (badOperand == 4) ZrCore_Value_InitAsRawObject(state, &values[3], ZR_CAST_RAW_OBJECT_AS_SUPER(receiver));
    for (unsigned i = 0; i < 4; ++i)
        code[i] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), i, i, 0);
    code[4] = instruction(test->opcode, test->store ? 2 : 3, 0, 1);
    code[5] = instruction(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, 3, 0);
    success = execute_and_destroy(state, code, ZR_ARRAY_COUNT(code), values, ZR_ARRAY_COUNT(values), observed);
    snprintf(context, sizeof(context), "%s, bad operand %u", test->name, badOperand);
    TEST_ASSERT_EQUAL_INT_MESSAGE(badOperand == 5, success, context);
}

static void test_typed_array_opcodes_reject_invalid_runtime_values(void) {
    for (unsigned observed = 0; observed < 2; ++observed) {
        for (unsigned i = 0; i < ZR_ARRAY_COUNT(arrayCases); ++i) {
            check_array(&arrayCases[i], 0, observed);
            check_array(&arrayCases[i], 1, observed);
            if (arrayCases[i].store) check_array(&arrayCases[i], 2, observed);
            check_array(&arrayCases[i], 3, observed);
            if (arrayCases[i].plain) check_array(&arrayCases[i], 4, observed);
            check_array(&arrayCases[i], 5, observed);
        }
    }
}

static void test_typed_array_bind_and_append_reject_invalid_runtime_values(void) {
    const EZrInstructionCode opcodes[] = {
        ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS), ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT),
        ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4), ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST),
        ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST)
    };
    for (unsigned observed = 0; observed < 2; ++observed) {
        for (unsigned op = 0; op < ZR_ARRAY_COUNT(opcodes); ++op) {
            for (unsigned badOperand = 0; badOperand < 4; ++badOperand) {
                if ((op == 0 && badOperand != 0 && badOperand != 3) ||
                    (op != 4 && badOperand == 2)) continue;
                SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
                SZrTypeValue values[6];
                TZrInstruction code[8];
                TZrBool success;
                char context[80];
                TEST_ASSERT_NOT_NULL(state);
                for (unsigned i = 0; i < 4; ++i) {
                    SZrObject *items;
                    SZrObject *receiver = array_receiver(state, &items, ZR_FALSE);
                    ZrCore_Value_InitAsRawObject(state, &values[i], ZR_CAST_RAW_OBJECT_AS_SUPER(receiver));
                }
                ZrCore_Value_InitAsInt(state, &values[4], 1);
                ZrCore_Value_InitAsInt(state, &values[5], 2);
                if (badOperand == 0) ZrCore_Value_ResetAsNull(&values[0]);
                if (badOperand == 1) ZrCore_Value_ResetAsNull(&values[op == 3 ? 5 : 4]);
                if (badOperand == 2) ZrCore_Value_ResetAsNull(&values[5]);
                for (unsigned i = 0; i < 6; ++i)
                    code[i] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), i, i, 0);
                code[6] = instruction(opcodes[op], op == 4 ? 5 : 6, 0, op == 0 ? 0 : (op == 3 ? 5 : 4));
                code[7] = instruction(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, 6, 0);
                success = execute_and_destroy(state, code, ZR_ARRAY_COUNT(code), values, ZR_ARRAY_COUNT(values), observed);
                snprintf(context, sizeof(context), "array opcode %u, bad operand %u", (unsigned)opcodes[op], badOperand);
                TEST_ASSERT_EQUAL_INT_MESSAGE(badOperand == 3, success, context);
            }
        }
    }
}

static void test_call_opcodes_reject_null_callables(void) {
    const EZrInstructionCode opcodes[] = {
        ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL), ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL),
        ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS), ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS),
        ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS), ZR_INSTRUCTION_ENUM(FUNCTION_CALL),
        ZR_INSTRUCTION_ENUM(FUNCTION_CALL_SPREAD), ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS),
        ZR_INSTRUCTION_ENUM(DYN_CALL), ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL),
        ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL), ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS),
        ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS), ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS),
        ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS), ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL)
    };
    for (unsigned observed = 0; observed < 2; ++observed) {
        for (unsigned i = 0; i < ZR_ARRAY_COUNT(opcodes); ++i) {
            SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
            SZrTypeValue value;
            TZrInstruction code[3];
            TEST_ASSERT_NOT_NULL(state);
            ZrCore_Value_ResetAsNull(&value);
            code[0] = instruction(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0, 0);
            code[1] = instruction(opcodes[i], 2, 0, 0);
            code[2] = instruction(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, 2, 0);
            TEST_ASSERT_FALSE(execute_and_destroy(state, code, ZR_ARRAY_COUNT(code), &value, 1, observed));
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_opcodes_reject_invalid_runtime_values);
    RUN_TEST(test_signed_comparisons_reject_unsigned_operands);
    RUN_TEST(test_typed_array_opcodes_reject_invalid_runtime_values);
    RUN_TEST(test_typed_array_bind_and_append_reject_invalid_runtime_values);
    RUN_TEST(test_call_opcodes_reject_null_callables);
    return UNITY_END();
}
