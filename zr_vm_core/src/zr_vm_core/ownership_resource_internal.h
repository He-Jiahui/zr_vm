#ifndef ZR_VM_CORE_OWNERSHIP_RESOURCE_INTERNAL_H
#define ZR_VM_CORE_OWNERSHIP_RESOURCE_INTERNAL_H

#include "zr_vm_core/ownership.h"

TZrBool ZrCore_OwnershipResource_IsObject(const SZrRawObject *object);
TZrBool ZrCore_OwnershipResource_IsDirectUniqueValue(const SZrTypeValue *value);
TZrBool ZrCore_OwnershipResource_InitUnique(struct SZrState *state,
                                             SZrTypeValue *destination,
                                             SZrRawObject *object);
TZrBool ZrCore_OwnershipResource_MoveUnique(struct SZrState *state,
                                             SZrTypeValue *destination,
                                             SZrTypeValue *source);
void ZrCore_OwnershipResource_CopyUnique(SZrTypeValue *destination,
                                         const SZrTypeValue *source);
void ZrCore_OwnershipResource_Drop(struct SZrState *state, SZrRawObject *object);

#endif
