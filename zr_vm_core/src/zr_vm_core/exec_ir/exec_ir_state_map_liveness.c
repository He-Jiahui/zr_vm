#include "zr_vm_core/exec_ir_state_map_liveness.h"

#include <stdlib.h>
#include <string.h>

static void zr_live_add(TZrUInt8 *row, TZrExecIrValueId value) {
    TZrUInt32 bit = value - 1u;
    row[bit / 8u] |= (TZrUInt8)(1u << (bit % 8u));
}

static void zr_live_remove(TZrUInt8 *row, TZrExecIrValueId value) {
    TZrUInt32 bit = value - 1u;
    row[bit / 8u] &= (TZrUInt8)~(1u << (bit % 8u));
}

void ZrCore_ExecIr_StateMapLivenessFree(SZrStateMapLiveness *liveness) {
    free(liveness->before);
    free(liveness->after);
    free(liveness->semanticBefore);
    free(liveness->semanticAfter);
    memset(liveness, 0, sizeof(*liveness));
}

TZrBool ZrCore_ExecIr_StateMapLivenessContains(const SZrStateMapLiveness *liveness,
                                        TZrExecIrInstructionId instruction,
                                        TZrExecIrValueId value,
                                        TZrBool after) {
    const TZrUInt8 *rows = after ? liveness->after : liveness->before;
    TZrUInt32 bit = value - 1u;
    if (rows == ZR_NULL) {
        return ZR_FALSE;
    }
    return (TZrBool)((rows[(size_t)(instruction - 1u) * liveness->rowBytes +
                          bit / 8u] & (1u << (bit % 8u))) != 0u);
}

static void zr_live_add_deopt(const SZrExecIrFunction *function,
                              const SZrExecIrInstruction *instruction,
                              TZrUInt8 *row, TZrBool fieldsOnly) {
    TZrUInt32 stateIndex;
    if (instruction->deoptId == 0u) {
        return;
    }
    for (stateIndex = 0u; stateIndex < function->deoptStateCount; ++stateIndex) {
        const SZrExecIrDeoptState *state = &function->deoptStates[stateIndex];
        TZrUInt32 index;
        if (state->id != instruction->deoptId) {
            continue;
        }
        for (index = state->valueRange.start;
             !fieldsOnly && index < state->valueRange.start + state->valueRange.count;
             ++index) {
            zr_live_add(row, function->deoptValues[index]);
        }
        /* A scalarized field is a recovery use even when optimized code no
         * longer reads it. Object references use stable IDs, not SSA values;
         * scan definitions once so aliases and cycles need no recursion. */
        for (index = state->aggregates.start;
             index < state->aggregates.start + state->aggregates.count;
             ++index) {
            const SZrExecIrDeoptAggregate *aggregate = &function->deoptAggregates[index];
            TZrUInt32 field;
            for (field = aggregate->fields.start;
                 field < aggregate->fields.start + aggregate->fields.count; ++field) {
                const SZrExecIrDeoptAggregateField *binding =
                        &function->deoptAggregateFields[field];
                if (binding->kind == ZR_EXEC_IR_DEOPT_FIELD_VALUE) {
                    zr_live_add(row, binding->valueId);
                }
            }
        }
    }
}

/* PHI operands belong to predecessor edges. The PHI result is killed at the
 * successor's entry, so neither it nor another edge's input leaks backwards. */
static void zr_live_add_successors(const SZrExecIrFunction *function,
                                   const SZrExecIrBlock *block,
                                   const TZrUInt8 *liveIn,
                                   size_t rowBytes,
                                   TZrUInt8 *row) {
    TZrUInt32 edge;
    for (edge = block->successorRange.start;
         edge < block->successorRange.start + block->successorRange.count;
         ++edge) {
        const SZrExecIrBlock *successor =
                &function->blocks[function->successors[edge] - 1u];
        const TZrUInt8 *input = liveIn + (size_t)(successor->id - 1u) * rowBytes;
        size_t byte;
        TZrUInt32 phiIndex;
        for (byte = 0u; byte < rowBytes; ++byte) {
            row[byte] |= input[byte];
        }
        for (phiIndex = successor->phis.start;
             phiIndex < successor->phis.start + successor->phis.count;
             ++phiIndex) {
            const SZrExecIrPhi *phi = &function->phiPool[phiIndex];
            TZrUInt32 incoming;
            for (incoming = phi->incomings.start;
                 incoming < phi->incomings.start + phi->incomings.count;
                 ++incoming) {
                if (function->phiIncoming[incoming].predecessor == block->id) {
                    zr_live_add(row, function->phiIncoming[incoming].value);
                }
            }
        }
    }
}

