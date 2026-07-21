#ifndef ZR_VM_CORE_OWNERSHIP_SHARED_INTERNAL_H
#define ZR_VM_CORE_OWNERSHIP_SHARED_INTERNAL_H

#include "zr_vm_core/ownership.h"

struct SZrRawObject;
struct SZrState;

SZrOwnershipControl *ZrCore_OwnershipShared_GetOrCreateControl(
        struct SZrState *state,
        struct SZrRawObject *object);

TZrBool ZrCore_OwnershipShared_IsInIsolationDomain(
        const struct SZrState *state,
        const SZrOwnershipControl *control);

TZrBool ZrCore_OwnershipShared_RetainStrong(
        struct SZrState *state,
        SZrOwnershipControl *control);

TZrBool ZrCore_OwnershipShared_SetInitialStrong(
        struct SZrState *state,
        SZrOwnershipControl *control,
        TZrUInt32 *outPreviousCount);

TZrBool ZrCore_OwnershipShared_ReleaseStrong(
        struct SZrState *state,
        SZrOwnershipControl *control,
        struct SZrRawObject **outFinalObject);

void ZrCore_OwnershipShared_FinishFinalStrong(
        struct SZrState *state,
        SZrOwnershipControl *control,
        struct SZrRawObject *object);

TZrBool ZrCore_OwnershipShared_RetainWeak(
        struct SZrState *state,
        SZrOwnershipControl *control);

void ZrCore_OwnershipShared_ReleaseWeak(
        struct SZrState *state,
        SZrOwnershipControl *control);

void ZrCore_OwnershipShared_InvalidateObject(
        struct SZrState *state,
        SZrOwnershipControl *control,
        struct SZrRawObject *object);

#endif
