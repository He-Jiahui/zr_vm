#ifndef ZR_VM_PARSER_EXEC_IR_ALIAS_AVAILABILITY_H
#define ZR_VM_PARSER_EXEC_IR_ALIAS_AVAILABILITY_H

#include "exec_ir/exec_ir_pass_internal.h"
#include <stddef.h>

typedef enum EZrAliasDefinitionKind {
    ZR_ALIAS_MISSING, ZR_ALIAS_EXTERNAL, ZR_ALIAS_INSTRUCTION, ZR_ALIAS_PHI_ENTRY
} EZrAliasDefinitionKind;

typedef struct SZrAliasDefinition {
    EZrAliasDefinitionKind kind;
    TZrExecIrBlockId block;
    TZrExecIrInstructionId instruction;
    TZrUInt32 phi;
} SZrAliasDefinition;

typedef struct SZrAliasAvailability {
    SZrExecIrFunction *function;
    SZrExecIrPassContext *context;
    TZrExecIrBlockId *instructionBlocks;
    TZrExecIrBlockId *stack;
    TZrUInt8 *visited;
    TZrUInt8 *reachable;
    TZrBool valid;
} SZrAliasAvailability;

/* Checked, budgeted zero-initialized private scratch. NULL means allocation
 * failure or budget exhaustion; the latter is distinguished through context. */
void *ZrParser_ExecIr_AliasAllocate(SZrExecIrFunction *function,
        SZrExecIrPassContext *context, TZrUInt64 count, size_t width,
        SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_AliasAvailabilityPrepare(SZrAliasAvailability *proof,
        SZrExecIrFunction *function, SZrExecIrPassContext *context,
        SZrExecIrDiagnostic *diagnostic);
void ZrParser_ExecIr_AliasAvailabilityDispose(SZrAliasAvailability *proof);
/* instruction==0 denotes block entry. A nonzero successor denotes an
 * incoming-edge use at the end of block, including a normal throwing edge. */
TZrBool ZrParser_ExecIr_AliasAvailable(SZrAliasAvailability *proof,
        const SZrAliasDefinition *definition, TZrExecIrBlockId block,
        TZrExecIrInstructionId instruction, TZrExecIrBlockId successor);
TZrBool ZrParser_ExecIr_AliasPhiEdge(SZrAliasAvailability *proof,
        const SZrExecIrBlock *target, TZrUInt32 slot,
        TZrExecIrBlockId predecessor);

#endif
