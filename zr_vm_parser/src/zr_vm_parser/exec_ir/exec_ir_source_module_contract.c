#include "zr_vm_parser/exec_ir_source_module_contract.h"

#include <string.h>

/* Feature RED: linkable contract surface; no source admission or publication. */
TZrBool ZrParser_ExecIr_BindSourceModuleContract(
        const SZrCompilerState *compiler, SZrExecIrModule *module,
        SZrExecIrDiagnostic *diagnostic) {
    (void)compiler;
    (void)module;
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
    }
    return ZR_FALSE;
}
