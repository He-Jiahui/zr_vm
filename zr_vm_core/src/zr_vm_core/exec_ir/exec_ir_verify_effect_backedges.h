#ifndef ZR_VM_CORE_EXEC_IR_VERIFY_EFFECT_BACKEDGES_H
#define ZR_VM_CORE_EXEC_IR_VERIFY_EFFECT_BACKEDGES_H

#include "zr_vm_core/exec_ir.h"

TZrBool zr_exec_ir_classify_backedges(
        const SZrExecIrFunction *function,
        TZrBool **outBackedges,
        SZrExecIrDiagnostic *diagnostic);

#endif
