#ifndef ZR_VM_CORE_OBJECT_WRITE_INTERNAL_H
#define ZR_VM_CORE_OBJECT_WRITE_INTERNAL_H

#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"

/* Storage options preserve the existing stack/new-owner operand lanes. All
 * failure paths return false after releasing transaction-owned root leases. */
TZrBool ZrCore_Object_TrySetValueWithStorageOptions(
        SZrState *state, SZrObject *object, const SZrTypeValue *key,
        const SZrTypeValue *value, TZrBool objectStackRooted,
        TZrBool keyStackRooted, TZrBool valueStackRooted, TZrBool skipWriteBarrier);

/* Representation and lookup-cache details remain owned by object.c. These
 * bridges are only called by the protected storage-write transaction. A new
 * pair receives an already-copied value by transfer; existing pairs keep their
 * original overwrite semantics. */
TZrBool ZrCore_Object_PrepareGenericStorageWrite(
        SZrState *state, SZrObject *object, const SZrTypeValue *key, TZrBool *skipWriteBarrier);
void ZrCore_Object_CommitStoragePair(
        SZrState *state, SZrObject *object, const SZrTypeValue *key,
        const SZrTypeValue *value, SZrHashKeyValuePair *pair,
        TZrBool newPair, TZrBool skipWriteBarrier);

#endif
