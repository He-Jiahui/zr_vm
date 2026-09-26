#include "zr_vm_library/aot_runtime.h"

#include "aot_runtime_internal.h"

#include "zr_vm_common/zr_runtime_sentinel_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/function_call_spread.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

/* 生成器标量返回快路径：借用 caller 结果槽并让 core 处理异常 handler 与关闭链。 */
/* BUG: CloseClosure 会运行用户 __close，可能经 ReserveScratchSlots 扩容迁移 VM 栈；
 * 这里及 Bool/U64/F64 变体在关闭前缓存 callerResultValue，随后仍写旧指针。
 * 通用 Return 在关闭后重新取得结果槽；需用会深调用的 __close 定向验证并修正四处。 */
TZrBool ZrLibrary_AotRuntime_ReturnI64(SZrState *state, TZrInt64 value) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    SZrTypeValue *callerResultValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT i64 return failed");
        return ZR_FALSE;
    }

    callerResultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (callerResultValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT i64 return failed");
        return ZR_FALSE;
    }

    execution_discard_exception_handlers_for_callinfo(state, callInfo);
    if (callInfo->functionTop.valuePointer != ZR_NULL &&
        (state->stackTop.valuePointer == ZR_NULL || state->stackTop.valuePointer < callInfo->functionTop.valuePointer)) {
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    }
    ZrCore_Closure_CloseClosure(state,
                                callInfo->functionBase.valuePointer + 1,
                                ZR_THREAD_STATUS_INVALID,
                                ZR_FALSE);
    ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, callInfo);
    if (callerResultValue->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
        callerResultValue->isGarbageCollectable) {
        ZrCore_Ownership_ReleaseValue(state, callerResultValue);
    }
    ZrCore_Value_InitAsInt(state, callerResultValue, value);
    state->stackTop.valuePointer = callInfo->functionBase.valuePointer + 1;
    return ZR_TRUE;
}

/* 与 ReturnI64 共用标量返回协议及其关闭回调后的栈位置约束。 */
/* BUG: callerResultValue 在 CloseClosure 触发栈迁移后可能悬空；需在关闭后重取。 */
TZrBool ZrLibrary_AotRuntime_ReturnBool(SZrState *state, TZrBool value) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    SZrTypeValue *callerResultValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT bool return failed");
        return ZR_FALSE;
    }

    callerResultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (callerResultValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT bool return failed");
        return ZR_FALSE;
    }

    execution_discard_exception_handlers_for_callinfo(state, callInfo);
    if (callInfo->functionTop.valuePointer != ZR_NULL &&
        (state->stackTop.valuePointer == ZR_NULL || state->stackTop.valuePointer < callInfo->functionTop.valuePointer)) {
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    }
    ZrCore_Closure_CloseClosure(state,
                                callInfo->functionBase.valuePointer + 1,
                                ZR_THREAD_STATUS_INVALID,
                                ZR_FALSE);
    ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, callInfo);
    if (callerResultValue->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
        callerResultValue->isGarbageCollectable) {
        ZrCore_Ownership_ReleaseValue(state, callerResultValue);
    }
    ZrCore_Value_InitAsBool(state, callerResultValue, value);
    state->stackTop.valuePointer = callInfo->functionBase.valuePointer + 1;
    return ZR_TRUE;
}

/* 与 ReturnI64 共用标量返回协议及其关闭回调后的栈位置约束。 */
/* BUG: callerResultValue 在 CloseClosure 触发栈迁移后可能悬空；需在关闭后重取。 */
TZrBool ZrLibrary_AotRuntime_ReturnU64(SZrState *state, TZrUInt64 value) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    SZrTypeValue *callerResultValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT u64 return failed");
        return ZR_FALSE;
    }

    callerResultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (callerResultValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT u64 return failed");
        return ZR_FALSE;
    }

    execution_discard_exception_handlers_for_callinfo(state, callInfo);
    if (callInfo->functionTop.valuePointer != ZR_NULL &&
        (state->stackTop.valuePointer == ZR_NULL || state->stackTop.valuePointer < callInfo->functionTop.valuePointer)) {
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    }
    ZrCore_Closure_CloseClosure(state,
                                callInfo->functionBase.valuePointer + 1,
                                ZR_THREAD_STATUS_INVALID,
                                ZR_FALSE);
    ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, callInfo);
    if (callerResultValue->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
        callerResultValue->isGarbageCollectable) {
        ZrCore_Ownership_ReleaseValue(state, callerResultValue);
    }
    ZrCore_Value_InitAsUInt(state, callerResultValue, value);
    state->stackTop.valuePointer = callInfo->functionBase.valuePointer + 1;
    return ZR_TRUE;
}

