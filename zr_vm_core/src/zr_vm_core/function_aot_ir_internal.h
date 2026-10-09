#ifndef ZR_VM_CORE_FUNCTION_AOT_IR_INTERNAL_H
#define ZR_VM_CORE_FUNCTION_AOT_IR_INTERNAL_H

#include "zr_vm_core/function.h"

/* Release this record's unique owner and clear its slot; absence is idempotent. */
void ZrCore_Function_ReleaseAotIrOwner(SZrFunction *function);

/* NULL only: the moved destination already owns the arena. Never free it here. */
void ZrCore_Function_ForgetAotIrOwnerAfterMove(SZrFunction *function);

#endif /* ZR_VM_CORE_FUNCTION_AOT_IR_INTERNAL_H */