static void zr_live_scan_instructions(const SZrExecIrFunction *function,
                                      SZrExecIrRange instructions,
                                      SZrStateMapLiveness *liveness,
                                      TZrUInt8 *row, TZrBool semanticOnly) {
    TZrUInt32 end = instructions.start + instructions.count;
    while (end > instructions.start) {
        TZrUInt32 index = --end;
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        TZrUInt32 valueIndex;
        size_t offset = (size_t)index * liveness->rowBytes;
        /* Recovery may use the values at either phase of this checkpoint,
         * even when the optimized instruction itself has no ordinary uses. */
        zr_live_add_deopt(function, instruction, row, ZR_FALSE);
        memcpy(liveness->after + offset, row, liveness->rowBytes);
        for (valueIndex = instruction->resultRange.start;
             valueIndex < instruction->resultRange.start + instruction->resultRange.count;
             ++valueIndex) {
            zr_live_remove(row, function->results[valueIndex]);
        }
        /* One recipe is shared by all emitted phases. Preserve its demanded
         * fields before the effect too, so ownership analysis rejects a field
         * that only becomes available as this instruction's result. */
        zr_live_add_deopt(function, instruction, row, ZR_TRUE);
        for (valueIndex = instruction->operandRange.start;
             (!semanticOnly || instruction->opcode != ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED) &&
             valueIndex < instruction->operandRange.start + instruction->operandRange.count;
             ++valueIndex) {
            zr_live_add(row, function->operands[valueIndex]);
        }
        memcpy(liveness->before + offset, row, liveness->rowBytes);
    }
}

static EZrExecutionDiagnosticCode zr_live_build(
        const SZrExecIrFunction *function, SZrStateMapLiveness *liveness,
        TZrBool semanticOnly) {
    TZrUInt8 *liveIn;
    TZrUInt8 *row;
    TZrUInt32 blockCount = function->blockCount != 0u ? function->blockCount : 1u;
    TZrBool changed;
    memset(liveness, 0, sizeof(*liveness));
    if (function->valueCount == 0u || function->instructionCount == 0u) {
        return ZR_EXECUTION_DIAGNOSTIC_NONE;
    }
    liveness->rowBytes = (size_t)(function->valueCount / 8u) +
                         (function->valueCount % 8u != 0u ? 1u : 0u);
    if ((size_t)function->instructionCount > SIZE_MAX / liveness->rowBytes ||
        (size_t)blockCount > SIZE_MAX / liveness->rowBytes) {
        return ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
    }
    liveness->before = (TZrUInt8 *)calloc(function->instructionCount, liveness->rowBytes);
    liveness->after = (TZrUInt8 *)calloc(function->instructionCount, liveness->rowBytes);
    liveIn = (TZrUInt8 *)calloc(blockCount, liveness->rowBytes);
    row = (TZrUInt8 *)calloc(1u, liveness->rowBytes);
    if (liveness->before == ZR_NULL || liveness->after == ZR_NULL ||
        liveIn == ZR_NULL || row == ZR_NULL) {
        free(liveIn);
        free(row);
        ZrCore_ExecIr_StateMapLivenessFree(liveness);
        return ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
    }
    do {
        TZrUInt32 blockIndex = blockCount;
        changed = ZR_FALSE;
        while (blockIndex != 0u) {
            const SZrExecIrBlock *block;
            SZrExecIrRange instructions = {0};
            TZrUInt8 *input;
            --blockIndex;
            block = function->blockCount != 0u ? &function->blocks[blockIndex] : ZR_NULL;
            memset(row, 0, liveness->rowBytes);
            if (block != ZR_NULL) {
                instructions = block->instructionRange;
                zr_live_add_successors(function, block, liveIn, liveness->rowBytes, row);
            } else {
                /* The verified blockless form is one straight-line region. */
                instructions.count = function->instructionCount;
            }
            zr_live_scan_instructions(function, instructions, liveness, row, semanticOnly);
            if (block != ZR_NULL) {
                TZrUInt32 phiIndex;
                for (phiIndex = block->phis.start;
                     phiIndex < block->phis.start + block->phis.count; ++phiIndex) {
                    zr_live_remove(row, function->phiPool[phiIndex].result);
                }
            }
            input = liveIn + (size_t)blockIndex * liveness->rowBytes;
            if (memcmp(input, row, liveness->rowBytes) != 0) {
                memcpy(input, row, liveness->rowBytes);
                changed = ZR_TRUE;
            }
        }
    } while (changed);
    free(liveIn);
    free(row);
    return ZR_EXECUTION_DIAGNOSTIC_NONE;
}

