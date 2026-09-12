#include "zr_vm_core/exec_ir.h"

#include <string.h>

static void zr_exec_ir_effect_diag(SZrExecIrDiagnostic *diagnostic,
                                   EZrExecutionDiagnosticCode code,
                                   const SZrExecIrFunction *function,
                                   TZrUInt32 blockId,
                                   TZrUInt32 instructionId,
                                   TZrUInt32 expected,
                                   TZrUInt32 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function->functionToken;
    diagnostic->blockId = blockId;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = instructionId != 0u && instructionId <= function->instructionCount
                               ? function->instructions[instructionId - 1u].sourceId
                               : 0u;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
}

static TZrBool zr_exec_ir_block_has_predecessor(const SZrExecIrFunction *function,
                                                const SZrExecIrBlock *block,
                                                TZrExecIrBlockId predecessor) {
    TZrUInt32 index;
    for (index = block->predecessors.start;
         index < block->predecessors.start + block->predecessors.count;
         ++index) {
        if (function->predecessors[index] == predecessor) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_exec_ir_verify_phi_predecessors(const SZrExecIrFunction *function,
                                                  SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 blockIndex;
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 phiIndex;
        for (phiIndex = block->phis.start;
             phiIndex < block->phis.start + block->phis.count;
             ++phiIndex) {
            const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
            TZrUInt32 incomingIndex;
            if (phi->incomings.count != block->predecessors.count) {
                zr_exec_ir_effect_diag(diagnostic,
                                       ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                       function,
                                       block->id,
                                       0u,
                                       block->predecessors.count,
                                       phi->incomings.count);
                return ZR_FALSE;
            }
            for (incomingIndex = phi->incomings.start;
                 incomingIndex < phi->incomings.start + phi->incomings.count;
                 ++incomingIndex) {
                const SZrExecIrPhiIncoming *incoming = &function->phiIncoming[incomingIndex];
                TZrUInt32 duplicateIndex;
                if (!zr_exec_ir_block_has_predecessor(function, block, incoming->predecessor)) {
                    zr_exec_ir_effect_diag(diagnostic,
                                           ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                           function,
                                           block->id,
                                           0u,
                                           block->predecessors.count,
                                           incoming->predecessor);
                    return ZR_FALSE;
                }
                for (duplicateIndex = phi->incomings.start;
                     duplicateIndex < incomingIndex;
                     ++duplicateIndex) {
                    if (function->phiIncoming[duplicateIndex].predecessor == incoming->predecessor) {
                        zr_exec_ir_effect_diag(diagnostic,
                                               ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
                                               function,
                                               block->id,
                                               0u,
                                               block->predecessors.count,
                                               incoming->predecessor);
                        return ZR_FALSE;
                    }
                }
            }
        }
    }
    return ZR_TRUE;
}

static TZrUInt16 zr_exec_ir_required_flags(const SZrExecIrOpcodeInfo *info,
                                           const SZrExecIrInstruction *instruction) {
    TZrUInt16 required = 0u;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_ALLOCATE;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_THROW;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_GC) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_GC;
    if ((info->flags & ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND) != 0u) required |= ZR_EXEC_IR_FLAG_MAY_SUSPEND;
    if (instruction->opcode == ZR_EXEC_IR_OPCODE_CALL && instruction->bindingRow == 0u) {
        required |= ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK;
    }
    return required;
}

TZrBool ZrCore_ExecIr_VerifyEffects(const SZrExecIrFunction *function,
                                    SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrMemoryTokenId latestMemory = ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID;
    TZrExecIrEffectTokenId latestEffect = ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID;
    TZrUInt32 index;

    if (function == ZR_NULL) {
        if (diagnostic != ZR_NULL) {
            memset(diagnostic, 0, sizeof(*diagnostic));
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return ZR_FALSE;
    }
    if (!zr_exec_ir_verify_phi_predecessors(function, diagnostic)) {
        return ZR_FALSE;
    }
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)instruction->opcode);
        TZrUInt16 required;
        TZrUInt32 tokenIndex;
        TZrBool observable;
        if (info == ZR_NULL) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE,
                                   function, 0u, index + 1u, 0u, instruction->opcode);
            return ZR_FALSE;
        }
        required = zr_exec_ir_required_flags(info, instruction);
        if ((instruction->flags & required) != required) {
            zr_exec_ir_effect_diag(diagnostic,
                                   (required & ZR_EXEC_IR_FLAG_MAY_THROW) != 0u
                                       ? ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE
                                       : ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
                                   function, 0u, index + 1u, required, instruction->flags);
            return ZR_FALSE;
        }
        if (info->memoryReads != 0u && instruction->memoryIn.count == 0u) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                   function, 0u, index + 1u, 1u, 0u);
            return ZR_FALSE;
        }
        if (info->memoryWrites != 0u && instruction->memoryOut.count == 0u) {
            zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                   function, 0u, index + 1u, 1u, 0u);
            return ZR_FALSE;
        }
        for (tokenIndex = instruction->memoryIn.start;
             tokenIndex < instruction->memoryIn.start + instruction->memoryIn.count;
             ++tokenIndex) {
            TZrExecIrMemoryTokenId token = function->memoryTokenPool[tokenIndex];
            if (token == ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID ||
                (tokenIndex > instruction->memoryIn.start &&
                 token < function->memoryTokenPool[tokenIndex - 1u]) ||
                (latestMemory != ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID && token < latestMemory)) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                       function, 0u, index + 1u, latestMemory, token);
                return ZR_FALSE;
            }
        }
        for (tokenIndex = instruction->memoryOut.start;
             tokenIndex < instruction->memoryOut.start + instruction->memoryOut.count;
             ++tokenIndex) {
            TZrExecIrMemoryTokenId token = function->memoryTokenPool[tokenIndex];
            if (token == ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID ||
                (latestMemory != ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID && token <= latestMemory)) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
                                       function, 0u, index + 1u,
                                       latestMemory == UINT32_MAX ? latestMemory : latestMemory + 1u,
                                       token);
                return ZR_FALSE;
            }
            if (token > latestMemory) latestMemory = token;
        }
        observable = (TZrBool)(info->memoryWrites != 0u || required != 0u ||
                               (info->effects & ZR_EXEC_IR_EFFECT_DROP) != 0u);
        if (observable) {
            if (instruction->effectIn == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                instruction->effectOut == ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
                instruction->effectOut <= instruction->effectIn ||
                (latestEffect != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID &&
                 instruction->effectIn < latestEffect)) {
                zr_exec_ir_effect_diag(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
                                       function, 0u, index + 1u, latestEffect,
                                       instruction->effectIn);
                return ZR_FALSE;
            }
            latestEffect = instruction->effectOut;
        }
    }
    return ZR_TRUE;
}
