#ifndef ZR_VM_CORE_GC_TEMPORARY_ROOTS_H
#define ZR_VM_CORE_GC_TEMPORARY_ROOTS_H
#include "zr_vm_core/state.h"
/* State-owner LIFO roots. Raw reserve never collects. Entries remain pinned
 * for the same native pointer contract as a temporary root lease. */
TZrBool ZrCore_Gc_TemporaryRootPush(SZrState *state, SZrRawObject *object);
void ZrCore_Gc_TemporaryRootsRestore(SZrState *state, TZrSize depth);
void ZrCore_Gc_TemporaryRootsFree(SZrState *state);
#endif