EZrExecutionDiagnosticCode ZrCore_ExecIr_StateMapLivenessBuild(
        const SZrExecIrFunction *function, SZrStateMapLiveness *liveness) {
    SZrStateMapLiveness semantic = {0};
    TZrUInt32 index, at;
    EZrExecutionDiagnosticCode code;
    memset(liveness, 0, sizeof(*liveness));
    /* OwnerAnalysis validates CFG and SSA pools before this helper is used.
     * Recovery-only uses have their own pools and need independent checks. */
    if (function->deoptStateCount > function->deoptStateCapacity ||
        function->deoptValueCount > function->deoptValueCapacity ||
        (function->deoptStateCount != 0u && function->deoptStates == ZR_NULL) ||
        (function->deoptValueCount != 0u && function->deoptValues == ZR_NULL))
        return ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
    for (index = 0u; index < function->deoptStateCount; ++index) {
        SZrExecIrRange range = function->deoptStates[index].valueRange;
        if (range.start > function->deoptValueCount ||
            range.count > function->deoptValueCount - range.start)
            return ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
        for (at = range.start; at < range.start + range.count; ++at)
            if (function->deoptValues[at] == 0u || function->deoptValues[at] > function->valueCount)
                return ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
    }
    code = zr_live_build(function, liveness, ZR_FALSE);
    if (code != ZR_EXECUTION_DIAGNOSTIC_NONE) return code;
    code = zr_live_build(function, &semantic, ZR_TRUE);
    if (code != ZR_EXECUTION_DIAGNOSTIC_NONE) {
        ZrCore_ExecIr_StateMapLivenessFree(liveness);
        return code;
    }
    liveness->semanticBefore = semantic.before;
    liveness->semanticAfter = semantic.after;
    return ZR_EXECUTION_DIAGNOSTIC_NONE;
}

static TZrBool zr_live_semantic(const SZrStateMapLiveness *liveness,
                                TZrExecIrInstructionId instruction,
                                TZrExecIrValueId value, EZrExecIrStateMapPhase phase) {
    SZrStateMapLiveness semantic = *liveness;
    semantic.before = liveness->semanticBefore;
    semantic.after = liveness->semanticAfter;
    return ZrCore_ExecIr_StateMapLivenessContains(&semantic, instruction, value,
            (TZrBool)(phase != ZR_EXEC_IR_STATE_BEFORE_EFFECT));
}

TZrUInt8 ZrCore_ExecIr_StateMapOwnerMaskAt(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase) {
    TZrUInt8 mask = ZrCore_ExecIr_OwnerStateMaskAt(ownership, instruction, value, phase);
    const SZrExecIrInstruction *ins;
    const SZrExecIrOpcodeInfo *info;
    TZrUInt32 index;
    if (mask == 0u || phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT ||
        zr_live_semantic(liveness, instruction, value, phase)) return mask;
    ins = &function->instructions[instruction - 1u];
    info = ZrCore_ExecIr_OpcodeInfo(ins->opcode);
    /* An after-effect checkpoint precedes edge selection. Cleanup-only
     * results may be absent on the exceptional successor. Ordinary result
     * uses retain the established normal-edge availability contract. */
    if ((info->flags & (ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR | ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW)) ==
        (ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR | ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW)) {
        for (index = ins->resultRange.start;
             index < ins->resultRange.start + ins->resultRange.count; ++index) {
            if (function->results[index] == value)
                mask |= ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED);
        }
    }
    return mask;
}

EZrExecIrStateMapOwnerState ZrCore_ExecIr_StateMapOwnerAt(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase) {
    TZrUInt8 mask = ZrCore_ExecIr_StateMapOwnerMaskAt(
            function, ownership, liveness, instruction, value, phase);
    TZrUInt32 state;
    EZrExecIrOwnership kind;
    if (mask == ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED))
        return ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
    if (mask == ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN))
        return ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN;
    if (mask == 0u || zr_live_semantic(liveness, instruction, value, phase))
        return ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT;
    kind = function->values[value - 1u].ownership;
    if ((kind != ZR_EXEC_IR_OWNERSHIP_UNIQUE && kind != ZR_EXEC_IR_OWNERSHIP_SHARED) ||
        (mask & ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN)) != 0u)
        return ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT;
    for (state = ZR_EXEC_IR_STATE_MAP_OWNER_MOVED;
         state < ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL; ++state) {
        if (mask == ZR_EXEC_IR_OWNER_STATE_BIT(state)) return (EZrExecIrStateMapOwnerState)state;
    }
    return ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL;
}

/**
 * @fn static void zr_live_add(TZrUInt8 *row, TZrExecIrValueId value)
 *
 * ## Summary
 * 把一个一基 ValueId 加入当前位图行，供 operand、deopt 恢复用途和 PHI 边用途汇合使用。
 *
 * ## Parameters
 * @param[in,out] row 当前位图行，至少覆盖相应 value 的位；原有其他位须保留。
 * @param[in] value 所属已验证函数中的有效一基 ValueId。
 *
 * ## Returns
 * 无 C 返回值；只将对应位设为 1，不改其他位。
 *
 * ## Constraints and lifecycle
 * 不检查 NULL、ValueId 或行长度。调用方保证 value 非零并落在已分配行的 valueCount 范围内，row 可写且在本次操作期间有效。
 * bit=value-1 后以字节和位下标访问；此处不分配、释放或延长输入的生命周期，也不提供并发写同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:6
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:8
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:53
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:68
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:103
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:137
 * - tests/parser/test_ssa_state_map_liveness.c:78
 * - tests/parser/test_ssa_state_map_liveness.c:79
 * - tests/parser/test_ssa_state_map_liveness.c:80
 */

