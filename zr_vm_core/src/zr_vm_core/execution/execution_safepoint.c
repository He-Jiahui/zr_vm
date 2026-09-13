#include "execution_context.h"

#include <stdint.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc_domain.h"

static TZrBool execution_context_pc_is_valid(const SZrFunction *function,
                                             const TZrInstruction *programCounter) {
    uintptr_t begin;
    uintptr_t end;
    uintptr_t pc;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL ||
        function->instructionsLength == 0u || programCounter == ZR_NULL) {
        return ZR_FALSE;
    }

    begin = (uintptr_t)(const void *)function->instructionsList;
    end = begin + ((uintptr_t)function->instructionsLength * sizeof(TZrInstruction));
    pc = (uintptr_t)(const void *)programCounter;
    return (TZrBool)(pc >= begin && pc <= end &&
                     ((pc - begin) % sizeof(TZrInstruction)) == 0u);
}

static TZrBool execution_context_stack_pointer_is_valid(const SZrState *state,
                                                         const TZrStackValuePointer pointer) {
    uintptr_t base;
    uintptr_t tail;
    uintptr_t value;

    if (pointer == ZR_NULL) {
        return ZR_FALSE;
    }
    /* Uninitialized fixtures may omit stack bounds; frame validity still gets
       checked through callInfo below.  A live state with bounds gets strict
       containment checks. */
    if (state == ZR_NULL || state->stackBase.valuePointer == ZR_NULL ||
        state->stackTail.valuePointer == ZR_NULL) {
        return ZR_TRUE;
    }
    base = (uintptr_t)(const void *)state->stackBase.valuePointer;
    tail = (uintptr_t)(const void *)state->stackTail.valuePointer;
    value = (uintptr_t)(const void *)pointer;
    return (TZrBool)(value >= base && value <= tail &&
                     ((value - base) % sizeof(SZrTypeValueOnStack)) == 0u);
}

void ZrCore_ExecutionContext_Init(SZrExecutionContext *context) {
    if (context != ZR_NULL) {
        ZrCore_Memory_RawSet(context, 0, sizeof(*context));
    }
}

EZrExecutionBoundaryStatus ZrCore_Execution_PublishBoundary(
        SZrExecutionContext *context,
        SZrState *state,
        SZrCallInfo *callInfo,
        const TZrInstruction *resumeProgramCounter) {
    SZrFunction *function;
    TZrStackValuePointer frameBase;

    if (context == ZR_NULL || state == ZR_NULL || callInfo == ZR_NULL) {
        return ZR_EXECUTION_BOUNDARY_INVALID_ARGUMENT;
    }
    function = callInfo->metadataFunction != ZR_NULL
             ? callInfo->metadataFunction
             : ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
    if (function == ZR_NULL) {
        return ZR_EXECUTION_BOUNDARY_INVALID_FUNCTION;
    }
    if (!execution_context_pc_is_valid(function, resumeProgramCounter)) {
        return ZR_EXECUTION_BOUNDARY_INVALID_PROGRAM_COUNTER;
    }
    if (callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_EXECUTION_BOUNDARY_INVALID_FRAME;
    }
    frameBase = callInfo->functionBase.valuePointer + 1;
    if (!execution_context_stack_pointer_is_valid(state, frameBase) ||
        !execution_context_stack_pointer_is_valid(state, callInfo->functionTop.valuePointer) ||
        !execution_context_stack_pointer_is_valid(state, state->stackTop.valuePointer)) {
        return ZR_EXECUTION_BOUNDARY_INVALID_STACK;
    }

    /* Publish roots first.  No local cache is used after this point until the
       caller either continues or explicitly reloads. */
    callInfo->context.context.programCounter = resumeProgramCounter;
    context->state = state;
    context->callInfo = callInfo;
    context->function = function;
    context->programCounter = resumeProgramCounter;
    context->instructionsBegin = function->instructionsList;
    context->instructionsEnd = function->instructionsList + function->instructionsLength;
    context->frameBase = frameBase;
    context->frameTop = callInfo->functionTop.valuePointer;
    context->stackTop = state->stackTop.valuePointer;
    context->domain = state->gcDomain;
    context->profileRuntime = ZrCore_Profile_FromState(state);
    if (resumeProgramCounter < context->instructionsEnd) {
        state->previousProgramCounter = resumeProgramCounter - context->instructionsBegin;
    }
    return ZR_EXECUTION_BOUNDARY_OK;
}

EZrExecutionBoundaryStatus ZrCore_Execution_ReloadBoundary(
        SZrExecutionContext *context,
        SZrState *state) {
    SZrCallInfo *callInfo;
    SZrFunction *function;
    const TZrInstruction *programCounter;
    TZrStackValuePointer frameBase;

    if (context == ZR_NULL || state == ZR_NULL) {
        return ZR_EXECUTION_BOUNDARY_INVALID_ARGUMENT;
    }
    callInfo = state->callInfoList;
    if (callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_EXECUTION_BOUNDARY_INVALID_FRAME;
    }
    function = callInfo->metadataFunction != ZR_NULL
             ? callInfo->metadataFunction
             : ZrCore_Closure_GetMetadataFunctionFromCallInfo(state, callInfo);
    if (function == ZR_NULL) {
        return ZR_EXECUTION_BOUNDARY_INVALID_FUNCTION;
    }
    programCounter = callInfo->context.context.programCounter;
    if (!execution_context_pc_is_valid(function, programCounter)) {
        return ZR_EXECUTION_BOUNDARY_INVALID_PROGRAM_COUNTER;
    }
    frameBase = callInfo->functionBase.valuePointer + 1;
    if (!execution_context_stack_pointer_is_valid(state, frameBase) ||
        !execution_context_stack_pointer_is_valid(state, callInfo->functionTop.valuePointer) ||
        !execution_context_stack_pointer_is_valid(state, state->stackTop.valuePointer)) {
        return ZR_EXECUTION_BOUNDARY_INVALID_STACK;
    }

    context->state = state;
    context->callInfo = callInfo;
    context->function = function;
    context->programCounter = programCounter;
    context->instructionsBegin = function->instructionsList;
    context->instructionsEnd = function->instructionsList + function->instructionsLength;
    context->frameBase = frameBase;
    context->frameTop = callInfo->functionTop.valuePointer;
    context->stackTop = state->stackTop.valuePointer;
    context->domain = state->gcDomain;
    context->profileRuntime = ZrCore_Profile_FromState(state);
    return ZR_EXECUTION_BOUNDARY_OK;
}

EZrExecutionBoundaryStatus ZrCore_Execution_SafepointPoll(
        SZrExecutionContext *context,
        SZrState *state,
        const TZrInstruction *resumeProgramCounter) {
    EZrExecutionBoundaryStatus status;

    status = ZrCore_Execution_PublishBoundary(context, state, state != ZR_NULL ? state->callInfoList : ZR_NULL,
                                               resumeProgramCounter);
    if (status != ZR_EXECUTION_BOUNDARY_OK) {
        return status;
    }
    if (state->gcDomain != ZR_NULL && ZrCore_GcDomain_MutatorPoll(state)) {
        return ZrCore_Execution_ReloadBoundary(context, state);
    }
    return ZR_EXECUTION_BOUNDARY_OK;
}
