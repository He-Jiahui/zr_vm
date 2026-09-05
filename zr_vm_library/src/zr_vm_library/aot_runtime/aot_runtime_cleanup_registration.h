#ifndef ZR_VM_LIBRARY_AOT_RUNTIME_CLEANUP_REGISTRATION_H
#define ZR_VM_LIBRARY_AOT_RUNTIME_CLEANUP_REGISTRATION_H

#include "zr_vm_library/aot_runtime.h"

/*
 * A generated frame has a dense value slot and may also have a direct VALUE
 * slot for the same logical slot.  AOT cleanup registers the physical slot so
 * member results can replace the dense value without losing the hidden owner.
 */
TZrBool aot_runtime_cleanup_registration_prepare(
        SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 logicalSlot,
        TZrStackValuePointer *outRegistration);

void aot_runtime_cleanup_registration_clear(
        SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 logicalSlot);

void aot_runtime_cleanup_registration_refresh(
        SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 logicalSlot);

#endif