/* 与 ReturnI64 共用标量返回协议及其关闭回调后的栈位置约束。 */
/* BUG: callerResultValue 在 CloseClosure 触发栈迁移后可能悬空；需在关闭后重取。 */
TZrBool ZrLibrary_AotRuntime_ReturnF64(SZrState *state, TZrFloat64 value) {
    SZrLibraryAotRuntimeState *runtimeState;
    SZrCallInfo *callInfo = state != ZR_NULL ? state->callInfoList : ZR_NULL;
    SZrTypeValue *callerResultValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT f64 return failed");
        return ZR_FALSE;
    }

    callerResultValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (callerResultValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT f64 return failed");
        return ZR_FALSE;
    }

    execution_discard_exception_handlers_for_callinfo(state, callInfo);
    if (callInfo->functionTop.valuePointer != ZR_NULL &&
        (state->stackTop.valuePointer == ZR_NULL || state->stackTop.valuePointer < callInfo->functionTop.valuePointer)) {
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
    }
    ZrCore_Closure_CloseClosure(state,
                                callInfo->functionBase.valuePointer + 1,
                                ZR_THREAD_STATUS_INVALID,
                                ZR_FALSE);
    ZrCore_Function_TryCopyInlineConstructorReceiverBack(state, callInfo);
    if (callerResultValue->ownershipKind != ZR_OWNERSHIP_VALUE_KIND_NONE ||
        callerResultValue->isGarbageCollectable) {
        ZrCore_Ownership_ReleaseValue(state, callerResultValue);
    }
    ZrCore_Value_InitAsFloat(state, callerResultValue, value);
    state->stackTop.valuePointer = callInfo->functionBase.valuePointer + 1;
    return ZR_TRUE;
}

/* 生成桥接只应使用已登记的 deopt id；缺表时当前实现把 id 当作可接受的弱校验。 */
/* TODO: function 或 semIrDeoptTable 缺失时直接返回 true；需核对生成器在缺元数据
 * 夹具下是否仍能发出非 NONE id，补桥接验证测试。 */