/**
 * @fn static void zr_live_remove(TZrUInt8 *row, TZrExecIrValueId value)
 *
 * ## Summary
 * 从反向数据流行中杀掉在当前指令或 PHI 入口定义的结果，避免把该定义无条件传播到定义之前。
 *
 * ## Parameters
 * @param[in,out] row 当前位图行，至少覆盖相应 value 的位；原有其他位须保留。
 * @param[in] value 所属已验证函数中的有效一基 ValueId。
 *
 * ## Returns
 * 无 C 返回值；只清除对应位，不改其他位。
 *
 * ## Constraints and lifecycle
 * 调用方保证 value 为匹配函数中的有效一基 ID，row 非空、可写且覆盖该位；函数本身不验证 ID 或行容量。
 * 行所有权仍属于上层分析。普通指令结果由反向扫描调用，PHI 结果在块入口统一移除；无分配、释放或同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:11
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:13
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:193
 * - tests/parser/test_ssa_state_map_liveness.c:78
 * - tests/parser/test_ssa_state_map_liveness.c:79
 * - tests/parser/test_ssa_state_map_liveness.c:80
 * - tests/parser/test_ssa_state_map_liveness.c:94
 */

/**
 * @fn void ZrCore_ExecIr_StateMapLivenessFree(SZrStateMapLiveness *liveness)
 *
 * ## Summary
 * 释放一份存活分析所持有的普通 before/after 与 semantic before/after 四个池，并将记录清零。
 *
 * ## Parameters
 * @param[in,out] liveness 非空的唯一 owning 记录；四个池为空或各自兼容 C free，调用后记录清零。
 *
 * ## Returns
 * 无 C 返回值；完成后四指针和 rowBytes 均为零。记录对象自身及其地址继续有效，旧池与池元素的借用失效。
 *
 * ## Constraints and lifecycle
 * liveness 必须非空。各非空池须可分别 free，不得指向函数输入池、栈存储或同一分配的重叠部分；不得对同一 owning 记录的浅复制分别 Free。
 * 全零记录与先前已 Free 的记录可再次传入；尺寸溢出后只有 rowBytes 非零、四池为空的记录也可清零释放。此函数不释放 function 或 ownership。
 * 调用方须独占修改/释放该记录和池，结束所有旧池借用后再调用；没有同步或借用跟踪。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:16
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:17
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:21
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:168
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:234
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:479
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:657
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:661
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:144
 * - tests/parser/test_ssa_licm_scalar_context.inc:134
 */

/**
 * @fn TZrBool ZrCore_ExecIr_StateMapLivenessContains(const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction, TZrExecIrValueId value, TZrBool after)
 *
 * ## Summary
 * 查询普通存活位图中某一指令 phase 是否保留指定 ValueId；这是原始位查询，不负责校验函数身份或 ID。
 *
 * ## Parameters
 * @param[in] liveness 非空且稳定的位图记录；所选池若非空，行宽和有效 ID 范围须与原函数匹配。
 * @param[in] instruction 位图所对应函数的一基指令 ID；非空池分支不检查范围。
 * @param[in] value 位图所对应函数的一基 ValueId；非空池分支不检查范围。
 * @param[in] after 零选择 before，非零选择 after；不选择 semantic 池。
 *
 * ## Returns
 * @return 所选 before/after 池为空时返回 ZR_FALSE；非空时返回对应位是否为 1。ZR_FALSE 不区分空池与合法查询得到的未存活。
 *
 * ## Constraints and lifecycle
 * liveness 自身不能为 NULL。所选池非空时，调用方保证 instruction 与 value 均为匹配函数的有效一基 ID，rowBytes 正确且整个偏移可读；记录没有 instructionCount、valueCount 或 generation 可供校验。
 * 查询只借用记录和所选行，不转移所有权，也不延长池或函数的生命周期。借用到 Free、重建或外部修改之前有效；共享只读须排除并发写与释放。
 * NULL 行的早退不构成任意 ID、任意指针或 NULL 记录的通用安全承诺。semantic 查询由内部临时视图另行选择池。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:24
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:28
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:30
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:33
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:248
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:252
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:52
 * - tests/parser/test_ssa_licm_scalar_context.inc:126
 */

