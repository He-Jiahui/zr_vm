#include "zr_vm_parser/exec_ir_builder.h"

TZrBool ZrParser_ExecIr_VerifyFunction(const SZrExecIrFunction *function,
                                       EZrExecIrVerifyLevel level,
                                       SZrExecIrDiagnostic *diagnostic) {
    return ZrCore_ExecIr_VerifyFunction(function, level, diagnostic);
}

TZrBool ZrParser_ExecIr_Verify(const SZrExecIrModule *module,
                               SZrExecIrDiagnostic *diagnostic) {
    return ZrCore_ExecIr_VerifyModule(module, diagnostic);
}
