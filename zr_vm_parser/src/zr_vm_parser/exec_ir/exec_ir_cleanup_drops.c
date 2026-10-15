#include "zr_vm_parser/exec_ir_state_maps.h"
#include "zr_vm_core/exec_ir_owner_state.h"

#include <string.h>

static TZrBool zr_cleanup_failure(const SZrExecIrFunction *function,
                                  SZrExecIrDiagnostic *diagnostic,
                                  EZrExecutionDiagnosticCode code,
                                  TZrExecIrInstructionId id) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
        diagnostic->instructionId = id;
        if (function != ZR_NULL && id != 0u && id <= function->instructionCount)
            diagnostic->sourceId = function->instructions[id - 1u].sourceId;
    }
    return ZR_FALSE;
}

TZrBool ZrParser_ExecIr_ElaborateCleanupDrops(
        SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrFunction candidate;
    SZrExecIrOwnerAnalysis ownership = {0};
    TZrUInt32 blockIndex, index;
    TZrBool changed = ZR_FALSE;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (function == ZR_NULL)
        return zr_cleanup_failure(function, diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    if (function->sealed)
        return zr_cleanup_failure(function, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_SEALED, 0u);
    /* The source may intentionally lack payload dominance at a cleanup join.
     * Its pools/CFG/effect contract must still be well formed before cloning. */
    if (!ZrCore_ExecIr_VerifyFunction(function,
            ZR_EXEC_IR_VERIFY_STRUCTURE | ZR_EXEC_IR_VERIFY_EFFECT, diagnostic)) return ZR_FALSE;
    ZrCore_ExecIr_FunctionInit(&candidate);
    if (!ZrCore_ExecIr_CloneFunction(function, &candidate, diagnostic)) return ZR_FALSE;
    for (blockIndex = 0u; blockIndex < candidate.blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &candidate.blocks[blockIndex];
        if ((block->flags & ZR_EXEC_IR_BLOCK_FLAG_CLEANUP) == 0u) continue;
        for (index = block->instructionRange.start;
             index < block->instructionRange.start + block->instructionRange.count; ++index) {
            SZrExecIrInstruction *instruction = &candidate.instructions[index];
            if (instruction->opcode != ZR_EXEC_IR_OPCODE_DROP) continue;
            instruction->opcode = ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED;
            changed = ZR_TRUE;
            if (!ZrCore_ExecIr_ConditionalCleanupValid(&candidate, index + 1u)) {
                zr_cleanup_failure(&candidate, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, index + 1u);
                goto failure;
            }
        }
    }
    if (!ZrCore_ExecIr_VerifyFunction(&candidate, ZR_EXEC_IR_VERIFY_ALL, diagnostic) ||
        !ZrCore_ExecIr_OwnerAnalysisBuild(&candidate, &ownership, diagnostic)) goto failure;
    /* Analyze the elaborated graph, including loop resets and exceptional
     * definitions, before publishing any new instruction or map. */
    for (index = 0u; index < candidate.instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &candidate.instructions[index];
        if (instruction->opcode == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED && ownership.reachable[index] &&
            ZrCore_ExecIr_OwnerStateMaskAt(&ownership, index + 1u,
                    candidate.operands[instruction->operandRange.start],
                    ZR_EXEC_IR_STATE_BEFORE_EFFECT) == 0u) {
            zr_cleanup_failure(&candidate, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID, index + 1u);
            goto failure;
        }
    }
    ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
    if (changed) {
        if (candidate.contract.generation == UINT64_MAX) {
            zr_cleanup_failure(&candidate, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u);
            goto failure;
        }
        ++candidate.contract.generation;
    }
    if (!ZrParser_ExecIr_BuildStateMaps(&candidate, diagnostic)) goto failure;
    ZrCore_ExecIr_FreeFunction(function);
    *function = candidate;
    return ZR_TRUE;
failure:
    ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
    ZrCore_ExecIr_FreeFunction(&candidate);
    return ZR_FALSE;
}

/**
 * @fn TZrBool zr_cleanup_failure(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, TZrExecIrInstructionId id)
 *
 * ## Summary
 * 统一构造清理展开失败诊断并返回假值，供早期参数检查及候选函数校验失败路径传播。
 *
 * ## Parameters
 * @param[in] function 可选的当前或候选函数；非空时用于提供 functionToken，并在 id 有效时读取对应 sourceId。
 * @param[out] diagnostic 可选诊断输出；非空时先清零再填写 code、functionToken、instructionId 和可取得的 sourceId。
 * @param[in] code 调用点已确定的失败类别。
 * @param[in] id 零表示函数级失败；非零时按 ExecIR 的一基指令编号解释。
 *
 * ## Returns
 * @return 始终返回 ZR_FALSE，便于调用方直接结束失败路径；不分配或转移资源。
 *
 * ## Constraints and lifecycle
 * function 与 diagnostic 仅在本次同步调用期间借用。只有 function 非空且 id 位于现有指令范围内时才读取指令；diagnostic 为空时不写入输出。
 *
 * ## References
 * ZrParser_ExecIr_ElaborateCleanupDrops 在 function 参数为空指针、函数已 sealed 及候选语义检查失败时使用该辅助函数。
 */
