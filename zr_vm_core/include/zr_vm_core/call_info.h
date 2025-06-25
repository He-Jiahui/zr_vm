//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_CALL_INFO_H
#define ZR_VM_CORE_CALL_INFO_H
#include "zr_vm_core/stack.h"
struct SZrState;

enum EZrCallStatus {
    ZR_CALL_STATUS_ALLOW_HOOK = 1 << 0,
    ZR_CALL_STATUS_NATIVE_CALL = 1 << 1,
    ZR_CALL_STATUS_CREATE_FRAME = 1 << 2,
    ZR_CALL_STATUS_DEBUG_HOOK = 1 << 3,
    ZR_CALL_STATUS_YIELD_CALL = 1 << 4,
    ZR_CALL_STATUS_TAIL_CALL = 1 << 5,
    ZR_CALL_STATUS_HOOK_YIELD = 1 << 6,
    ZR_CALL_STATUS_DECONSTRUCTOR_CALL = 1 << 7,
    ZR_CALL_STATUS_CALL_INFO_TRANSFER = 1 << 8,
    ZR_CALL_STATUS_RELEASE_CALL = 1 << 9,
};

typedef enum EZrCallStatus EZrCallStatus;

typedef EZrThreadStatus (*FZrContinuationNativeFunction)(struct SZrState *state, EZrThreadStatus previousStatus,
                                                         TZrNativePtr arguments);


struct ZR_STRUCT_ALIGN SZrCallInfoContext {
    const TZrInstruction *programCounter;
    volatile TZrDebugSignal trap;
    TZrSize variableArgumentCount;
};

typedef struct SZrCallInfoContext SZrCallInfoContext;

struct ZR_STRUCT_ALIGN SZrCallInfoNativeContext {
    FZrContinuationNativeFunction continuationFunction;
    TZrMemoryOffset previousErrorFunction;
    TZrNativePtr continuationArguments;
};

typedef struct SZrCallInfoNativeContext SZrCallInfoNativeContext;

union TZrCallInfoContext {
    SZrCallInfoContext context;
    SZrCallInfoNativeContext nativeContext;
};

typedef union TZrCallInfoContext TZrCallInfoContext;

union TZrCallInfoYieldContext {
    TZrSize functionIndex;
    TUInt64 yieldValueCount;
    TUInt64 returnValueCount;

    struct {
        TUInt32 transferredValueStartIndex;
        TUInt32 transferredValueCount;
    } transferredValue;
};

typedef union TZrCallInfoYieldContext TZrCallInfoYieldContext;

struct ZR_STRUCT_ALIGN SZrCallInfo {
    TZrStackPointer functionBase;
    TZrStackPointer functionTop;

    EZrCallStatus callStatus;

    struct SZrCallInfo *previous;
    struct SZrCallInfo *next;

    TZrCallInfoContext context;
    TZrCallInfoYieldContext yieldContext;

    TZrSize expectedReturnCount;
};


typedef struct SZrCallInfo SZrCallInfo;

ZR_CORE_API void ZrCallInfoEntryNativeInit(struct SZrState *state, SZrCallInfo *callInfo, TZrStackPointer functionIndex,
                                           TZrStackPointer functionTop, SZrCallInfo *previous);
#endif //ZR_VM_CORE_CALL_INFO_H
