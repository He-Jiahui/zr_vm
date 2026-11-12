#include "compiler/compiler_internal.h"
#include "harness/runtime_support.h"

#include <stdio.h>
#include <stdlib.h>

static int failures;

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "fused add packing failed at line %d: %s\n", __LINE__, #condition);                        \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)

static SZrCompilerState *create_compiler(SZrState *state) {
    SZrCompilerState *compiler = (SZrCompilerState *)malloc(sizeof(SZrCompilerState));
    if (compiler == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_CompilerState_Init(compiler, state);
    compiler->maxStackSlotCount = 4;
    return compiler;
}

static void push_local_at_slot(SZrState *state, SZrCompilerState *compiler, TZrUInt32 stackSlot) {
    SZrFunctionLocalVariable local = {0};
    SZrFunctionExecutionLocationInfo location = {0};

    local.stackSlot = stackSlot;
    ZrCore_Array_Push(state, &compiler->localVars, &local);
    ZrCore_Array_Push(state, &compiler->executionLocations, &location);
}

static void test_dense_compact_still_moves_unrelated_slots(SZrState *state) {
    SZrCompilerState *compiler = create_compiler(state);
    TZrInstruction instruction = {0};
    SZrFunctionLocalVariable *local;

    CHECK(compiler != ZR_NULL);
    if (compiler == ZR_NULL) {
        return;
    }

    instruction.instruction.operationCode = ZR_INSTRUCTION_ENUM(GET_STACK);
    instruction.instruction.operandExtra = 1;
    ZrCore_Array_Push(state, &compiler->instructions, &instruction);
    push_local_at_slot(state, compiler, 3);

    optimize_instructions(compiler);

    local = (SZrFunctionLocalVariable *)ZrCore_Array_Get(&compiler->localVars, 0);
    CHECK(local != ZR_NULL && local->stackSlot == 0);

    ZrParser_CompilerState_Free(compiler);
    free(compiler);
}

static void test_optimizer_preserves_overlapping_constant_index(SZrState *state) {
    SZrCompilerState *compiler = create_compiler(state);
    TZrInstruction instruction = {0};
    TZrInstruction *stored;
    SZrFunctionLocalVariable *local;
    TZrUInt8 storedSlot;

    CHECK(compiler != ZR_NULL);
    if (compiler == ZR_NULL) {
        return;
    }

    instruction.instruction.operationCode = ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST);
    instruction.instruction.operand.operand1[1] = 2;
    storedSlot = instruction.instruction.operand.operand0[2];
    ZrCore_Array_Push(state, &compiler->instructions, &instruction);
    push_local_at_slot(state, compiler, 3);

    optimize_instructions(compiler);

    stored = (TZrInstruction *)ZrCore_Array_Get(&compiler->instructions, 0);
    local = (SZrFunctionLocalVariable *)ZrCore_Array_Get(&compiler->localVars, 0);
    CHECK(stored != ZR_NULL);
    CHECK(local != ZR_NULL);
    if (stored != ZR_NULL) {
        CHECK(stored->instruction.operand.operand1[1] == 2);
        CHECK(stored->instruction.operand.operand0[2] == storedSlot);
        CHECK(ZR_INSTRUCTION_LOAD_STACK_LOAD_CONST_SLOT_FITS(stored->instruction.operand.operand0[2],
                                                             stored->instruction.operand.operand1[1]));
    }
    if (local != ZR_NULL) {
        CHECK(local->stackSlot == 3);
    }

    ZrParser_CompilerState_Free(compiler);
    free(compiler);
}

int main(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);

    CHECK(state != ZR_NULL);
    if (state != ZR_NULL) {
        test_dense_compact_still_moves_unrelated_slots(state);
        test_optimizer_preserves_overlapping_constant_index(state);
        ZrTests_Runtime_State_Destroy(state);
    }

    return failures != 0;
}
