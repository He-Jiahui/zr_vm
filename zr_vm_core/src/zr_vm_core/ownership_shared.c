/* ownership.c 编排值槽、GC 根和资源析构；此处只维护域内控制块的引用生命周期。 */
#include "ownership_shared_internal.h"

#include <stdint.h>

#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/state.h"

static TZrUInt64 ownership_shared_isolation_domain_id(const SZrState *state) {
    /* 当前域身份取自创建者 state；取得、增减引用的入口据此拒绝跨域操作。 */
    return (TZrUInt64)(uintptr_t)state;
}

static void ownership_shared_free_control(SZrState *state,
                                          SZrOwnershipControl *control) {
    if (state == ZR_NULL || state->global == ZR_NULL || control == ZR_NULL) {
        return;
    }
    ZrCore_Memory_RawFreeWithType(state->global,
                                  control,
                                  sizeof(*control),
                                  ZR_MEMORY_NATIVE_TYPE_OBJECT);
}

static void ownership_shared_try_free_control(SZrState *state,
                                              SZrOwnershipControl *control) {
    /* 最终 strong 释放期间仍需保留控制块，供资源 drop 结束后的收尾使用。 */
    if (state == ZR_NULL || control == ZR_NULL ||
        control->strongRefCount != 0U ||
        control->weakRefCount != 0U ||
        control->dropInProgress) {
        return;
    }
    /* 清理对象的反向关联后，GC 才能把无 owner 的对象按常规规则处理。 */
    if (control->object != ZR_NULL &&
        control->object->ownershipControl == control) {
        control->object->ownershipControl = ZR_NULL;
    }
    ownership_shared_free_control(state, control);
}

SZrOwnershipControl *ZrCore_OwnershipShared_GetOrCreateControl(
        SZrState *state,
        SZrRawObject *object) {
    SZrOwnershipControl *control;

    if (state == ZR_NULL || state->global == ZR_NULL || object == ZR_NULL) {
        return ZR_NULL;
    }
    /* 原对象缓存控制块，使 plain->shared 与后续 unique 转换使用同一份引用状态；
     * 已绑定其他域的对象不能在本域再建一块独立控制块。 */
    if (object->ownershipControl != ZR_NULL) {
        control = object->ownershipControl;
        return ZrCore_OwnershipShared_IsInIsolationDomain(state, control)
                       ? control
                       : ZR_NULL;
    }

    /* BUG: 未持控制块的对象未核真实 GC 域就绑定本域；外域对象随后可进错误 collector 的 ignored registry，原域回收后留下悬空根。 */
    control = (SZrOwnershipControl *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(*control),
            ZR_MEMORY_NATIVE_TYPE_OBJECT);
    if (control == ZR_NULL) {
        return ZR_NULL;
    }
    control->object = object;
    control->strongRefCount = 0U;
    control->weakRefCount = 0U;
    control->isolationDomainId = ownership_shared_isolation_domain_id(state);
    control->objectIsAlive = ZR_TRUE;
    control->dropInProgress = ZR_FALSE;
    control->usesAtomicRefCounts = ZR_FALSE;
    control->ownsGcIgnore = ZR_FALSE;
    object->ownershipControl = control;
    return control;
}

TZrBool ZrCore_OwnershipShared_IsInIsolationDomain(
        const SZrState *state,
        const SZrOwnershipControl *control) {
    return state != ZR_NULL &&
           control != ZR_NULL &&
           control->isolationDomainId == ownership_shared_isolation_domain_id(state);
}

TZrBool ZrCore_OwnershipShared_RetainStrong(
        SZrState *state,
        SZrOwnershipControl *control) {
    if (!ZrCore_OwnershipShared_IsInIsolationDomain(state, control) ||
        !control->objectIsAlive ||
        control->dropInProgress ||
        control->usesAtomicRefCounts ||
        control->strongRefCount == UINT32_MAX) {
        return ZR_FALSE;
    }
    /* 第一份 strong 同时持有隐含 weak，使最终 drop 与显式 weak 的寿命可分离。 */
    if (control->strongRefCount == 0U) {
        if (control->weakRefCount == UINT32_MAX) {
            return ZR_FALSE;
        }
        control->weakRefCount++;
    }
    control->strongRefCount++;
    return ZR_TRUE;
}