static TZrBool aot_runtime_function_has_semir_deopt_id(const SZrFunction *function, TZrUInt32 deoptId) {
    TZrUInt32 index;

    if (deoptId == ZR_RUNTIME_SEMIR_DEOPT_ID_NONE) {
        return ZR_TRUE;
    }
    if (function == ZR_NULL || function->semIrDeoptTable == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0u; index < function->semIrDeoptTableLength; index++) {
        if (function->semIrDeoptTable[index].deoptId == deoptId) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 动态调用桥接在回退 core 前核对生成器给出的 deopt id，失败转为 AOT 诊断。 */
TZrBool ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 deoptId,
                                                        const TZrChar *errorLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *label = errorLabel != ZR_NULL ? errorLabel : "dynamic value";

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (frame == ZR_NULL || !aot_runtime_function_has_semir_deopt_id(frame->function, deoptId)) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s has invalid deopt bridge", label);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool aot_runtime_typed_direct_function_bindings_compatible(const SZrFunction *function) {
    EZrMetadataRuntimeBindingCompatibilityStatus status;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    status = ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility(function,
                                                                           function->moduleVersion,
                                                                           ZR_NULL,
                                                                           ZR_NULL,
                                                                           ZR_NULL);
    return (TZrBool)(status == ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE);
}

/* typed thunk 只在 caller 与 callee 的元数据绑定均与当前模块版本相容时采用。 */
TZrBool ZrLibrary_AotRuntime_CanUseTypedDirectCall(SZrState *state,
                                                   ZrAotGeneratedFrame *frame,
                                                   TZrUInt32 calleeFunctionIndex) {
    SZrFunction *calleeFunction;

    (void)state;
    if (frame == ZR_NULL || frame->function == ZR_NULL || frame->functionTable == ZR_NULL ||
        calleeFunctionIndex >= frame->functionCount) {
        return ZR_FALSE;
    }

    calleeFunction = frame->functionTable[calleeFunctionIndex];
    if (!aot_runtime_typed_direct_function_bindings_compatible(frame->function)) {
        return ZR_FALSE;
    }

    return aot_runtime_typed_direct_function_bindings_compatible(calleeFunction);
}

/* 绑定失配时经物化的 VM 值槽重新调用目标，不能继续使用旧 callee 索引的 typed thunk。 */
TZrBool ZrLibrary_AotRuntime_DeoptTypedDirectCall(SZrState *state,
                                                  ZrAotGeneratedFrame *frame,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 functionSlot,
                                                  TZrUInt32 argumentCount,
                                                  TZrUInt32 calleeFunctionIndex,
                                                  const TZrChar *errorLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *label = errorLabel != ZR_NULL ? errorLabel : "typed direct call";

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (frame == ZR_NULL || frame->functionTable == ZR_NULL || calleeFunctionIndex >= frame->functionCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s deopt failed", label);
        return ZR_FALSE;
    }

    return ZrLibrary_AotRuntime_CallStackValue(state, frame, destinationSlot, functionSlot, argumentCount, label);
}

/* 动态/deopt/展开调用共享此桥接：锚定 callable 与结果槽以跨越 core 调用期间的栈迁移。 */
/* TODO: 当前检查到的生成调用使 destinationSlot==functionSlot；接口未强制此关系。
 * 若两槽不同，末尾原始 SZrTypeValue 赋值绕过 Value_Copy 的旧目标释放及新值所有权处理；
 * 需枚举所有生成器与外部调用，加入不同槽的 ownership 测试后确定契约或修正。 */
TZrBool ZrLibrary_AotRuntime_CallStackValue(SZrState *state,
                                            ZrAotGeneratedFrame *frame,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 functionSlot,
                                            TZrUInt32 argumentCount,
                                            const TZrChar *errorLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *label = errorLabel != ZR_NULL ? errorLabel : "function call";
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    TZrStackValuePointer resultBase;
    SZrFunctionStackAnchor callAnchor;
    SZrFunctionStackAnchor destinationAnchor;
    SZrTypeValue *callableValue;
    SZrTypeValue *destinationValue;
    SZrTypeValue *resultValue;
    ZrAotGeneratedDirectCall directCall;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL || frame->slotBase == ZR_NULL ||
        functionSlot >= frame->generatedFrameSlotCount || destinationSlot >= frame->generatedFrameSlotCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s failed", label);
        return ZR_FALSE;
    }

    callBase = frame->slotBase + functionSlot;
    destinationPointer = frame->slotBase + destinationSlot;
    if (state->callInfoList == ZR_NULL || state->stackTop.valuePointer == ZR_NULL ||
        state->stackTop.valuePointer < callBase + 1 + argumentCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s has invalid stack range", label);
        return ZR_FALSE;
    }

    callableValue = ZrCore_Stack_GetValue(callBase);
    if (callableValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s is missing callable value", label);
        return ZR_FALSE;
    }

    if (!ZrLibrary_AotRuntime_PrepareDirectCall(state, frame, destinationSlot,
            functionSlot, argumentCount, &directCall)) return ZR_FALSE;
    if (directCall.prepared) {
        return ZrLibrary_AotRuntime_CallPreparedOrGeneric(state, frame, &directCall,
                destinationSlot, functionSlot, argumentCount, 1u);
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callAnchor);
    ZrCore_Function_StackAnchorInit(state, destinationPointer, &destinationAnchor);
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (state->callInfoList->functionTop.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer < state->stackTop.valuePointer) {
        state->callInfoList->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    resultBase = ZrCore_Function_CallAndRestoreAnchor(state, &callAnchor, 1);
    if (resultBase == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s failed", label);
        return ZR_FALSE;
    }

    destinationPointer = ZrCore_Function_StackAnchorRestore(state, &destinationAnchor);
    if (destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s lost destination slot", label);
        return ZR_FALSE;
    }

    destinationValue = ZrCore_Stack_GetValue(destinationPointer);
    resultValue = ZrCore_Stack_GetValue(resultBase);
    if (destinationValue == ZR_NULL || resultValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s has invalid result slot", label);
        return ZR_FALSE;
    }

    *destinationValue = *resultValue;
    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    state->stackTop.valuePointer = state->callInfoList->functionTop.valuePointer;
    return ZR_TRUE;
}

/* 先展开尾部数组实参，再用同一 CallStackValue 桥接保留调用约定和栈锚点。 */
TZrBool ZrLibrary_AotRuntime_CallSpread(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 prefixArgumentCount,
        const TZrChar *errorLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *label = errorLabel != ZR_NULL
                                   ? errorLabel
                                   : "spread function call";
    TZrUInt32 spreadSlot;
    TZrSize spreadArgumentCount;
    TZrSize argumentCount;
    TZrStackValuePointer callBase;
    SZrFunctionStackAnchor frameBaseAnchor;
    SZrFunctionStackAnchor callBaseAnchor;
    const SZrTypeValue *spreadValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL
                           ? aot_runtime_get_state_from_global(state->global)
                           : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL ||
        frame->slotBase == ZR_NULL ||
        functionSlot >= frame->generatedFrameSlotCount ||
        destinationSlot >= frame->generatedFrameSlotCount ||
        prefixArgumentCount > UINT32_MAX - functionSlot - 1u) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s failed", label);
        return ZR_FALSE;
    }

    spreadSlot = functionSlot + prefixArgumentCount + 1u;
    if (spreadSlot >= frame->generatedFrameSlotCount) {
        aot_runtime_fail(
                state, runtimeState, "generated AOT %s has invalid spread slot", label);
        return ZR_FALSE;
    }
    spreadValue = ZrCore_Stack_GetValue(frame->slotBase + spreadSlot);
    if (!ZrCore_Function_CallSpread_TryGetArgumentCount(
                spreadValue, &spreadArgumentCount)) {
        aot_runtime_fail(
                state, runtimeState, "generated AOT %s requires an array operand", label);
        return ZR_FALSE;
    }
    if (spreadArgumentCount > (TZrSize)UINT32_MAX - prefixArgumentCount) {
        aot_runtime_fail(
                state, runtimeState, "generated AOT %s argument count overflow", label);
        return ZR_FALSE;
    }

    callBase = frame->slotBase + functionSlot;
    ZrCore_Function_StackAnchorInit(state, frame->slotBase, &frameBaseAnchor);
    ZrCore_Function_StackAnchorInit(state, callBase, &callBaseAnchor);
    callBase = ZrCore_Function_CheckStackAndGc(
            state,
            prefixArgumentCount + spreadArgumentCount + 1u,
            callBase);
    frame->slotBase = ZrCore_Function_StackAnchorRestore(
            state, &frameBaseAnchor);
    callBase = ZrCore_Function_StackAnchorRestore(state, &callBaseAnchor);
    if (frame->slotBase == ZR_NULL || callBase == ZR_NULL ||
        !ZrCore_Function_CallSpread_ExpandPrepared(
                state, callBase, prefixArgumentCount, &argumentCount)) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s failed", label);
        return ZR_FALSE;
    }

    return ZrLibrary_AotRuntime_CallStackValue(
            state,
            frame,
            destinationSlot,
            functionSlot,
            (TZrUInt32)argumentCount,
            label);
}

