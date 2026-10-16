#ifndef ZR_VM_CORE_FUNCTION_IR_OWNERS_INTERNAL_H
#define ZR_VM_CORE_FUNCTION_IR_OWNERS_INTERNAL_H

#include "function/function_aot_ir_internal.h"

/* Releases this record's unique ExecIR storage and clears its owner slot. */
void ZrCore_Function_ReleaseExecIrOwner(SZrFunction *function);

/* Clears only: a proven moved destination already owns this storage. */
void ZrCore_Function_ForgetExecIrOwnerAfterMove(SZrFunction *function);

static inline void ZrCore_Function_ReleaseIrOwners(SZrFunction *function) {
    ZrCore_Function_ReleaseAotIrOwner(function);
    ZrCore_Function_ReleaseExecIrOwner(function);
}

static inline void ZrCore_Function_ForgetIrOwnersAfterMove(SZrFunction *function) {
    ZrCore_Function_ForgetAotIrOwnerAfterMove(function);
    ZrCore_Function_ForgetExecIrOwnerAfterMove(function);
}

#endif /* ZR_VM_CORE_FUNCTION_IR_OWNERS_INTERNAL_H */
