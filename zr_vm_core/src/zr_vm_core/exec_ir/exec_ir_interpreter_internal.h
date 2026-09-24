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

TZrBool zr_oracle_validate(const SZrExecIrFunction *function,
                           SZrExecIrDiagnostic *diagnostic);
TZrBool zr_oracle_value_kind_valid(EZrExecIrOracleValueKind kind);
void zr_oracle_undefined(SZrExecIrOracleValue *value);
TZrBool zr_oracle_exec(const SZrExecIrOracleInput *input,
                       SZrExecIrOracleExecutionResult *result,
                       const SZrExecIrInstruction *instruction,
                       TZrExecIrInstructionId id, TZrExecIrBlockId block,
                       TZrBool *terminated, TZrExecIrBlockId *next,
                       TZrUInt32 *nextOrdinal, SZrExecIrDiagnostic *diagnostic);

typedef struct SZrOracleCursor {
    TZrExecIrBlockId block;
    TZrExecIrBlockId previous;
    TZrUInt32 successorOrdinal;
    TZrUInt32 instructionIndex;
    TZrBool enterBlock;
    TZrBool done;
    TZrBool skipFirstStop;
} SZrOracleCursor;

TZrBool zr_oracle_select_checkpoint(const SZrExecIrOracleInput *input,
                                    const SZrExecIrOracleCheckpoint *point,
                                    const SZrExecIrOracleContinuation *identity,
                                    SZrExecIrMaterializedState *state,
                                    SZrExecIrStateMapEntry *entry,
                                    SZrExecIrDiagnostic *diagnostic);
TZrBool zr_oracle_capture_checkpoint(const SZrExecIrOracleInput *input,
                                     SZrExecIrOracleExecutionResult *result,
                                     const SZrExecIrStateMapEntry *entry,
                                     TZrBool terminated, TZrExecIrBlockId next,
                                     TZrUInt32 ordinal,
                                     SZrExecIrDiagnostic *diagnostic);
TZrBool zr_oracle_prepare_resume(const SZrExecIrOracleInput *input,
                                 const SZrExecIrOracleExecutionResult *state,
                                 SZrExecIrOracleExecutionResult *prepared,
                                 SZrOracleCursor *cursor,
                                 SZrExecIrDiagnostic *diagnostic);

TZrBool zr_oracle_enter(const SZrExecIrFunction *function,
                       TZrExecIrBlockId blockId,
                       TZrExecIrBlockId previous,
                       TZrUInt32 successorOrdinal,
                       SZrExecIrOracleExecutionResult *result,
                       SZrExecIrDiagnostic *diagnostic);

#endif