/* 动态退优化先验证 deopt id，再回到值槽调用，以保留 VM 的实际 callable 语义。 */
TZrBool ZrLibrary_AotRuntime_CallDynamicDeoptBridge(SZrState *state,
                                                    ZrAotGeneratedFrame *frame,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 functionSlot,
                                                    TZrUInt32 argumentCount,
                                                    TZrUInt32 deoptId,
                                                    const TZrChar *errorLabel) {
    const TZrChar *label = errorLabel != ZR_NULL ? errorLabel : "dynamic call";

    if (!ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(state, frame, deoptId, label)) {
        return ZR_FALSE;
    }
    if (deoptId == ZR_RUNTIME_SEMIR_DEOPT_ID_NONE) {
        return ZrLibrary_AotRuntime_CallStackValue(state,
                                                   frame,
                                                   destinationSlot,
                                                   functionSlot,
                                                   argumentCount,
                                                   label);
    }

    return ZrLibrary_AotRuntime_CallStackValue(state,
                                               frame,
                                               destinationSlot,
                                               functionSlot,
                                               argumentCount,
                                               label);
}

/* 生成器显式未支持的元调用边界：报错而不猜测可能改变 receiver/ownership 的退路。 */
TZrBool ZrLibrary_AotRuntime_UnsupportedMetaCall(SZrState *state,
                                                 ZrAotGeneratedFrame *frame,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 receiverSlot,
                                                 TZrUInt32 argumentCount,
                                                 const TZrChar *errorLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *label = errorLabel != ZR_NULL ? errorLabel : "unsupported AOT meta call";
    SZrTypeValue *receiverValue;
    SZrTypeValue *destinationValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL ||
        destinationSlot >= frame->generatedFrameSlotCount || receiverSlot >= frame->generatedFrameSlotCount) {
        aot_runtime_fail(state, runtimeState, "%s", label);
        return ZR_FALSE;
    }

    receiverValue = ZrCore_Stack_GetValue(frame->slotBase + receiverSlot);
    destinationValue = ZrCore_Stack_GetValue(frame->slotBase + destinationSlot);
    if (receiverValue == ZR_NULL || destinationValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "%s", label);
        return ZR_FALSE;
    }

    (void)argumentCount;
    (void)receiverValue;
    (void)destinationValue;
    aot_runtime_fail(state, runtimeState, "%s", label);
    return ZR_FALSE;
}

TZrBool ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(SZrState *state,
                                                        ZrAotGeneratedFrame *frame,
                                                        TZrUInt32 primarySlot,
                                                        TZrUInt32 secondarySlot,
                                                        TZrUInt32 memberOrCacheIndex,
                                                        const TZrChar *opcodeName) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *safeOpcodeName = opcodeName != ZR_NULL ? opcodeName : "META_VALUE_ACCESS";
    SZrTypeValue *primaryValue;
    SZrTypeValue *secondaryValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL ||
        primarySlot >= frame->generatedFrameSlotCount || secondarySlot >= frame->generatedFrameSlotCount) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT meta value access: %s", safeOpcodeName);
        return ZR_FALSE;
    }

    primaryValue = ZrCore_Stack_GetValue(frame->slotBase + primarySlot);
    secondaryValue = ZrCore_Stack_GetValue(frame->slotBase + secondarySlot);
    if (primaryValue == ZR_NULL || secondaryValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT meta value access: %s", safeOpcodeName);
        return ZR_FALSE;
    }

    (void)memberOrCacheIndex;
    (void)primaryValue;
    (void)secondaryValue;
    aot_runtime_fail(state, runtimeState, "unsupported AOT meta value access: %s", safeOpcodeName);
    return ZR_FALSE;
}

TZrBool ZrLibrary_AotRuntime_UnsupportedDynamicValueAccess(SZrState *state,
                                                           ZrAotGeneratedFrame *frame,
                                                           TZrUInt32 primarySlot,
                                                           TZrUInt32 secondarySlot,
                                                           TZrUInt32 operandIndex,
                                                           const TZrChar *opcodeName) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *safeOpcodeName = opcodeName != ZR_NULL ? opcodeName : "DYNAMIC_VALUE_ACCESS";
    SZrTypeValue *primaryValue;
    SZrTypeValue *secondaryValue;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->slotBase == ZR_NULL ||
        primarySlot >= frame->generatedFrameSlotCount || secondarySlot >= frame->generatedFrameSlotCount) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT dynamic value access: %s", safeOpcodeName);
        return ZR_FALSE;
    }

    primaryValue = ZrCore_Stack_GetValue(frame->slotBase + primarySlot);
    secondaryValue = ZrCore_Stack_GetValue(frame->slotBase + secondarySlot);
    if (primaryValue == ZR_NULL || secondaryValue == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "unsupported AOT dynamic value access: %s", safeOpcodeName);
        return ZR_FALSE;
    }

    (void)operandIndex;
    (void)primaryValue;
    (void)secondaryValue;
    aot_runtime_fail(state, runtimeState, "unsupported AOT dynamic value access: %s", safeOpcodeName);
    return ZR_FALSE;
}

