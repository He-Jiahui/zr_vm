#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"

static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0) {
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }

    if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
        return realloc(pointer, newSize);
    }

    return malloc(newSize);
}

static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0, &callbacks);
    if (global == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    return global->mainThreadState;
}

static void destroy_test_state(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
}

static SZrFunction *compile_typed_scalar_fixture(SZrState *state) {
    const char *source =
            "var left: int = 21;\n"
            "var right: int = 5;\n"
            "var sum: int = left + right;\n"
            "var delta: int = sum - right;\n"
            "var product: int = sum * delta;\n"
            "var quotient: int = product / right;\n"
            "var remainder: int = product % right;\n"
            "var isBigger: bool = quotient > remainder;\n"
            "var inverted: int = ~right;\n"
            "var masked: int = left & right;\n"
            "var joined: int = masked | delta;\n"
            "var toggled: int = joined ^ remainder;\n"
            "var shiftedLeft: int = toggled << right;\n"
            "var shiftedRight: int = shiftedLeft >> right;\n"
            "return shiftedRight + inverted;";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, "semir_typed_opcode_guardrails.zr", 32);
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static TZrUInt32 function_count_semir_opcode(const SZrFunction *function, EZrSemIrOpcode opcode) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0u; index < function->semIrInstructionLength; index++) {
        if ((EZrSemIrOpcode)function->semIrInstructions[index].opcode == opcode) {
            count++;
        }
    }

    return count;
}

static const SZrSemIrInstruction *function_find_semir_opcode(const SZrFunction *function, EZrSemIrOpcode opcode) {
    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0u; index < function->semIrInstructionLength; index++) {
        const SZrSemIrInstruction *instruction = &function->semIrInstructions[index];
        if ((EZrSemIrOpcode)instruction->opcode == opcode) {
            return instruction;
        }
    }

    return ZR_NULL;
}

static TZrBool function_has_generic_exec_arithmetic_opcode(const SZrFunction *function) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        switch ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode) {
            case ZR_INSTRUCTION_ENUM(ADD):
            case ZR_INSTRUCTION_ENUM(SUB):
            case ZR_INSTRUCTION_ENUM(MUL):
            case ZR_INSTRUCTION_ENUM(DIV):
            case ZR_INSTRUCTION_ENUM(MOD):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
                return ZR_TRUE;
            default:
                break;
        }
    }

    return ZR_FALSE;
}

static void assert_semir_opcode_static_type(const SZrFunction *function,
                                            EZrSemIrOpcode opcode,
                                            EZrStaticCType expectedStaticType) {
    const SZrSemIrInstruction *instruction = function_find_semir_opcode(function, opcode);

    TEST_ASSERT_NOT_NULL(instruction);
    TEST_ASSERT_NOT_NULL(function->semIrTypeTable);
    TEST_ASSERT_LESS_THAN_UINT32(function->semIrTypeTableLength, instruction->typeTableIndex);
    TEST_ASSERT_EQUAL_UINT32(expectedStaticType, function->semIrTypeTable[instruction->typeTableIndex].staticCType);
}

static void test_typed_numeric_function_emits_scalar_semir_without_generic_exec_arithmetic(void) {
    SZrState *state = create_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_typed_scalar_fixture(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_FALSE(function_has_generic_exec_arithmetic_opcode(function));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_ADD));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_SUB));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_MUL));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_DIV));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_MOD));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_GT));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_BIT_NOT));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_BIT_AND));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_BIT_OR));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_BIT_XOR));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_SHL));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, function_count_semir_opcode(function, ZR_SEMIR_OPCODE_SHR));

    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_ADD, ZR_STATIC_C_TYPE_I64);
    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_SUB, ZR_STATIC_C_TYPE_I64);
    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_MUL, ZR_STATIC_C_TYPE_I64);
    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_DIV, ZR_STATIC_C_TYPE_I64);
    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_MOD, ZR_STATIC_C_TYPE_I64);
    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_GT, ZR_STATIC_C_TYPE_BOOL);
    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_BIT_NOT, ZR_STATIC_C_TYPE_I64);
    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_BIT_AND, ZR_STATIC_C_TYPE_I64);
    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_BIT_OR, ZR_STATIC_C_TYPE_I64);
    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_BIT_XOR, ZR_STATIC_C_TYPE_I64);
    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_SHL, ZR_STATIC_C_TYPE_I64);
    assert_semir_opcode_static_type(function, ZR_SEMIR_OPCODE_SHR, ZR_STATIC_C_TYPE_I64);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_numeric_function_emits_scalar_semir_without_generic_exec_arithmetic);
    return UNITY_END();
}
