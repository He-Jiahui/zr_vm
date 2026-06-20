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

static void init_type_ref(SZrFunctionTypedTypeRef *typeRef, EZrValueType baseType) {
    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = baseType;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->staticCType = ZR_STATIC_C_TYPE_DYNAMIC;
    typeRef->staticCTypeId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
}

static SZrFunction *create_conflicting_typed_scalar_function(SZrState *state) {
    SZrFunction *function;
    TZrInstruction *instructions;
    SZrFunctionTypedLocalBinding *bindings;

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
    bindings = (SZrFunctionTypedLocalBinding *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedLocalBinding) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (instructions == ZR_NULL || bindings == ZR_NULL) {
        if (instructions != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          instructions,
                                          sizeof(TZrInstruction),
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (bindings != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          bindings,
                                          sizeof(SZrFunctionTypedLocalBinding) * 2u,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(instructions, 0, sizeof(*instructions));
    instructions[0].instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(ADD_SIGNED);
    instructions[0].instruction.operandExtra = 2u;
    instructions[0].instruction.operand.operand1[0] = 0u;
    instructions[0].instruction.operand.operand1[1] = 1u;

    ZrCore_Memory_RawSet(bindings, 0, sizeof(SZrFunctionTypedLocalBinding) * 2u);
    bindings[0].stackSlot = 0u;
    init_type_ref(&bindings[0].type, ZR_VALUE_TYPE_INT64);
    bindings[1].stackSlot = 0u;
    init_type_ref(&bindings[1].type, ZR_VALUE_TYPE_DOUBLE);

    function->instructionsList = instructions;
    function->instructionsLength = 1u;
    function->stackSize = 3u;
    function->typedLocalBindings = bindings;
    function->typedLocalBindingLength = 2u;
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

static void test_typed_scalar_slot_type_conflict_becomes_dynamic_deopt_boundary(void) {
    SZrState *state = create_test_state();
    SZrFunction *function;
    const SZrSemIrInstruction *instruction;

    TEST_ASSERT_NOT_NULL(state);
    function = create_conflicting_typed_scalar_function(state);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(compiler_build_function_semir_metadata(state, function));
    instruction = find_semir_opcode(function, ZR_SEMIR_OPCODE_DYN_ARITHMETIC);
    TEST_ASSERT_NOT_NULL(instruction);
    TEST_ASSERT_NULL(find_semir_opcode(function, ZR_SEMIR_OPCODE_ADD));

    TEST_ASSERT_EQUAL_UINT32(1u, function->semIrInstructionLength);
    TEST_ASSERT_EQUAL_UINT32(1u, function->semIrDeoptTableLength);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMIR_EFFECT_KIND_DYNAMIC_RUNTIME,
                             function->semIrEffectTable[instruction->effectTableIndex].kind);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_RUNTIME_SEMIR_DEOPT_ID_NONE, instruction->deoptId);
    TEST_ASSERT_EQUAL_UINT32(instruction->deoptId, function->semIrDeoptTable[0].deoptId);
    TEST_ASSERT_EQUAL_UINT32(0u, function->semIrDeoptTable[0].execInstructionIndex);
    TEST_ASSERT_EQUAL_UINT32(2u, instruction->destinationSlot);
    TEST_ASSERT_EQUAL_UINT32(0u, instruction->operand0);
    TEST_ASSERT_EQUAL_UINT32(1u, instruction->operand1);
    TEST_ASSERT_EQUAL_UINT32(ZR_STATIC_C_TYPE_DYNAMIC,
                             function->semIrTypeTable[instruction->typeTableIndex].staticCType);

    ZrCore_Function_Free(state, function);
    destroy_test_state(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_scalar_slot_type_conflict_becomes_dynamic_deopt_boundary);
    return UNITY_END();
}