/* 静态 AOT thunk 经 core PreCall 建立 VM 帧，供生成调用保留异常和返回协议。 */
/* TODO: 多处参数准备或 PreCall 失败直接返回，需核对已物化值槽和 counted owner
 * 是否都由上层异常路径回收；补失败注入与 ownership 测试。 */
TZrBool ZrLibrary_AotRuntime_CallStaticDirect(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 functionSlot,
                                              TZrUInt32 argumentCount,
                                              TZrUInt32 calleeFunctionIndex,
                                              FZrAotEntryThunk calleeThunk) {
    SZrLibraryAotRuntimeState *runtimeState;
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    SZrFunctionStackAnchor callerFrameTopAnchor;
    SZrCallInfo *callInfo;
    SZrFunction *metadataFunction;
    SZrTypeValue *callableValue;
    TZrUInt32 callWindowIndex;
    TZrUInt32 argumentIndex;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL || frame->slotBase == ZR_NULL ||
        frame->callInfo == ZR_NULL || frame->callInfo != state->callInfoList ||
        destinationSlot >= frame->generatedFrameSlotCount || functionSlot >= frame->generatedFrameSlotCount ||
        argumentCount > frame->generatedFrameSlotCount - functionSlot - 1u ||
        calleeFunctionIndex == ZR_AOT_RUNTIME_RESUME_FALLTHROUGH || calleeThunk == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }

    callableValue = ZrCore_Stack_GetValue(frame->slotBase + functionSlot);
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue);
    if (callableValue == ZR_NULL || metadataFunction == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }
    if (!aot_runtime_static_direct_call_identity_matches(
                frame, calleeFunctionIndex, metadataFunction, calleeThunk)) {
        aot_runtime_fail(state, runtimeState, "generated AOT direct-core call identity drift");
        return ZR_FALSE;
    }

    callBase = frame->callInfo->functionTop.valuePointer;
    if (callBase != ZR_NULL && callBase < frame->slotBase + frame->generatedFrameSlotCount) {
        callBase = frame->slotBase + frame->generatedFrameSlotCount;
    }
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid stack range");
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callerFrameTopAnchor);
    callBase = ZrCore_Function_CheckStackAndGc(state, 1u + argumentCount, callBase);
    if (callBase == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid stack range");
        return ZR_FALSE;
    }
    callBase = ZrCore_Function_StackAnchorRestore(state, &callerFrameTopAnchor);
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid stack range");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    for (callWindowIndex = 0u; callWindowIndex < 1u + argumentCount; callWindowIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase + callWindowIndex));
    }
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (state->callInfoList->functionTop.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer < state->stackTop.valuePointer) {
        state->callInfoList->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    callableValue = ZrCore_Stack_GetValue(frame->slotBase + functionSlot);
    destinationPointer = frame->slotBase + destinationSlot;
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue);
    if (callableValue == ZR_NULL || metadataFunction == ZR_NULL ||
        frame->callInfo->functionTop.valuePointer < frame->slotBase + functionSlot + 1u + argumentCount) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(callBase), callableValue);
    for (argumentIndex = 0u; argumentIndex < argumentCount; argumentIndex++) {
        const TZrUInt32 sourceSlot = functionSlot + 1u + argumentIndex;
        const SZrFunctionFrameSlotLayout *argumentLayout =
                ZrCore_Function_FindFrameSlotLayout(frame->function, sourceSlot);
        SZrTypeValue *argumentValue = ZrCore_Stack_GetValue(frame->slotBase + sourceSlot);

        if (argumentValue == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid argument source");
            return ZR_FALSE;
        }

        if (argumentLayout != ZR_NULL &&
            argumentLayout->slotKind == (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE &&
            argumentLayout->byteSize >= (TZrUInt32)sizeof(SZrTypeValue)) {
            SZrStackFramePlace argumentPlace;
            SZrTypeValue *argumentFrame;

            if (!ZrCore_Function_MakeFrameSlotPlace(state,
                                                    frame->function,
                                                    frame->slotBase,
                                                    sourceSlot,
                                                    &argumentPlace)) {
                aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid argument source");
                return ZR_FALSE;
            }

            argumentFrame = (SZrTypeValue *)argumentPlace.address;
            if (argumentFrame == ZR_NULL) {
                aot_runtime_fail(state, runtimeState, "generated AOT static direct call has invalid argument source");
                return ZR_FALSE;
            }
            if (argumentFrame != argumentValue) {
                ZrCore_Value_Copy(state, argumentFrame, argumentValue);
            }
            argumentValue = argumentFrame;
        }

        ZrCore_Value_Copy(state,
                          ZrCore_Stack_GetValue(callBase + 1u + argumentIndex),
                          argumentValue);
    }

    callInfo = ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource(state,
                                                                                   callBase,
                                                                                   metadataFunction,
                                                                                   argumentCount,
                                                                                   1,
                                                                                   destinationPointer,
                                                                                   frame->slotBase,
                                                                                   functionSlot + 1u);
    if (callInfo == ZR_NULL || state->callInfoList != callInfo) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }

    if (!calleeThunk(state)) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }
    ZrCore_Function_PostCall(state, callInfo, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE || state->callInfoList == ZR_NULL ||
        state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    callBase = ZrCore_Function_StackAnchorRestore(state, &callerFrameTopAnchor);
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT static direct call failed");
        return ZR_FALSE;
    }

    for (callWindowIndex = 0u; callWindowIndex < 1u + argumentCount; callWindowIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase + callWindowIndex));
    }
    frame->callInfo->functionTop.valuePointer = callBase;
    state->stackTop.valuePointer = callBase;
    return ZR_TRUE;
}

