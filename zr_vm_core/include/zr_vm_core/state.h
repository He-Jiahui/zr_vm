//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_STATE_H
#define ZR_VM_CORE_STATE_H
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/stack.h"

struct SZrGlobalState;

typedef enum EZrVmExceptionHandlerPhase {
    ZR_VM_EXCEPTION_HANDLER_PHASE_TRY = 0,
    ZR_VM_EXCEPTION_HANDLER_PHASE_CATCH,
    ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY
} EZrVmExceptionHandlerPhase;

typedef enum EZrVmPendingControlKind {
    ZR_VM_PENDING_CONTROL_NONE = 0,
    ZR_VM_PENDING_CONTROL_EXCEPTION,
    ZR_VM_PENDING_CONTROL_RETURN,
    ZR_VM_PENDING_CONTROL_BREAK,
    ZR_VM_PENDING_CONTROL_CONTINUE
} EZrVmPendingControlKind;

typedef struct SZrVmExceptionHandlerState {
    SZrCallInfo *callInfo;
    TZrUInt32 handlerIndex;
    EZrVmExceptionHandlerPhase phase;
} SZrVmExceptionHandlerState;

typedef struct SZrVmPendingControl {
    EZrVmPendingControlKind kind;
    SZrCallInfo *callInfo;
    TZrMemoryOffset targetInstructionOffset;
    TZrUInt32 valueSlot;
    SZrTypeValue value;
    TZrBool hasValue;
} SZrVmPendingControl;

struct ZR_STRUCT_ALIGN SZrState {
    SZrRawObject super;
    // reverse pointer to global
    struct SZrGlobalState *global;

    // SZrRawObject *gcList;
    // thread management
    EZrThreadStatus threadStatus;
    TZrMemoryOffset previousProgramCounter;


    SZrCallInfo *callInfoList;
    TZrUInt32 callInfoListLength;
    SZrCallInfo baseCallInfo;

    // address of current stack top
    TZrStackPointer stackTop;
    // we mark stack BASE and TAIL to indicate its space usage
    // last address of stack
    TZrStackPointer stackTail;
    // first address of stack
    TZrStackPointer stackBase;

    TZrStackPointer toBeClosedValueList;
    // closures
    struct SZrState *threadWithStackClosures;
    SZrClosureValue *stackClosureValueList;


    // for exceptions
    TZrUInt32 nestedNativeCalls;
    TZrUInt32 nestedNativeCallYieldFlag;
    SZrExceptionLongJump *exceptionRecoverPoint;
    TZrMemoryOffset exceptionHandlingFunctionOffset;
    SZrTypeValue currentException;
    EZrThreadStatus currentExceptionStatus;
    TZrBool hasCurrentException;
    SZrVmExceptionHandlerState *exceptionHandlerStack;
    TZrUInt32 exceptionHandlerStackLength;
    TZrUInt32 exceptionHandlerStackCapacity;
    SZrVmPendingControl pendingControl;


    // debug
    TZrBool allowDebugHook;
    TZrUInt32 baseDebugHookCount;
    TZrUInt32 debugHookCount;
    volatile FZrDebugHook debugHook;
    volatile TZrDebugSignal debugHookSignal;
    
    // 运行时检查标志
    TZrBool enableRuntimeBoundsCheck;   // 启用运行时边界检查
    TZrBool enableRuntimeTypeCheck;      // 启用运行时类型检查
    TZrBool enableRuntimeRangeCheck;     // 启用运行时范围检查
};

typedef struct SZrState SZrState;


ZR_CORE_API SZrState *ZrCore_State_New(struct SZrGlobalState *global);

ZR_CORE_API void ZrCore_State_Init(SZrState *state, struct SZrGlobalState *global);

ZR_CORE_API void ZrCore_State_MainThreadLaunch(SZrState *state, TZrPtr arguments);

ZR_CORE_API void ZrCore_State_Exit(SZrState *state);

ZR_CORE_API void ZrCore_State_Free(struct SZrGlobalState *global, SZrState *state);

ZR_CORE_API TZrInt32 ZrCore_State_ResetThread(SZrState *state, EZrThreadStatus status);

ZR_CORE_API EZrThreadStatus ZrCore_State_DoRun(SZrState *state, TZrNativeString entry);

ZR_FORCE_INLINE TZrSize ZrCore_State_StackGetSize(SZrState *state) {
    return state->stackTail.valuePointer - state->stackBase.valuePointer;
}

ZR_FORCE_INLINE TZrBool ZrCore_State_IsInClosureValueThreadList(SZrState *state) {
    // if list is not point to self, it means it is in list
    return state->threadWithStackClosures != state;
}


#endif // ZR_VM_CORE_STATE_H