/**
 * @fn static void zr_live_add_deopt(const SZrExecIrFunction *function, const SZrExecIrInstruction *instruction, TZrUInt8 *row, TZrBool fieldsOnly)
 *
 * ## Summary
 * 把当前 checkpoint 的恢复用途合并到存活行。对所有匹配 deoptId 的 state 扫描普通值和 aggregate 定义；aggregate 间引用使用独立身份，不当作 SSA ValueId。
 *
 * ## Parameters
 * @param[in] function 已验证且稳定的函数；deopt 与 aggregate 的池、范围、字段种类和 VALUE ID 均有效。
 * @param[in] instruction 属于该 function 的稳定指令；deoptId 为零时无需恢复用途。
 * @param[in,out] row 覆盖所有 ValueId 的可写存活行，按位合并恢复用途。
 * @param[in] fieldsOnly 非零只加入 aggregate 的 VALUE 字段；为零还加入匹配 state 的普通 deoptValues。
 *
 * ## Returns
 * 无 C 返回值；deoptId 为零时不改 row。否则加入所选恢复值，并保留已有位。
 *
 * ## Constraints and lifecycle
 * 只扫描字段定义一次，不对 aggregate 引用做 DFS；alias 或 recipe 对象图的环不会触发递归。只有 ZR_EXEC_IR_DEOPT_FIELD_VALUE 字段将 valueId 加入行。
 * 普通 deoptValues 和 aggregate 字段使用不同池。公开 Build 只补验普通 deopt state/value 的基础尺寸与 ID，aggregate 池、范围和 VALUE 字段 ID 仍依赖上游完整验证。
 * fieldsOnly 不表示忽略整个恢复场景：反向扫描先以零调用保存 after，再移除结果并以非零重加字段，保证同一配方的字段在 before 也有需求。
 * 函数和指令仅借用，row 由上层持有；不分配、释放或校验任意地址。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:37
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:41
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:44
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:50
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:58
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:67
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:122
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:132
 * - tests/parser/test_ssa_deopt_aggregates.c:113
 * - tests/parser/test_ssa_deopt_aggregates.c:119
 * - tests/parser/test_ssa_deopt_aggregates.c:121
 * - tests/parser/test_ssa_deopt_aggregates.c:255
 * - tests/parser/test_ssa_deopt_aggregates.c:258
 * - tests/parser/test_ssa_deopt_aggregates.c:520
 * - tests/parser/test_ssa_deopt_aggregates.c:536
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:573
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:403
 */

/**
 * @fn static void zr_live_add_successors(const SZrExecIrFunction *function, const SZrExecIrBlock *block, const TZrUInt8 *liveIn, size_t rowBytes, TZrUInt8 *row)
 *
 * ## Summary
 * 合并当前块所有后继的 live-in，并仅为前驱 ID 等于当前 block.id 的 PHI incoming 加入边用途；后继 PHI 的结果不在此处作为前驱定义。
 *
 * ## Parameters
 * @param[in] function CFG、successors、PHI 与 incoming 范围及 ID 已验证且稳定的函数。
 * @param[in] block 属于 function 的当前块，其 id 与 successorRange 有效。
 * @param[in] liveIn 所有块当前迭代的输入位图，至少含 blockCount 个 rowBytes 行；本次只读。
 * @param[in] rowBytes 与 liveIn 和 row 一致的每行字节数。
 * @param[in,out] row 当前块的可写输出行，用 OR 合并后继输入及当前前驱边的 PHI incoming。
 *
 * ## Returns
 * 无 C 返回值；row 得到原有位、后继输入与当前边 PHI 用途的并集。
 *
 * ## Constraints and lifecycle
 * 不按 NORMAL、EXCEPTION 或 CLEANUP 边种类筛选，所有列出的后继均参加合并；PHI incoming 根据 predecessor 块 ID 选择。
 * 调用方保证块/后继 ID、每个范围和输入池有效，liveIn 足长可读，row 足长可写并与待读 liveIn 和函数输入不重叠。函数不自行验证或创建 CFG。
 * PHI 结果在 zr_live_build 的块入口移除；此函数只借用已有池和迭代缓冲，不分配、释放或同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:77
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:83
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:91
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:94
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:102
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:183
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:193
 * - tests/parser/test_ssa_state_map_liveness.c:73
 * - tests/parser/test_ssa_state_map_liveness.c:75
 * - tests/parser/test_ssa_state_map_liveness.c:78
 * - tests/parser/test_ssa_state_map_liveness.c:79
 * - tests/parser/test_ssa_state_map_liveness.c:80
 * - tests/parser/test_ssa_state_map_liveness.c:122
 * - tests/parser/test_ssa_state_map_liveness.c:124
 */