/* 内联结构体调用按布局偏移把结果写入物理帧，供生成器避免装箱。 */
/* TODO: destinationByteOffset 的目标边界目前只做部分尺寸检查；需与 FrameSlotLayout
 * 的对齐及目标槽上界核对，并用非法偏移生成夹具验证。 */
TZrBool ZrLibrary_AotRuntime_CallInlineStruct(SZrState *state,
                                              ZrAotGeneratedFrame *frame,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 functionSlot,
                                              TZrUInt32 argumentCount,
                                              TZrUInt32 calleeFunctionIndex,
                                              TZrUInt32 destinationTypeLayoutId,
                                              TZrUInt32 destinationByteOffset,
                                              TZrUInt32 destinationByteSize,
                                              FZrAotEntryThunk calleeThunk) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrTypeLayout *returnLayout;
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    SZrFunctionStackAnchor callerFrameTopAnchor;
    SZrCallInfo *callInfo;
    SZrFunction *metadataFunction;
    SZrTypeValue *callableValue;
    TZrUInt32 callWindowIndex;
    TZrUInt32 argumentIndex;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL || frame->slotBase == ZR_NULL ||
        frame->callInfo == ZR_NULL || frame->callInfo != state->callInfoList ||
        destinationSlot >= frame->generatedFrameSlotCount || functionSlot >= frame->generatedFrameSlotCount ||
        argumentCount > frame->generatedFrameSlotCount - functionSlot - 1u ||
        calleeFunctionIndex == ZR_AOT_RUNTIME_RESUME_FALLTHROUGH || calleeThunk == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    callableValue = ZrCore_Stack_GetValue(frame->slotBase + functionSlot);
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue);
    if (callableValue == ZR_NULL || metadataFunction == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }
    if (!aot_runtime_static_direct_call_identity_matches(
                frame, calleeFunctionIndex, metadataFunction, calleeThunk)) {
        aot_runtime_fail(state, runtimeState, "generated AOT direct-core call identity drift");
        return ZR_FALSE;
    }

    returnLayout = ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function,
                                                                    destinationTypeLayoutId);
    if (returnLayout == ZR_NULL || returnLayout->kind != ZR_TYPE_LAYOUT_KIND_STRUCT ||
        returnLayout->byteSize != destinationByteSize) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    callBase = frame->callInfo->functionTop.valuePointer;
    if (callBase != ZR_NULL && callBase < frame->slotBase + frame->generatedFrameSlotCount) {
        callBase = frame->slotBase + frame->generatedFrameSlotCount;
    }
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callerFrameTopAnchor);
    callBase = ZrCore_Function_CheckStackAndGc(state, 1u + argumentCount, callBase);
    if (callBase == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }
    callBase = ZrCore_Function_StackAnchorRestore(state, &callerFrameTopAnchor);
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    for (callWindowIndex = 0u; callWindowIndex < 1u + argumentCount; callWindowIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase + callWindowIndex));
    }
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (state->callInfoList->functionTop.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer < state->stackTop.valuePointer) {
        state->callInfoList->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    callableValue = ZrCore_Stack_GetValue(frame->slotBase + functionSlot);
    destinationPointer = (TZrStackValuePointer)((TZrByte *)frame->slotBase + destinationByteOffset);
    metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state, callableValue);
    if (callableValue == ZR_NULL || metadataFunction == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(callBase), callableValue);
    for (argumentIndex = 0u; argumentIndex < argumentCount; argumentIndex++) {
        const TZrUInt32 sourceSlot = functionSlot + 1u + argumentIndex;
        const SZrFunctionFrameSlotLayout *argumentLayout =
                ZrCore_Function_FindFrameSlotLayout(frame->function, sourceSlot);
        SZrTypeValue *argumentDense = ZrCore_Stack_GetValue(frame->slotBase + sourceSlot);
        SZrTypeValue *argumentFrame = ZR_NULL;

        if (argumentLayout == ZR_NULL ||
            argumentLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            argumentLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue) ||
            argumentDense == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
            return ZR_FALSE;
        }

        {
            SZrStackFramePlace argumentPlace;

            if (!ZrCore_Function_MakeFrameSlotPlace(state,
                                                    frame->function,
                                                    frame->slotBase,
                                                    sourceSlot,
                                                    &argumentPlace)) {
                aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
                return ZR_FALSE;
            }
            argumentFrame = (SZrTypeValue *)argumentPlace.address;
        }
        if (argumentFrame == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
            return ZR_FALSE;
        }
        if (argumentFrame != argumentDense) {
            ZrCore_Value_Copy(state, argumentFrame, argumentDense);
        }
        ZrCore_Value_Copy(state,
                          ZrCore_Stack_GetValue(callBase + 1u + argumentIndex),
                          argumentFrame);
    }

    callInfo = ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource(state,
                                                                                   callBase,
                                                                                   metadataFunction,
                                                                                   argumentCount,
                                                                                   1,
                                                                                   destinationPointer,
                                                                                   frame->slotBase,
                                                                                   functionSlot + 1u);
    if (callInfo == ZR_NULL || state->callInfoList != callInfo) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    if (!calleeThunk(state)) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }
    ZrCore_Function_PostCall(state, callInfo, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE || state->callInfoList == ZR_NULL ||
        state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    callBase = ZrCore_Function_StackAnchorRestore(state, &callerFrameTopAnchor);
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct typed call failed");
        return ZR_FALSE;
    }

    for (callWindowIndex = 0u; callWindowIndex < 1u + argumentCount; callWindowIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase + callWindowIndex));
    }
    frame->callInfo->functionTop.valuePointer = callBase;
    state->stackTop.valuePointer = callBase;
    return ZR_TRUE;
}

