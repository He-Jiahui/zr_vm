//
// Tail-call frame reuse helpers split from execution_dispatch.c.
//

#include "execution/execution_internal.h"

TZrBool execution_prepare_meta_call_target(SZrState *state, TZrStackValuePointer stackPointer) {
    SZrTypeValue *value;
    SZrMeta *metaValue;
    TZrStackValuePointer p;

    if (state == ZR_NULL || stackPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    value = ZrCore_Stack_GetValue(stackPointer);
    metaValue = ZrCore_Value_GetMeta(state, value, ZR_META_CALL);
    if (metaValue == ZR_NULL || metaValue->function == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Profile_RecordSlowPathCurrent(ZR_PROFILE_SLOWPATH_META_CALL_PREPARE);

    for (p = state->stackTop.valuePointer; p > stackPointer; p--) {
        ZrCore_Stack_CopyValue(state, p, ZrCore_Stack_GetValue(p - 1));
    }
    state->stackTop.valuePointer++;
    ZrCore_Value_InitAsRawObject(state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(metaValue->function));
    return ZR_TRUE;
}

static TZrBool execution_callinfo_has_cleanup_registrations(SZrState *state, SZrCallInfo *callInfo) {
    TZrStackValuePointer frameBase;

    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_FALSE;
    }

    frameBase = callInfo->functionBase.valuePointer + 1;
    return state->toBeClosedValueList.valuePointer >= frameBase;
}

TZrBool execution_try_reuse_tail_call_frame(SZrState *state,
                                            SZrCallInfo *callInfo,
                                            TZrStackValuePointer functionPointer) {
    if (state == ZR_NULL || callInfo == ZR_NULL || functionPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZR_CALL_INFO_IS_VM(callInfo) || state->callInfoList != callInfo) {
        return ZR_FALSE;
    }

    if (execution_has_exception_handler_for_callinfo(state, callInfo)) {
        return ZR_FALSE;
    }

    if (execution_callinfo_has_cleanup_registrations(state, callInfo)) {
        return ZR_FALSE;
    }

    if (ZrCore_Function_TryReuseTailVmCall(state, callInfo, functionPointer)) {
        return ZR_TRUE;
    }

    if (!execution_prepare_meta_call_target(state, functionPointer)) {
        return ZR_FALSE;
    }

    return ZrCore_Function_TryReuseTailVmCall(state, callInfo, functionPointer);
}
