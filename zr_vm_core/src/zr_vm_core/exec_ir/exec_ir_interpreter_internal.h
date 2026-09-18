#ifndef ZR_VM_CORE_EXEC_IR_INTERPRETER_INTERNAL_H
#define ZR_VM_CORE_EXEC_IR_INTERPRETER_INTERNAL_H

#include "zr_vm_core/exec_ir_interpreter.h"

#include <stddef.h>

void zr_oracle_diag(SZrExecIrDiagnostic *diagnostic,
                    EZrExecutionDiagnosticCode code,
                    const SZrExecIrFunction *function,
                    TZrExecIrBlockId blockId,
                    TZrExecIrInstructionId instructionId,
                    TZrExecIrSourceId sourceId,
                    TZrUInt32 expected,
                    TZrUInt32 actual);

TZrBool zr_oracle_bytes(TZrUInt32 count, size_t element, size_t *bytes);

TZrBool zr_oracle_enter(const SZrExecIrFunction *function,
                       TZrExecIrBlockId blockId,
                       TZrExecIrBlockId previous,
                       TZrUInt32 successorOrdinal,
                       SZrExecIrOracleExecutionResult *result,
                       SZrExecIrDiagnostic *diagnostic);

#endif
