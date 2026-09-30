#ifndef ZR_VM_CORE_EXEC_IR_VERIFY_CONSTANTS_H
#define ZR_VM_CORE_EXEC_IR_VERIFY_CONSTANTS_H

#include "zr_vm_core/exec_ir.h"

/* The module and function must have passed structural/full verification. */
TZrBool zr_exec_ir_verify_owned_constants(
        const SZrExecIrModule *module, const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic);

#endif
