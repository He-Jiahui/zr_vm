#include <stdlib.h>

#include "unity.h"

#include "compiler_internal.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

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

static SZrFunction *create_single_member_instruction_function(SZrState *state,
                                                             EZrInstructionCode opcode,
                                                             TZrUInt32 valueOrDestinationSlot,
                                                             TZrUInt32 receiverSlot,
                                                             TZrUInt32 memberEntryIndex) {
    SZrFunction *function;
    TZrInstruction *instructions;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    instructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                    sizeof(TZrInstruction),
                                                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (instructions == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(instructions, 0, sizeof(*instructions));
    instructions[0].instruction.operationCode = (TZrUInt16)opcode;
    instructions[0].instruction.operandExtra = (TZrUInt16)valueOrDestinationSlot;
    instructions[0].instruction.operand.operand1[0] = (TZrUInt16)receiverSlot;
    instructions[0].instruction.operand.operand1[1] = (TZrUInt16)memberEntryIndex;

    function->instructionsList = instructions;
    function->instructionsLength = 1u;
    function->stackSize = 5u;
    return function;
}

static const SZrSemIrInstruction *find_semir_opcode(const SZrFunction *function, EZrSemIrOpcode opcode) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0u; index < function->semIrInstructionLength; index++) {
        if ((EZrSemIrOpcode)function->semIrInstructions[index].opcode == opcode) {
            return &function->semIrInstructions[index];
        }
    }

    return ZR_NULL;
}

static void assert_dynamic_member_boundary(EZrInstructionCode execOpcode,
                                           EZrSemIrOpcode expectedSemIrOpcode,
                                           TZrUInt32 valueOrDestinationSlot,
                                           TZrUInt32 receiverSlot,
                                           TZrUInt32 memberEntryIndex) {
    SZrState *state = create_test_state();
    SZrFunction *function;
    const SZrSemIrInstruction *instruction;

    TEST_ASSERT_NOT_NULL(state);
    function = create_single_member_instruction_function(state,
                                                        execOpcode,
                                                        valueOrDestinationSlot,
                                                        receiverSlot,
                                                        memberEntryIndex);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(compiler_build_function_semir_metadata(state, function));
    instruction = find_semir_opcode(function, expectedSemIrOpcode);
    TEST_ASSERT_NOT_NULL(instruction);

    TEST_ASSERT_EQUAL_UINT32(1u, function->semIrInstructionLength);
    TEST_ASSERT_EQUAL_UINT32(1u, function->semIrDeoptTableLength);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME,
                             function->semIrEffectTable[instruction->effectTableIndex].kind);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_RUNTIME_SEMIR_DEOPT_ID_NONE, instruction->deoptId);
    TEST_ASSERT_EQUAL_UINT32(0u, function->semIrDeoptTable[0].execInstructionIndex);
    TEST_ASSERT_EQUAL_UINT32(valueOrDestinationSlot, instruction->destinationSlot);
    TEST_ASSERT_EQUAL_UINT32(receiverSlot, instruction->operand0);
    TEST_ASSERT_EQUAL_UINT32(memberEntryIndex, instruction->operand1);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

static void test_generic_member_access_exec_opcodes_become_dynamic_deopt_boundaries(void) {
    assert_dynamic_member_boundary(ZR_INSTRUCTION_ENUM(GET_MEMBER), ZR_SEMIR_OPCODE_META_GET, 3u, 1u, 2u);
    assert_dynamic_member_boundary(ZR_INSTRUCTION_ENUM(SET_MEMBER), ZR_SEMIR_OPCODE_META_SET, 4u, 1u, 2u);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_generic_member_access_exec_opcodes_become_dynamic_deopt_boundaries);
    return UNITY_END();
}
