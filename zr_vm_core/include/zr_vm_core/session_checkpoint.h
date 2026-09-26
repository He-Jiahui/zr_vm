#ifndef ZR_VM_CORE_SESSION_CHECKPOINT_H
#define ZR_VM_CORE_SESSION_CHECKPOINT_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrSessionCheckpoint;

typedef struct SZrSessionCheckpoint SZrSessionCheckpoint;

/*
 * Capture and restore the retained session at a quiescent VM boundary.  The
 * checkpoint owns GC handles and a logical copy of every reachable mutable
 * value; it is not a byte-for-byte heap clone.
 */
ZR_CORE_API TZrBool ZrCore_SessionCheckpoint_Create(
        struct SZrState *state,
        SZrSessionCheckpoint **outCheckpoint);
ZR_CORE_API TZrBool ZrCore_SessionCheckpoint_Rollback(
        struct SZrState *state,
        const SZrSessionCheckpoint *checkpoint);
ZR_CORE_API void ZrCore_SessionCheckpoint_Free(
        struct SZrState *state,
        SZrSessionCheckpoint *checkpoint);

#endif
