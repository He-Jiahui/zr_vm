#include "ownership_resource_internal.h"

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "gc/gc_domain_internal.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"

/**
 * @brief 清空已经移交或消费的值槽，避免后续清理再次取得同一份责任。
 * @pre 调用者已完成需要的所有权转移；本函数只重置槽位，不执行析构。
 */
static void ownership_resource_reset_value(SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return;
    }

    value->type = ZR_VALUE_TYPE_NULL;
    value->value.nativeObject.nativeUInt64 = 0;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

/**
 * @brief 为 resource 对象建立不经共享控制块的直接所有权值。
 * @note Resource 的存活由 GC domain root 与生命周期状态共同维护。
 */
static void ownership_resource_set_direct_value(SZrTypeValue *value,
                                                 SZrRawObject *object,
                                                 EZrOwnershipValueKind ownershipKind) {
    if (value == ZR_NULL || object == ZR_NULL) {
        return;
    }

    value->type = (EZrValueType)object->type;
    value->value.object = object;
    value->isGarbageCollectable = ZR_TRUE;
    value->isNative = object->isNative;
    value->ownershipKind = ownershipKind;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

/** @brief 将直接 owner 槽标记为唯一所有者；调用者负责先确立资源根。 */
static void ownership_resource_set_direct_unique(SZrTypeValue *value,
                                                  SZrRawObject *object) {
    ownership_resource_set_direct_value(
            value, object, ZR_OWNERSHIP_VALUE_KIND_UNIQUE);
}

/**
 * @brief 判断对象是否具有 resource 生命周期协议。
 * @note 仅 class object 且原型声明 RESOURCE 才走直接根和显式 drop 路径。
 */
TZrBool ZrCore_OwnershipResource_IsObject(const SZrRawObject *object) {
    const SZrObject *zrObject;

    if (object == ZR_NULL || object->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
        return ZR_FALSE;
    }
    zrObject = (const SZrObject *)object;
    return zrObject->prototype != ZR_NULL &&
           (zrObject->prototype->modifierFlags & ZR_TYPE_MODIFIER_FLAG_RESOURCE) != 0;
}

/** @brief 识别无需控制块的直接唯一 owner，供 move/copy/drop 分流。 */
TZrBool ZrCore_OwnershipResource_IsDirectUniqueValue(const SZrTypeValue *value) {
    return value != ZR_NULL &&
           value->isGarbageCollectable &&
           !ZR_VALUE_IS_TYPE_NULL(value->type) &&
           value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_UNIQUE &&
           value->ownershipControl == ZR_NULL &&
           ZrCore_OwnershipResource_IsObject(value->value.object);
}

/** @brief 识别直接借用槽；该借用不增加根，必须遵守其外部 owner 的寿命。 */
TZrBool ZrCore_OwnershipResource_IsDirectLoanedValue(const SZrTypeValue *value) {
    return value != ZR_NULL &&
           value->isGarbageCollectable &&
           !ZR_VALUE_IS_TYPE_NULL(value->type) &&
           value->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_LOANED &&
           value->ownershipControl == ZR_NULL &&
           ZrCore_OwnershipResource_IsObject(value->value.object);
}

/**
 * @brief 把新建或已有 resource 登记为当前 state 的唯一直接 owner。
 * @pre 调用方应先准备 destination；失败时不取得资源所有权。
 * @return domain root 登记成功后返回 true，并把资源置为 ALIVE。
 * TODO: 核实调用约定禁止对已交付或已 DROPPED 的 resource 再次初始化；本层
 *       会重新设置 ALIVE，但不验证此前的生命周期状态。
 */
TZrBool ZrCore_OwnershipResource_InitUnique(SZrState *state,
                                             SZrTypeValue *destination,
                                             SZrRawObject *object) {
    if (state == ZR_NULL || destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsObject(object)) {
        return ZR_FALSE;
    }
    if (!ZrCore_GcDomain_RegisterOwnershipRoot(state, object)) {
        return ZR_FALSE;
    }

    object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_ALIVE;
    ownership_resource_set_direct_unique(destination, object);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

/**
 * @brief 在两个值槽之间转移直接唯一所有权，不触碰 domain root 计数。
 * @pre source 必须是直接唯一 owner；该操作消费 source。
 * @note 构造期间的资源在首次交付 owner 时转为 ALIVE。
 */
TZrBool ZrCore_OwnershipResource_MoveUnique(SZrState *state,
                                             SZrTypeValue *destination,
                                             SZrTypeValue *source) {
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    if (object->resourceLifecycleState == ZR_RESOURCE_LIFECYCLE_CONSTRUCTING) {
        object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_ALIVE;
    }
    ownership_resource_reset_value(source);
    ownership_resource_set_direct_unique(destination, object);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

/**
 * @brief 把唯一 owner 槽移成 loaned 槽，供受控借用链暂存。
 * @pre source 必须是直接唯一 owner；返回前 source 被清空。
 * @note loan 不创建第二个根；借用必须在原责任链允许的范围内归还。
 */
TZrBool ZrCore_OwnershipResource_LoanUnique(SZrState *state,
                                            SZrTypeValue *destination,
                                            SZrTypeValue *source) {
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    ownership_resource_reset_value(source);
    ownership_resource_set_direct_value(
            destination, object, ZR_OWNERSHIP_VALUE_KIND_LOANED);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

/**
 * @brief 将直接 loan 槽恢复为唯一 owner 槽。
 * @pre source 必须是直接 loaned 槽；调用者负责保证借用责任已经结束。
 */
TZrBool ZrCore_OwnershipResource_ReturnLoan(SZrState *state,
                                            SZrTypeValue *destination,
                                            SZrTypeValue *source) {
    SZrRawObject *object;

    if (state == ZR_NULL || destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsDirectLoanedValue(source)) {
        return ZR_FALSE;
    }

    object = source->value.object;
    ownership_resource_reset_value(source);
    ownership_resource_set_direct_unique(destination, object);
    ZrCore_Gc_ValueStaticAssertIsAlive(state, destination);
    return ZR_TRUE;
}

/**
 * @brief 为值复制路径复制直接唯一 resource 的句柄。
 * @note 这不是所有权复制：预期用于受约束的内部布局镜像；调用者不得借此
 *       创建第二个可独立释放的 owner。
 * TODO: 核实所有 ZrCore_Value_Copy 到 direct unique resource 的调用都只用于
 *       物理槽与逻辑槽镜像；一般复制若留下两个 UNIQUE 槽会破坏唯一性约定。
 */
void ZrCore_OwnershipResource_CopyUnique(SZrTypeValue *destination,
                                         const SZrTypeValue *source) {
    if (destination == ZR_NULL ||
        !ZrCore_OwnershipResource_IsDirectUniqueValue(source)) {
        return;
    }
    ownership_resource_set_direct_unique(destination, source->value.object);
}

/**
 * @brief 暂存 drop 回调首次失败的异常及其 GC 根。
 * @note 清理仍需完成全部字段释放；失败状态在恢复调用者状态后再交还。
 */
typedef struct SZrResourceDropFailure {
    EZrThreadStatus status;
    SZrTypeValue exception;
    EZrThreadStatus exceptionStatus;
    TZrBool hasException;
    SZrRawObject **root;
} SZrResourceDropFailure;

/** @brief 保留第一次清理失败及异常对象；后续失败不覆盖首个诊断。 */
static void ownership_resource_capture_failure(
        SZrState *state,
        SZrResourceDropFailure *failure,
        EZrThreadStatus status) {
    if (failure->status != ZR_THREAD_STATUS_FINE || status == ZR_THREAD_STATUS_FINE) {
        return;
    }
    failure->status = status;
    failure->exception = state->currentException;
    failure->exceptionStatus = state->currentExceptionStatus;
    failure->hasException = state->hasCurrentException;
    *failure->root = failure->hasException &&
                             ZrCore_Value_IsGarbageCollectable(&failure->exception)
                             ? ZrCore_Value_GetRawObject(&failure->exception)
                             : ZR_NULL;
}

/** @brief 回调失败后清理到资源 drop 前保存的 close-list 边界。 */
static void ownership_resource_close_callback_values(SZrState *state, TZrPtr argument) {
    TZrMemoryOffset *boundary = (TZrMemoryOffset *)argument;
    ZrCore_Closure_CloseClosure(
            state, ZrCore_Stack_LoadOffsetToPointer(state, *boundary),
            ZR_THREAD_STATUS_INVALID, ZR_FALSE);
}

/** @brief 在回调隔离边界内取消尚未完成的控制转移。 */
static void ownership_resource_clear_callback_pending(SZrState *state, TZrPtr argument) {
    ZR_UNUSED_PARAMETER(argument);
    execution_clear_pending_control(state);
}

/**
 * @brief 在隔离的 VM 执行上下文中调用析构或字段释放回调。
 * @note drop 回调可抛异常、关闭值或重定位栈；返回前恢复调用者栈锚、
 *       handler 深度及 AOT 根帧，并把首个失败交给外层 drop 汇总。
 */
static EZrThreadStatus ownership_resource_run_callback(
        SZrState *state,
        FZrTryFunction callback,
        TZrPtr argument,
        SZrResourceDropFailure *failure) {
    SZrCallInfo *savedCallInfo = state->callInfoList;
    SZrFunctionStackAnchor stackTop;
    SZrFunctionStackAnchor functionBase;
    SZrFunctionStackAnchor functionTop;
    SZrFunctionStackAnchor returnDestination;
    SZrAotGcRootFrame *savedRootFrame = state->aotGcRootFrameStack;
    TZrUInt32 savedRootDepth = state->aotGcRootFrameDepth;
    TZrUInt32 savedHandlerCount = state->exceptionHandlerStackLength;
    TZrUInt32 savedYieldCount = state->nestedNativeCallYieldFlag;
    TZrMemoryOffset cleanupBoundary;
    TZrBool hasBase = savedCallInfo != ZR_NULL &&
                     savedCallInfo->functionBase.valuePointer != ZR_NULL;
    TZrBool hasTop = savedCallInfo != ZR_NULL &&
                    savedCallInfo->functionTop.valuePointer != ZR_NULL;
    TZrBool hasReturn = savedCallInfo != ZR_NULL &&
                       savedCallInfo->hasReturnDestination &&
                       savedCallInfo->returnDestination != ZR_NULL;
    EZrThreadStatus status;

    ZrCore_Function_StackAnchorInit(state, state->stackTop.valuePointer, &stackTop);
    cleanupBoundary = ZrCore_Stack_SavePointerAsOffset(
            state, hasTop ? savedCallInfo->functionTop.valuePointer
                          : state->stackTop.valuePointer);
    if (hasBase) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->functionBase.valuePointer, &functionBase);
    }
    if (hasTop) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->functionTop.valuePointer, &functionTop);
    }
    if (hasReturn) {
        ZrCore_Function_StackAnchorInit(
                state, savedCallInfo->returnDestination, &returnDestination);
    }

    ZrCore_Exception_ClearCurrent(state);
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    status = ZrCore_Exception_TryRun(state, callback, argument);
    if (status == ZR_THREAD_STATUS_FINE) {
        status = state->threadStatus;
    }
    if (status != ZR_THREAD_STATUS_FINE) {
        ownership_resource_capture_failure(state, failure, status);
        state->aotGcRootFrameStack = savedRootFrame;
        state->aotGcRootFrameDepth = savedRootDepth;
        /* 关闭记录先出链后执行，保证回调重入时不会重复领取同一项。 */
        do {
            ZrCore_Exception_ClearCurrent(state);
            state->threadStatus = ZR_THREAD_STATUS_FINE;
            (void)ZrCore_Exception_TryRun(
                    state, ownership_resource_close_callback_values, &cleanupBoundary);
            state->aotGcRootFrameStack = savedRootFrame;
            state->aotGcRootFrameDepth = savedRootDepth;
        } while (state->toBeClosedValueList.valuePointer >=
                 ZrCore_Stack_LoadOffsetToPointer(state, cleanupBoundary));
    }

    {
        EZrThreadStatus handlerStatus =
                execution_discard_exception_handlers_to_depth(state, savedHandlerCount);
        if (handlerStatus != ZR_THREAD_STATUS_FINE) {
            ownership_resource_capture_failure(state, failure, handlerStatus);
            if (status == ZR_THREAD_STATUS_FINE) {
                status = handlerStatus;
            }
        }
    }
    while (state->pendingControl.kind != ZR_VM_PENDING_CONTROL_NONE ||
           state->pendingControl.hasValue) {
        EZrThreadStatus clearStatus;
        ZrCore_Exception_ClearCurrent(state);
        state->threadStatus = ZR_THREAD_STATUS_FINE;
        clearStatus = ZrCore_Exception_TryRun(
                state, ownership_resource_clear_callback_pending, ZR_NULL);
        if (clearStatus != ZR_THREAD_STATUS_FINE) {
            ownership_resource_capture_failure(state, failure, clearStatus);
            state->aotGcRootFrameStack = savedRootFrame;
            state->aotGcRootFrameDepth = savedRootDepth;
            if (status == ZR_THREAD_STATUS_FINE) {
                status = clearStatus;
            }
        }
    }

    state->callInfoList = savedCallInfo;
    state->stackTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &stackTop);
    state->nestedNativeCallYieldFlag = savedYieldCount;
    if (hasBase) {
        savedCallInfo->functionBase.valuePointer =
                ZrCore_Function_StackAnchorRestore(state, &functionBase);
    }
    if (hasTop) {
        savedCallInfo->functionTop.valuePointer =
                ZrCore_Function_StackAnchorRestore(state, &functionTop);
    }
    if (hasReturn) {
        savedCallInfo->returnDestination =
                ZrCore_Function_StackAnchorRestore(state, &returnDestination);
    }
    return status;
}

