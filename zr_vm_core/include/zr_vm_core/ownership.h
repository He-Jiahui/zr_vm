//
// Created by Codex on 2026/3/30.
//

#ifndef ZR_VM_CORE_OWNERSHIP_H
#define ZR_VM_CORE_OWNERSHIP_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

struct SZrState;
struct SZrRawObject;

struct ZR_STRUCT_ALIGN SZrOwnershipWeakRef {
    SZrTypeValue *slot;
    TZrMemoryOffset stackSlotOffset;
    TZrBool usesStackSlotOffset;
    struct SZrOwnershipControl *control;
    struct SZrOwnershipWeakRef *next;
};

typedef struct SZrOwnershipWeakRef SZrOwnershipWeakRef;

struct ZR_STRUCT_ALIGN SZrOwnershipControl {
    struct SZrRawObject *object;
    TZrUInt32 strongRefCount;
    TZrBool isDetachedFromGc;
    SZrOwnershipWeakRef *weakRefs;
};

typedef struct SZrOwnershipControl SZrOwnershipControl;

ZR_CORE_API TZrBool ZrCore_Ownership_InitUniqueValue(struct SZrState *state,
                                                     SZrTypeValue *destination,
                                                     struct SZrRawObject *object);

ZR_CORE_API TZrBool ZrCore_Ownership_UniqueValue(struct SZrState *state,
                                                 SZrTypeValue *destination,
                                                 SZrTypeValue *source);

ZR_CORE_API TZrBool ZrCore_Ownership_BorrowValue(struct SZrState *state,
                                                 SZrTypeValue *destination,
                                                 SZrTypeValue *source);

ZR_CORE_API TZrBool ZrCore_Ownership_LoanValue(struct SZrState *state,
                                               SZrTypeValue *destination,
                                               SZrTypeValue *source);

ZR_CORE_API TZrBool ZrCore_Ownership_UsingValue(struct SZrState *state,
                                                SZrTypeValue *destination,
                                                SZrTypeValue *source);

ZR_CORE_API TZrBool ZrCore_Ownership_ShareValue(struct SZrState *state,
                                                SZrTypeValue *destination,
                                                SZrTypeValue *source);

ZR_CORE_API TZrBool ZrCore_Ownership_WeakValue(struct SZrState *state,
                                               SZrTypeValue *destination,
                                               SZrTypeValue *source);

ZR_CORE_API TZrBool ZrCore_Ownership_UpgradeValue(struct SZrState *state,
                                                  SZrTypeValue *destination,
                                                  SZrTypeValue *source);

ZR_CORE_API TZrBool ZrCore_Ownership_DetachValue(struct SZrState *state,
                                                 SZrTypeValue *destination,
                                                 SZrTypeValue *source);

ZR_CORE_API TZrBool ZrCore_Ownership_ReturnToGcValue(struct SZrState *state,
                                                     SZrTypeValue *destination,
                                                     SZrTypeValue *source);

ZR_CORE_API void ZrCore_Ownership_ReleaseValue(struct SZrState *state, SZrTypeValue *value);

ZR_CORE_API TZrUInt32 ZrCore_Ownership_GetStrongRefCount(struct SZrRawObject *object);

ZR_CORE_API void ZrCore_Ownership_AssignValue(struct SZrState *state,
                                              SZrTypeValue *destination,
                                              const SZrTypeValue *source);

ZR_CORE_API void ZrCore_Ownership_NotifyObjectReleased(struct SZrState *state,
                                                       struct SZrRawObject *object);

ZR_CORE_API TZrInt64 ZrCore_Ownership_NativeUnique(struct SZrState *state);
ZR_CORE_API TZrInt64 ZrCore_Ownership_NativeShared(struct SZrState *state);
ZR_CORE_API TZrInt64 ZrCore_Ownership_NativeWeak(struct SZrState *state);
ZR_CORE_API TZrInt64 ZrCore_Ownership_NativeUsing(struct SZrState *state);

#endif // ZR_VM_CORE_OWNERSHIP_H
