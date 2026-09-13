// Interpreter dispatch boundary state.
//
// The dispatch loop may keep these values in registers, but the call-info and
// state fields remain the GC-visible authority.  Boundary helpers are kept
// deliberately small so they can be introduced before the dispatch loop is
// rewired to use them.
#ifndef ZR_VM_CORE_EXECUTION_CONTEXT_H
#define ZR_VM_CORE_EXECUTION_CONTEXT_H

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/state.h"

typedef enum EZrExecutionBoundaryStatus {
    ZR_EXECUTION_BOUNDARY_OK = 0,
    ZR_EXECUTION_BOUNDARY_INVALID_ARGUMENT,
    ZR_EXECUTION_BOUNDARY_INVALID_FRAME,
    ZR_EXECUTION_BOUNDARY_INVALID_FUNCTION,
    ZR_EXECUTION_BOUNDARY_INVALID_PROGRAM_COUNTER,
    ZR_EXECUTION_BOUNDARY_INVALID_STACK,
    ZR_EXECUTION_BOUNDARY_TERMINATED
} EZrExecutionBoundaryStatus;

typedef struct SZrExecutionContext {
    SZrState *state;
    SZrCallInfo *callInfo;
    SZrFunction *function;
    const TZrInstruction *programCounter;
    const TZrInstruction *instructionsBegin;
    const TZrInstruction *instructionsEnd;
    TZrStackValuePointer frameBase;
    TZrStackValuePointer frameTop;
    TZrStackValuePointer stackTop;
    struct SZrGcDomain *domain;
    SZrProfileRuntime *profileRuntime;
} SZrExecutionContext;

/* Draft plan terminology retained as a source-compatible alias. */
typedef SZrExecutionContext SZrExecutionLocalContext;

ZR_CORE_API void ZrCore_ExecutionContext_Init(SZrExecutionContext *context);

/* Publish the continuation PC and root-visible stack state before a boundary. */
ZR_CORE_API EZrExecutionBoundaryStatus ZrCore_Execution_PublishBoundary(
        SZrExecutionContext *context,
        SZrState *state,
        SZrCallInfo *callInfo,
        const TZrInstruction *resumeProgramCounter);

/* Rebuild register state from the current GC-visible call frame. */
ZR_CORE_API EZrExecutionBoundaryStatus ZrCore_Execution_ReloadBoundary(
        SZrExecutionContext *context,
        SZrState *state);

ZR_CORE_API const TZrChar *ZrCore_Execution_BoundaryStatusName(
        EZrExecutionBoundaryStatus status);

/* Poll the mutator and reload only after a pause/collection boundary. */
ZR_CORE_API EZrExecutionBoundaryStatus ZrCore_Execution_SafepointPoll(
        SZrExecutionContext *context,
        SZrState *state,
        const TZrInstruction *resumeProgramCounter);

#endif