/** @brief 以 borrowed self 调用资源的析构元方法，避免回调取得新 owner。 */
static void ownership_resource_call_destructor(SZrState *state, TZrPtr argument) {
    SZrTypeValue borrowedSelf;
    ZrCore_Value_InitAsRawObject(state, &borrowedSelf, (SZrRawObject *)argument);
    borrowedSelf.ownershipKind = ZR_OWNERSHIP_VALUE_KIND_BORROWED;
    (void)ZrCore_Value_CallMetaMethod(
            state, &borrowedSelf, ZR_META_DESTRUCTOR, ZR_NULL, 0U);
}

/** @brief 释放对象各原型层声明为 managed 的字段；逐项失败由外层继续收尾。 */
static void ownership_resource_drop_fields(SZrState *state, TZrPtr argument) {
    ZrCore_Object_DropManagedFields(state, (SZrObject *)argument);
}

/**
 * @brief 执行 resource 的一次性析构及字段释放，并保留调用者异常状态。
 * @note state/object 可为空；仅 resource 会执行 drop，其他输入按无操作处理。
 * @return 回调清理失败时仍完成剩余字段释放并返回首个失败；建立 AOT root frame
 *         失败会在开始 drop 前返回 MEMORY_ERROR。
 * @note 仅最后 owner、GC box 回收和构造展开等责任终结路径应调用；对象在回调期间
 *       保持 rooted 与 pinned，防止重入或 GC 提前回收。
 */
