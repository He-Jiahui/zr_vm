#ifndef ZR_VM_PARSER_EXEC_IR_ORACLE_H
#define ZR_VM_PARSER_EXEC_IR_ORACLE_H

#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/conf.h"

/* Parser-facing name used by the SSA pipeline.  The implementation delegates
 * to the core oracle so parser tests and runtime differential tests observe
 * exactly one semantic interpreter. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_Interpret(
        const SZrExecIrOracleInput *input,
        SZrExecIrOracleExecutionResult *result,
        SZrExecIrDiagnostic *diagnostic);

#endif
