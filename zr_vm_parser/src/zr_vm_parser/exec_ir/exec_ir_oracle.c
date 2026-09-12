#include "zr_vm_parser/exec_ir_oracle.h"

TZrBool ZrParser_ExecIr_Interpret(
        const SZrExecIrOracleInput *input,
        SZrExecIrOracleExecutionResult *result,
        SZrExecIrDiagnostic *diagnostic) {
    return ZrCore_ExecIr_RunOracleEx(input, result, diagnostic);
}
