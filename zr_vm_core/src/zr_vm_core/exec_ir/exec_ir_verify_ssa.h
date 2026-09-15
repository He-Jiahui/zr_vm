#ifndef ZR_VM_CORE_EXEC_IR_VERIFY_SSA_H
#define ZR_VM_CORE_EXEC_IR_VERIFY_SSA_H

#include "zr_vm_core/exec_ir.h"

TZrBool zr_exec_ir_verify_ssa(const SZrExecIrFunction *function,
                              SZrExecIrDiagnostic *diagnostic);

#endif
