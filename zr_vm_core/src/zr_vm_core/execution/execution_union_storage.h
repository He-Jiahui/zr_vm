#ifndef ZR_VM_CORE_EXECUTION_UNION_STORAGE_H
#define ZR_VM_CORE_EXECUTION_UNION_STORAGE_H

#include "zr_vm_core/function.h"
#include "zr_vm_core/object.h"

/* Use the same checked carrier conversion for assignments and call arguments. */
TZrBool execution_inline_frame_materialize_union_carrier_to_inline_storage(
        SZrState *state, const SZrFunction *function, TZrUInt32 typeLayoutId,
        TZrPtr destinationAddress, TZrUInt32 destinationByteSize, SZrObject *carrier);

TZrBool execution_inline_frame_materialize_union_storage_to_carrier(
        SZrState *state, const SZrFunction *function, TZrUInt32 typeLayoutId,
        const void *sourceAddress, TZrUInt32 sourceByteSize, SZrTypeValue *outValue);

#endif
