#ifndef ZR_VM_CORE_CALL_BINDING_INLINE_FRAME_H
#define ZR_VM_CORE_CALL_BINDING_INLINE_FRAME_H

#include "zr_vm_core/function.h"
#include "zr_vm_core/call_info.h"

/* Validate a borrowed inline receiver's canonical identity without materializing its value. */
TZrBool zr_call_binding_prepare_inline_frame_member(
        struct SZrState *state, SZrCallInfo *callerCallInfo, TZrUInt32 cacheIndex,
        TZrUInt32 sourceSlot, TZrStackValuePointer *ioCallWindow,
        TZrStackValuePointer *ioSourceFrame, TZrStackValuePointer *ioReturnDestination,
        SZrCallBindingDiagnostic *diagnostic);

#endif