/* 内联结构体的动态退优化桥接仍要沿用目标布局与 core 调用的清理契约。 */
/* TODO: 与 CallInlineStruct 共用目标偏移边界疑点，需加入非法偏移和失败后临时所有权测试。 */
TZrBool ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(
        SZrState *state,
        ZrAotGeneratedFrame *frame,
        TZrUInt32 destinationSlot,
        TZrUInt32 functionSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 destinationTypeLayoutId,
        TZrUInt32 destinationByteOffset,
        TZrUInt32 destinationByteSize,
        TZrUInt32 deoptId,
        const TZrChar *errorLabel) {
    SZrLibraryAotRuntimeState *runtimeState;
    const TZrChar *label = errorLabel != ZR_NULL ? errorLabel : "generic inline struct call";
    const SZrTypeLayout *returnLayout;
    TZrStackValuePointer callBase;
    TZrStackValuePointer destinationPointer;
    TZrStackValuePointer resultBase;
    SZrFunctionStackAnchor callerFrameTopAnchor;
    SZrTypeValue *callableValue;
    TZrUInt32 callWindowIndex;
    TZrUInt32 argumentIndex;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (!ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(state, frame, deoptId, label)) {
        return ZR_FALSE;
    }

    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL || frame->slotBase == ZR_NULL ||
        frame->callInfo == ZR_NULL || frame->callInfo != state->callInfoList ||
        destinationSlot >= frame->generatedFrameSlotCount || functionSlot >= frame->generatedFrameSlotCount ||
        argumentCount > frame->generatedFrameSlotCount - functionSlot - 1u) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s deopt failed", label);
        return ZR_FALSE;
    }

    returnLayout = ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function,
                                                                    destinationTypeLayoutId);
    if (returnLayout == ZR_NULL || returnLayout->kind != ZR_TYPE_LAYOUT_KIND_STRUCT ||
        returnLayout->byteSize != destinationByteSize) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s deopt failed", label);
        return ZR_FALSE;
    }

    callBase = frame->callInfo->functionTop.valuePointer;
    if (callBase != ZR_NULL && callBase < frame->slotBase + frame->generatedFrameSlotCount) {
        callBase = frame->slotBase + frame->generatedFrameSlotCount;
    }
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s deopt failed", label);
        return ZR_FALSE;
    }

    ZrCore_Function_StackAnchorInit(state, callBase, &callerFrameTopAnchor);
    callBase = ZrCore_Function_CheckStackAndGc(state, 1u + argumentCount, callBase);
    if (callBase == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s deopt failed", label);
        return ZR_FALSE;
    }
    callBase = ZrCore_Function_StackAnchorRestore(state, &callerFrameTopAnchor);
    if (callBase == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s deopt failed", label);
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    for (callWindowIndex = 0u; callWindowIndex < 1u + argumentCount; callWindowIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase + callWindowIndex));
    }
    state->stackTop.valuePointer = callBase + 1 + argumentCount;
    if (state->callInfoList->functionTop.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer < state->stackTop.valuePointer) {
        state->callInfoList->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    callableValue = ZrCore_Stack_GetValue(frame->slotBase + functionSlot);
    destinationPointer = (TZrStackValuePointer)((TZrByte *)frame->slotBase + destinationByteOffset);
    if (callableValue == ZR_NULL || destinationPointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s deopt failed", label);
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(callBase), callableValue);
    for (argumentIndex = 0u; argumentIndex < argumentCount; argumentIndex++) {
        const TZrUInt32 sourceSlot = functionSlot + 1u + argumentIndex;
        const SZrFunctionFrameSlotLayout *argumentLayout =
                ZrCore_Function_FindFrameSlotLayout(frame->function, sourceSlot);
        SZrTypeValue *argumentDense = ZrCore_Stack_GetValue(frame->slotBase + sourceSlot);
        SZrTypeValue *argumentFrame = ZR_NULL;

        if (argumentLayout == ZR_NULL ||
            argumentLayout->slotKind != (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE ||
            argumentLayout->byteSize < (TZrUInt32)sizeof(SZrTypeValue) ||
            argumentDense == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "generated AOT %s deopt failed", label);
            return ZR_FALSE;
        }

        {
            SZrStackFramePlace argumentPlace;

            if (!ZrCore_Function_MakeFrameSlotPlace(state,
                                                    frame->function,
                                                    frame->slotBase,
                                                    sourceSlot,
                                                    &argumentPlace)) {
                aot_runtime_fail(state, runtimeState, "generated AOT %s deopt failed", label);
                return ZR_FALSE;
            }
            argumentFrame = (SZrTypeValue *)argumentPlace.address;
        }
        if (argumentFrame == ZR_NULL) {
            aot_runtime_fail(state, runtimeState, "generated AOT %s deopt failed", label);
            return ZR_FALSE;
        }
        if (argumentFrame != argumentDense) {
            ZrCore_Value_Copy(state, argumentFrame, argumentDense);
        }
        ZrCore_Value_Copy(state,
                          ZrCore_Stack_GetValue(callBase + 1u + argumentIndex),
                          argumentFrame);
    }

    resultBase = ZrCore_Function_CallWithoutYieldAndRestoreWithReturnDestination(state,
                                                                                 callBase,
                                                                                 1,
                                                                                 destinationPointer);
    if (resultBase == ZR_NULL || state->threadStatus != ZR_THREAD_STATUS_FINE ||
        state->callInfoList == ZR_NULL || state->callInfoList->functionBase.valuePointer == ZR_NULL ||
        state->callInfoList->functionTop.valuePointer == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT %s deopt failed", label);
        return ZR_FALSE;
    }

    frame->callInfo = state->callInfoList;
    frame->slotBase = state->callInfoList->functionBase.valuePointer + 1;
    callBase = resultBase;
    for (callWindowIndex = 0u; callWindowIndex < 1u + argumentCount; callWindowIndex++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase + callWindowIndex));
    }
    frame->callInfo->functionTop.valuePointer = callBase;
    state->stackTop.valuePointer = callBase;
    return ZR_TRUE;
}