/**
 * @fn static void zr_live_scan_instructions(const SZrExecIrFunction *function, SZrExecIrRange instructions, SZrStateMapLiveness *liveness, TZrUInt8 *row, TZrBool semanticOnly)
 *
 * ## Summary
 * 在一个块内反向计算每条指令的 before/after 存活行，并按需要忽略 guarded cleanup 的 operand 语义用途。
 *
 * ## Parameters
 * @param[in] function 已验证且稳定的函数；指令、operand、result 和恢复配方池可读。
 * @param[in] instructions 一个有效、无端点溢出的指令池区间，按反序扫描。
 * @param[in,out] liveness 本轮拥有的 before/after 输出池及正确 rowBytes；两池可写且与输入和 scratch 分离。
 * @param[in,out] row 进入扫描时为块 live-out，退出时为杀掉普通结果后的块输入行；由上层复用。
 * @param[in] semanticOnly 非零时仅省略 DROP_IF_INITIALIZED 的 operand 用途，其余 operand 与恢复用途仍参加。
 *
 * ## Returns
 * 无 C 返回值；写出本区间各指令的 before/after 行并更新 row。PHI 结果仍由上层在块入口另行移除。
 *
 * ## Constraints and lifecycle
 * 顺序为加入普通和字段恢复用途、保存 after、移除结果、重加字段恢复用途、加入允许的 operand、保存 before。普通 deoptValues 不会在杀掉结果后单独重加，aggregate 的 VALUE 字段会重加。
 * semanticOnly 仅在 opcode 等于 DROP_IF_INITIALIZED 时跳过整个 operandRange；普通 DROP 及其他 opcode 的 operand 不被省略，两种分析都保留 deopt 恢复用途。
 * 不验证区间端点、SSA ID、opcode 或配方，也不分配临时池；调用方保证所有已读存储稳定、输出与 row 不重叠。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:110
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:122
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:123
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:127
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:132
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:134
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:139
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:188
 * - tests/parser/test_ssa_state_map_liveness.c:146
 * - tests/parser/test_ssa_state_map_liveness.c:147
 * - tests/parser/test_ssa_state_map_liveness.c:148
 * - tests/parser/test_ssa_state_map_liveness.c:149
 * - tests/parser/test_ssa_deopt_aggregates.c:533
 * - tests/parser/test_ssa_deopt_aggregates.c:534
 * - tests/parser/test_ssa_deopt_aggregates.c:536
 * - tests/parser/test_ssa_conditional_cleanup.c:339
 * - tests/parser/test_ssa_conditional_cleanup.c:344
 */

/**
 * @fn static EZrExecutionDiagnosticCode zr_live_build(const SZrExecIrFunction *function, SZrStateMapLiveness *liveness, TZrBool semanticOnly)
 *
 * ## Summary
 * 建立一套 before/after 位图，以 CFG 后继和 PHI 边用途迭代到固定点；无块的已验证直线形式作为一个区域处理。
 *
 * ## Parameters
 * @param[in] function 非空、调用期间稳定的已验证函数；CFG、SSA 与 aggregate recipe 的池、范围、ID 和 opcode 已由上层验证。
 * @param[out] liveness 非空且为空或已先 Free 的独立输出记录；入口清零，不保留旧分析。
 * @param[in] semanticOnly 为零计算完整存活，非零计算省略 guarded-drop operand 的语义存活。
 *
 * ## Returns
 * @return 成功返回 NONE，保留 before/after；值或指令数为零时成功且记录全零。尺寸检查失败返回 CAPACITY_OVERFLOW，rowBytes 已计算而四池为空；任意本轮 calloc 失败返回 OUT_OF_MEMORY，释放所有本轮池并清零输出。
 *
 * ## Constraints and lifecycle
 * 入口清零输出，不释放其中旧指针；调用前必须为空或先 Free。function 与输出记录、输出池和 scratch 必须分离，输入在分析期间不得修改或释放。
 * rowBytes 用 valueCount/8 加余数计算，避免先加 7；随后分别检查 instructionCount 与有效 blockCount 的乘积。成功非空一轮分配 before、after、liveIn、row 四池，正常结束只保留前两池，两个 scratch 统一释放。
 * 按物理块倒序扫描所有块，无单独可达性筛选，反复更新 liveIn 直到无变化；不承诺一次线性扫描、迭代次数上限或其他优化复杂度。
 * 此处依赖已验证的 CFG/SSA/aggregate recipe；不是输入校验器。内部循环和错误清理属于此函数，不另拆重复单元。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:143
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:148
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:150
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:151
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:154
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:156
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:160
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:164
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:168
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:183
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:193
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:197
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:202
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:203
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:230
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:232
 * - tests/parser/test_ssa_state_map_liveness.c:19
 * - tests/parser/test_ssa_state_map_liveness.c:168
 * - tests/parser/test_ssa_state_map_liveness.c:171
 * - tests/parser/test_ssa_state_map_liveness.c:176
 */

