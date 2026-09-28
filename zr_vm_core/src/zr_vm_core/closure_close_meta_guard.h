#ifndef ZR_VM_CORE_CLOSURE_CLOSE_META_GUARD_H
#define ZR_VM_CORE_CLOSURE_CLOSE_META_GUARD_H

#include "zr_vm_core/state.h"

/* Invoke an @close callback while an outer exception is being unwound. */
void ZrCore_ClosureCloseMetaGuard_Invoke(SZrState *state,
                                         TZrMemoryOffset boundarySlotOffset,
                                         TZrBool isYield);

#endif