TZrBool ZrCore_OwnershipShared_SetInitialStrong(
        SZrState *state,
        SZrOwnershipControl *control,
        TZrUInt32 *outPreviousCount) {
    TZrUInt32 previousCount;

    if (!ZrCore_OwnershipShared_IsInIsolationDomain(state, control) ||
        !control->objectIsAlive ||
        control->dropInProgress ||
        control->usesAtomicRefCounts) {
        return ZR_FALSE;
    }
    /* BUG: SharePlainValue 保留 plain 源；随后 UniqueValue 可从它复用同一控制块。
     * 已有 shared 时这里把计数重置为 1，先释放的 owner 撤 GC 根并使对象身份失效；
     * 有显式 weak 时块残留，否则被释放。另一 owner 的 strong 均已失效。 */
    previousCount = control->strongRefCount;
    if (previousCount == 0U) {
        if (control->weakRefCount == UINT32_MAX) {
            return ZR_FALSE;
        }
        control->weakRefCount++;
    }
    control->strongRefCount = 1U;
    if (outPreviousCount != ZR_NULL) {
        *outPreviousCount = previousCount;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_OwnershipShared_ReleaseStrong(
        SZrState *state,
        SZrOwnershipControl *control,
        SZrRawObject **outFinalObject) {
    if (outFinalObject != ZR_NULL) {
        *outFinalObject = ZR_NULL;
    }
    if (!ZrCore_OwnershipShared_IsInIsolationDomain(state, control) ||
        control->usesAtomicRefCounts ||
        control->strongRefCount == 0U) {
        return ZR_FALSE;
    }

    control->strongRefCount--;
    if (control->strongRefCount != 0U) {
        return ZR_FALSE;
    }

    if (outFinalObject != ZR_NULL) {
        *outFinalObject = control->object;
    }
    /* 最后 strong 先让 weak 观察到失效，再把对象交给上层 drop 或归还 GC。 */
    control->objectIsAlive = ZR_FALSE;
    control->dropInProgress = ZR_TRUE;
    control->object = ZR_NULL;
    return ZR_TRUE;
}

void ZrCore_OwnershipShared_FinishFinalStrong(
        SZrState *state,
        SZrOwnershipControl *control,
        SZrRawObject *object) {
    if (state == ZR_NULL || control == ZR_NULL) {
        return;
    }
    if (object != ZR_NULL && object->ownershipControl == control) {
        object->ownershipControl = ZR_NULL;
    }
    /* 上层清理完对象后才释放隐含 weak；显式 weak 仍可持有已失效控制块。 */
    control->dropInProgress = ZR_FALSE;
    if (control->weakRefCount > 0U) {
        control->weakRefCount--;
    }
    ownership_shared_try_free_control(state, control);
}

TZrBool ZrCore_OwnershipShared_RetainWeak(
        SZrState *state,
        SZrOwnershipControl *control) {
    if (!ZrCore_OwnershipShared_IsInIsolationDomain(state, control) ||
        control->usesAtomicRefCounts ||
        control->weakRefCount == 0U ||
        control->weakRefCount == UINT32_MAX) {
        return ZR_FALSE;
    }
    control->weakRefCount++;
    return ZR_TRUE;
}

void ZrCore_OwnershipShared_ReleaseWeak(
        SZrState *state,
        SZrOwnershipControl *control) {
    if (!ZrCore_OwnershipShared_IsInIsolationDomain(state, control) ||
        control->usesAtomicRefCounts ||
        control->weakRefCount == 0U) {
        return;
    }
    control->weakRefCount--;
    ownership_shared_try_free_control(state, control);
}

void ZrCore_OwnershipShared_InvalidateObject(
        SZrState *state,
        SZrOwnershipControl *control,
        SZrRawObject *object) {
    TZrBool hadStrong;

    if (!ZrCore_OwnershipShared_IsInIsolationDomain(state, control)) {
        return;
    }
    /* GC 回调令控制块和对象断开；外部 weak 保留身份但无法再唤醒。 */
    hadStrong = (TZrBool)(control->strongRefCount > 0U);
    control->strongRefCount = 0U;
    control->object = ZR_NULL;
    control->objectIsAlive = ZR_FALSE;
    control->dropInProgress = ZR_FALSE;
    control->ownsGcIgnore = ZR_FALSE;
    if (object != ZR_NULL && object->ownershipControl == control) {
        object->ownershipControl = ZR_NULL;
    }
    if (hadStrong && control->weakRefCount > 0U) {
        control->weakRefCount--;
    }
    ownership_shared_try_free_control(state, control);
}