/**
 * @fn EZrExecutionDiagnosticCode ZrCore_ExecIr_StateMapLivenessBuild(const SZrExecIrFunction *function, SZrStateMapLiveness *liveness)
 *
 * ## Summary
 * 在同一已验证函数上建立完整存活和语义存活两套位图，供 checkpoint 活值/根集合及 cleanup-only owner 判定共用。
 *
 * ## Parameters
 * @param[in] function 非空、调用期间稳定的已验证函数；CFG、SSA 与 aggregate recipe 的池、范围、ID 和 opcode 已由上层验证。
 * @param[out] liveness 非空且为空或已先 Free 的独立输出记录；入口清零，不保留旧分析。
 *
 * ## Returns
 * @return 成功返回 NONE：非空分析持有四个位图池，空值或空指令函数输出全零。普通 deopt 池、区间或 ID 无效时返回 STATE_MAP_INVALID 且输出全零。首套尺寸溢出返回 CAPACITY_OVERFLOW，可能仅 rowBytes 非零而四池仍 NULL；首套 OOM 或第二套构建失败会释放已分配输出并归零。
 *
 * ## Constraints and lifecycle
 * function 与 liveness 都必须非空，输出记录必须为空或事先 Free。入口先 memset，不能用来事务替换旧分析；输出记录不得与 function 或其被读池重叠。
 * 上游须先验证 CFG、SSA、opcode 和 aggregate recipe。此函数只额外检查 deoptStates/deoptValues 的 count/capacity、非零 count 的 NULL 池、各普通 valueRange 和 ValueId，不独立验证 aggregate 池或全部函数输入。Parser 当前先 Verify 再本函数，随后才建立 owner 分析；不能把现有所有路径描述为先 OwnerAnalysisBuild。
 * 两套非空分析共执行八个 calloc 表达式；每套两个 scratch 在该套结束前释放，成功仅四个结果池归输出持有。第二套的 before/after 所有权移交至 semanticBefore/semanticAfter，不随局部记录退出释放。
 * semantic 分析只省略 DROP_IF_INITIALIZED 的 operand 用途；DROP 和 deopt 恢复用途仍保留。不读取运行时 payload，也不会让逻辑 ID 变成实际 GC root。
 * 输出不保留 function 指针或 epoch。调用方保留对应函数布局/ID 上下文，IR 改变后重建，并在独占 Build/Free 时结束旧池借用；函数不提供同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:208
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:213
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:216
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:223
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:227
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:230
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:232
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:234
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:237
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:238
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:375
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:376
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:379
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:384
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:479
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:573
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:611
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:614
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:657
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:661
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:100
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:101
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:144
 * - tests/parser/test_ssa_licm_scalar_context.inc:123
 * - tests/parser/test_ssa_licm_scalar_context.inc:134
 * - tests/parser/test_ssa_state_map_liveness.c:19
 * - tests/parser/test_ssa_state_map_liveness.c:176
 */

/**
 * @fn static TZrBool zr_live_semantic(const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction, TZrExecIrValueId value, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 用临时浅视图把 semantic 池映射为 Contains 的 before/after，复用位查询而不转移或复制池所有权。
 *
 * ## Parameters
 * @param[in] liveness 非空且稳定的分析；semantic 池的维度与调用方的函数 ID 相匹配。
 * @param[in] instruction 有效一基指令 ID，或在上层已提前拒绝后不进入此函数。
 * @param[in] value 有效一基 ValueId。
 * @param[in] phase BEFORE 选择 semanticBefore，其余传入值选择 semanticAfter；本 helper 不验证 phase。
 *
 * ## Returns
 * @return 返回所选 semantic 行的位值；所选 semantic 池为空时为 FALSE。
 *
 * ## Constraints and lifecycle
 * 临时结构只借用原池，不能对该浅复制调用 Free。返回布尔值不延长任何记录或池的生命周期。
 * 非空行仍依赖有效 ID、正确 rowBytes 和稳定存储。合法 AFTER 与 CLEANUP 均使用 after；未知 phase 在此 helper 中也会选 after，公开 owner 查询先靠 OwnerStateMaskAt 的校验与 mask=0 短路拒绝。
 * 本函数只读，没有分配、释放或同步；语义位图区别只来自构建阶段对 guarded-drop operand 的省略。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:242
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:245
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:246
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:248
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:249
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:256
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:260
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:261
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:290
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:427
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:429
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:433
 */