EZrThreadStatus ZrCore_OwnershipResource_DropProtected(
        SZrState *state, SZrRawObject *object) {
    static const SZrAotGcRootSlot stateRoots[] = {
        {0u, 0u, 0u, 0u, ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u},
        {0u, (TZrUInt32)sizeof(SZrRawObject *), 0u, 0u,
         ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u},
        {0u, (TZrUInt32)(2u * sizeof(SZrRawObject *)), 0u, 0u,
         ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS, 0u, 0u}
    };
    static const SZrAotGcRootMap stateRootMap = {3u, stateRoots};
    EZrResourceLifecycleState previousState;
    SZrMeta *destructor;
    SZrRawObject *rootedValues[3] = {ZR_NULL, ZR_NULL, ZR_NULL};
    SZrAotGcRootFrame stateRootFrame;
    SZrResourceDropFailure failure;
    SZrTypeValue savedException;
    SZrVmPendingControl savedPending;
    EZrThreadStatus savedExceptionStatus;
    EZrThreadStatus savedThreadStatus;
    TZrBool hadException;
    TZrBool addedPin;

    if (state == ZR_NULL || !ZrCore_OwnershipResource_IsObject(object)) {
        return ZR_THREAD_STATUS_FINE;
    }
    previousState = (EZrResourceLifecycleState)object->resourceLifecycleState;
    /* 重入 drop 不重复调用析构；外层调用仍拥有最终清理责任。 */
    if (previousState == ZR_RESOURCE_LIFECYCLE_DROPPING ||
        previousState == ZR_RESOURCE_LIFECYCLE_DROPPED) {
        return ZR_THREAD_STATUS_FINE;
    }

    savedException = state->currentException;
    savedExceptionStatus = state->currentExceptionStatus;
    savedThreadStatus = state->threadStatus;
    hadException = state->hasCurrentException;
    savedPending = state->pendingControl;
    if (hadException && ZrCore_Value_IsGarbageCollectable(&savedException)) {
        rootedValues[0] = ZrCore_Value_GetRawObject(&savedException);
    }
    if (savedPending.hasValue && ZrCore_Value_IsGarbageCollectable(&savedPending.value)) {
        rootedValues[2] = ZrCore_Value_GetRawObject(&savedPending.value);
    }
    if (!ZrCore_Gc_AotRootFramePush(
                state, &stateRootFrame,
                (TZrStackValuePointer)rootedValues, &stateRootMap)) {
        return ZR_THREAD_STATUS_MEMORY_ERROR;
    }
    /* 回调暂时独占线程的 pending-control 槽；结束时必须恢复原记录。 */
    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_NONE;
    state->pendingControl.callInfo = ZR_NULL;
    state->pendingControl.targetInstructionOffset = 0;
    state->pendingControl.valueSlot = 0;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
    failure.status = ZR_THREAD_STATUS_FINE;
    failure.root = &rootedValues[1];
    failure.hasException = ZR_FALSE;
    failure.exceptionStatus = ZR_THREAD_STATUS_FINE;
    ZrCore_Value_ResetAsNull(&failure.exception);
    addedPin = (TZrBool)((object->garbageCollectMark.pinFlags &
                         ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE) == 0u);
    /* 保持 domain root 并增加临时 pin，覆盖析构重入、异常及字段释放。 */
    ZrCore_GarbageCollector_PinObject(
            state, object, ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
    object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_DROPPING;
    destructor = ZrCore_Object_GetMetaRecursively(
            state->global, (SZrObject *)object, ZR_META_DESTRUCTOR);
    /* 仅完整构造过的对象运行用户析构；构造展开只释放已初始化字段。 */
    if (previousState == ZR_RESOURCE_LIFECYCLE_ALIVE &&
        destructor != ZR_NULL && destructor->function != ZR_NULL) {
        (void)ownership_resource_run_callback(
                state, ownership_resource_call_destructor, object, &failure);
    }
    /* ReleaseValue 先消费字段槽再传播失败，因此重跑只处理尚未释放的字段。 */
    while (ownership_resource_run_callback(
                   state, ownership_resource_drop_fields, object, &failure) !=
           ZR_THREAD_STATUS_FINE) {
    }
    object->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_DROPPED;
    ZrCore_GcDomain_UnregisterOwnershipRoot(state, object);
    if (addedPin) {
        object->garbageCollectMark.pinFlags &=
                (TZrUInt32)~((TZrUInt32)ZR_GARBAGE_COLLECT_PIN_KIND_NATIVE_HANDLE);
    }
    state->pendingControl = savedPending;
    if (rootedValues[2] != ZR_NULL) {
        state->pendingControl.value.value.object = rootedValues[2];
    }
    if (failure.status != ZR_THREAD_STATUS_FINE) {
        state->currentException = failure.exception;
        if (rootedValues[1] != ZR_NULL) {
            state->currentException.value.object = rootedValues[1];
        }
        state->currentExceptionStatus = failure.exceptionStatus;
        state->hasCurrentException = failure.hasException;
        state->threadStatus = failure.status;
    } else {
        state->currentException = savedException;
        if (rootedValues[0] != ZR_NULL) {
            state->currentException.value.object = rootedValues[0];
        }
        state->currentExceptionStatus = savedExceptionStatus;
        state->hasCurrentException = hadException;
        state->threadStatus = savedThreadStatus;
    }
    (void)ZrCore_Gc_AotRootFramePop(state, &stateRootFrame);
    return failure.status;
}

/**
 * @brief 普通释放入口：完成受保护 drop 后按既有 VM 约定重新抛出失败。
 * @note 需要在释放失败后继续整理 ownership 控制块的调用方应使用 DropProtected。
 */
void ZrCore_OwnershipResource_Drop(SZrState *state, SZrRawObject *object) {
    EZrThreadStatus status = ZrCore_OwnershipResource_DropProtected(state, object);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_Throw(state, status);
    }
}
