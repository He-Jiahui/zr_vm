#include "ownership_shared_internal.h"

#include <stdint.h>

#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/state.h"

static TZrUInt64 ownership_shared_isolation_domain_id(const SZrState *state) {
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
    if (state == ZR_NULL || control == ZR_NULL ||
        control->strongRefCount != 0U ||
        control->weakRefCount != 0U ||
        control->dropInProgress) {
        return;
    }
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
    if (object->ownershipControl != ZR_NULL) {
        control = object->ownershipControl;
        return ZrCore_OwnershipShared_IsInIsolationDomain(state, control)
                       ? control
                       : ZR_NULL;
    }

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