/* 内联结构体返回把物理槽字节交给 caller，需在关闭当前作用域后保持来源地址有效。 */
/* TODO: returnSource 与 functionTop 的边界以及 sourceByteSize 对齐只做部分校验；
 * 需以短槽/偏移异常夹具核对读取是否可能越过帧布局。 */
TZrBool ZrLibrary_AotRuntime_ReturnInlineStruct(SZrState *state,
                                                ZrAotGeneratedFrame *frame,
                                                TZrUInt32 sourceSlot,
                                                TZrUInt32 sourceTypeLayoutId,
                                                TZrUInt32 sourceByteOffset,
                                                TZrUInt32 sourceByteSize,
                                                TZrUInt32 *outSkipDropSlot) {
    SZrLibraryAotRuntimeState *runtimeState;
    const SZrTypeLayout *returnLayout;
    SZrCallInfo *callInfo;
    TZrStackValuePointer returnSource;

    runtimeState = state != ZR_NULL && state->global != ZR_NULL ? aot_runtime_get_state_from_global(state->global) : ZR_NULL;
    if (state == ZR_NULL || frame == ZR_NULL || frame->function == ZR_NULL || frame->slotBase == ZR_NULL ||
        outSkipDropSlot == ZR_NULL) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct return failed");
        return ZR_FALSE;
    }

    returnLayout = ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function, sourceTypeLayoutId);
    callInfo = frame->callInfo != ZR_NULL ? frame->callInfo : state->callInfoList;
    returnSource = (TZrStackValuePointer)((TZrByte *)frame->slotBase + sourceByteOffset);
    if (returnLayout == ZR_NULL || returnLayout->kind != ZR_TYPE_LAYOUT_KIND_STRUCT ||
        returnLayout->byteSize != sourceByteSize || callInfo == ZR_NULL ||
        callInfo->functionBase.valuePointer == ZR_NULL || returnSource == ZR_NULL ||
        (callInfo->functionTop.valuePointer != ZR_NULL && returnSource >= callInfo->functionTop.valuePointer)) {
        aot_runtime_fail(state, runtimeState, "generated AOT inline struct return failed");
        return ZR_FALSE;
    }

    state->stackTop.valuePointer = returnSource + 1;
    *outSkipDropSlot = sourceSlot;
    return ZR_TRUE;
}
