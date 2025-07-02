//
// Created by HeJiahui on 2025/6/15.
//

#include "zr_vm_core/execution.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/convertion.h"
#include "zr_vm_core/state.h"

void ZrExecute(SZrState *state, SZrCallInfo *callInfo) {
    SZrClosure *closure;
    SZrTypeValue *constants;
    TZrStackValuePointer base;
    const TZrInstruction *programCounter;
    TZrDebugSignal trap;
    ZR_INSTRUCTION_DISPATCH_TABLE
#define DONE ZR_INSTRUCTION_DONE(instruction, programCounter)
LZrStart:
    trap = state->debugHookSignal;
LZrReturning:
    closure = ZR_CAST_VM_CLOSURE(state, ZrStackGetValue(callInfo->functionBase.valuePointer));
    constants = closure->function->constantVariableList;
    programCounter = callInfo->context.context.programCounter;
    if (ZR_UNLIKELY(trap != ZR_DEBUG_SIGNAL_NONE)) {
        // todo
    }
    base = callInfo->functionBase.valuePointer + 1;
    for (;;) {
        TZrInstruction instruction;
        /*
         * fetch instruction
         */
        ZR_INSTRUCTION_FETCH(instruction, programCounter);

        // debug line
#if ZR_DEBUG
#endif
        ZR_ASSERT(base == callInfo->functionBase.valuePointer + 1);
        ZR_ASSERT(
            base <= state->stackTop.valuePointer && state->stackTop.valuePointer <= state->stackTail.valuePointer);
        ZR_INSTRUCTION_DISPATCH(instruction) {
        ZR_INSTRUCTION_LABEL(MOVE) {
            }
            DONE;
        ZR_INSTRUCTION_LABEL(LOAD_CONSTANT) {
            }
            DONE;
        ZR_INSTRUCTION_LABEL(ADD) {
            }
            DONE;
        }
    }

#undef DONE
}