/**
 * @fn TZrUInt8 ZrCore_ExecIr_StateMapOwnerMaskAt(const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership, const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction, TZrExecIrValueId value, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 查询 owner may-state mask，并为 effect 之后、尚未选边且只有 cleanup 用途的可抛出终结指令结果补入 UNINITIALIZED 的可能性。
 *
 * ## Parameters
 * @param[in] function 与 ownership、liveness 同一次 IR 状态匹配的已验证函数及稳定输入池。
 * @param[in] ownership 与 function 相匹配、仍有效的 owner may-state 分析。
 * @param[in] liveness 与同一 function 的 ID、行宽和指令布局匹配的存活位图记录。
 * @param[in] instruction 该函数的一基指令 ID。
 * @param[in] value 该函数的一基 ValueId。
 * @param[in] phase 同一 checkpoint 的合法 phase；BEFORE 读 before，其余合法 phase 使用 after 语义行。
 *
 * ## Returns
 * @return 返回 owner 状态位集合，非具体 owner 枚举。底层 owner 查询拒绝参数、phase、不可达指令或无行时返回 0；BEFORE 或语义存活时保留原 mask；满足后效应结果规则时在原 mask 中 OR UNINITIALIZED。
 *
 * ## Constraints and lifecycle
 * 正常查询要求 function、ownership、liveness 来自同一未变更 IR，相关池可读且 opcode 有效。底层 mask=0 或 BEFORE 的短路不会读后面的 function/liveness，不应由此推断一般 NULL 参数支持。
 * 补位依据 OpcodeInfo 的 schema 同时含 TERMINATOR 和 MAY_THROW，且 value 位于该指令 resultRange；不是仅凭 instruction.flags 或 MAY_THROW 名称。合法 AFTER 与 CLEANUP 均可走此规则。
 * 只在非 BEFORE 且 semantic 位为零时补位，保留普通结果用途的正常边可用性；它不选择具体 successor，不执行 DROP，不读取或创建运行时 owner payload。
 * 所有参数只借用，不分配、释放或同步；返回 mask 可复制，但解释它仍须使用对应函数与 phase 上下文。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:252
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:256
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:260
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:261
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:263
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:267
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:269
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:272
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:423
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:427
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:429
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:433
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:276
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:67
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:111
 * - tests/parser/test_ssa_conditional_cleanup.c:415
 * - tests/parser/test_ssa_conditional_cleanup.c:426
 * - tests/parser/test_ssa_conditional_cleanup.c:435
 * - tests/parser/test_ssa_conditional_cleanup.c:437
 * - tests/parser/test_ssa_conditional_cleanup.c:449
 * - tests/parser/test_ssa_conditional_cleanup.c:451
 */

/**
 * @fn EZrExecIrStateMapOwnerState ZrCore_ExecIr_StateMapOwnerAt(const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership, const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction, TZrExecIrValueId value, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 把 checkpoint 的 owner mask 与语义用途合并为可记录的 owner state，允许 UNIQUE/SHARED 的 cleanup-only 非活动或条件状态，并拒绝不可用的语义读取。
 *
 * ## Parameters
 * @param[in] function 与 ownership、liveness 同一次 IR 状态匹配的已验证函数及稳定输入池。
 * @param[in] ownership 与 function 相匹配、仍有效的 owner may-state 分析。
 * @param[in] liveness 与同一 function 的 ID、行宽和指令布局匹配的存活位图记录。
 * @param[in] instruction 该函数的一基指令 ID。
 * @param[in] value 该函数的一基 ValueId。
 * @param[in] phase 同一 checkpoint 的合法 phase；BEFORE 读 before，其余合法 phase 使用 after 语义行。
 *
 * ## Returns
 * @return 精确 INITIALIZED、UNKNOWN 单位 mask 分别直接返回同名状态。其余 mask 为零或 value 仍语义存活时返回 OWNER_STATE_COUNT。语义不存活且为 UNIQUE/SHARED、mask 无 UNKNOWN 时，精确 MOVED、DROPPED、UNINITIALIZED 单位 mask 均返回同名状态；其余符合前提的非零多状态 mask 返回 CONDITIONAL。
 *
 * ## Constraints and lifecycle
 * COUNT 是不可接受状态的哨兵，不是有效 witness。INITIALIZED 和 UNKNOWN 的单值分支先于后面的 ownership/semantic 限制；不能声称所有返回状态都仅限 UNIQUE/SHARED。
 * 公开枚举中 MOVED=2、DROPPED=3、UNINITIALIZED=4、CONDITIONAL=5，故 MOVED 到 CONDITIONAL 之前的循环包含 UNINITIALIZED；单 UNINITIALIZED 在语义已死且 ownership 为 UNIQUE/SHARED 等前提满足时返回 UNINITIALIZED。语义仍活时，前面的检查先返回 COUNT。
 * 调用方保证函数、两份分析和有效 ID/phase 匹配且稳定。底层返回零的短路不证明一般损坏记录或越界 ID 可安全查询。
 * 不执行移动、释放或 DROP；不验证 runtime witness payload，不分配或延长函数/分析生命周期，无内部同步。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:278
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:282
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:286
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:288
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:290
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:292
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:293
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:296
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:298
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:300
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:256
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:258
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:65
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:72
 * - tests/parser/test_ssa_licm_scalar_context.inc:129
 * - tests/parser/test_ssa_state_map_ownership.c:96
 * - tests/parser/test_ssa_state_map_ownership.c:98
 * - tests/parser/test_ssa_state_map_ownership.c:100
 * - tests/parser/test_ssa_state_map_ownership.c:104
 * - tests/parser/test_ssa_conditional_cleanup.c:339
 * - tests/parser/test_ssa_conditional_cleanup.c:344
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:31
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:34
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:36
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:38
 */
