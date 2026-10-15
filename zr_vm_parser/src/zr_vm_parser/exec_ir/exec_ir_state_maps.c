#include "zr_vm_parser/exec_ir_state_maps.h"

#include "zr_vm_core/exec_ir_state_map_liveness.h"
#include "zr_vm_core/exec_ir_owner_state.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef enum EZrStateMapBuildResult {
    ZR_STATE_MAP_BUILD_OK = 0,
    ZR_STATE_MAP_BUILD_OUT_OF_MEMORY,
    ZR_STATE_MAP_BUILD_BORROWED,
    ZR_STATE_MAP_BUILD_INVALID
} EZrStateMapBuildResult;

static TZrBool zr_state_map_allocation_size_valid(TZrUInt32 count,
                                                  size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
                     (TZrUInt64)count <= (TZrUInt64)(SIZE_MAX / elementSize));
}

static TZrBool zr_state_map_phase_valid(EZrExecIrStateMapPhase phase) {
    switch (phase) {
        case ZR_EXEC_IR_STATE_BEFORE_EFFECT:
        case ZR_EXEC_IR_STATE_AFTER_EFFECT:
        case ZR_EXEC_IR_STATE_CLEANUP_COMPLETE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static void zr_state_map_clear_diagnostic(SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static void zr_state_map_set_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                        EZrExecutionDiagnosticCode code,
                                        const SZrExecIrFunction *function,
                                        TZrUInt32 instructionId,
                                        TZrUInt32 sourceId) {
    TZrUInt32 blockIndex;

    if (diagnostic == ZR_NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->instructionId = instructionId;
    diagnostic->sourceId = sourceId;
    diagnostic->blockId = ZR_EXEC_IR_BLOCK_ID_INVALID;
    if (function == ZR_NULL || instructionId == 0u || function->blocks == ZR_NULL) {
        return;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 instructionIndex = instructionId - 1u;
        if (instructionIndex >= block->instructionRange.start &&
            instructionIndex - block->instructionRange.start < block->instructionRange.count) {
            diagnostic->blockId = block->id;
            return;
        }
    }
}

static TZrBool zr_state_map_reserve(void **storage,
                                    TZrUInt32 *capacity,
                                    TZrUInt32 count,
                                    TZrUInt32 needed,
                                    size_t elementSize) {
    TZrUInt32 next;
    void *memory;

    if (storage == ZR_NULL || capacity == ZR_NULL || elementSize == 0u ||
        needed < count || (size_t)needed > SIZE_MAX / elementSize) {
        return ZR_FALSE;
    }
    if (needed <= *capacity) {
        return ZR_TRUE;
    }
    next = *capacity == 0u ? 4u : *capacity;
    while (next < needed) {
        if (next > UINT32_MAX / 2u) {
            next = needed;
            break;
        }
        next *= 2u;
    }
    if ((size_t)next > SIZE_MAX / elementSize) {
        return ZR_FALSE;
    }
    memory = realloc(*storage, (size_t)next * elementSize);
    if (memory == ZR_NULL) {
        return ZR_FALSE;
    }
    *storage = memory;
    *capacity = next;
    return ZR_TRUE;
}

static TZrBool zr_state_map_append_values(SZrExecIrStateMap *map,
                                          const TZrExecIrValueId *values,
                                          TZrUInt32 count,
                                          SZrExecIrRange *range,
                                          TZrBool roots) {
    TZrExecIrValueId **storage = roots ? &map->rootPool : &map->valuePool;
    TZrUInt32 *used = roots ? &map->rootCount : &map->valueCount;
    TZrUInt32 *capacity = roots ? &map->rootCapacity : &map->valueCapacity;
    TZrUInt32 old;

    if (map == ZR_NULL || (count != 0u && values == ZR_NULL) ||
        count > UINT32_MAX - *used) {
        return ZR_FALSE;
    }
    old = *used;
    if (!zr_state_map_reserve((void **)storage, capacity, old, old + count,
                              sizeof(**storage))) {
        return ZR_FALSE;
    }
    if (count != 0u) {
        memcpy(*storage + old, values, (size_t)count * sizeof(**storage));
    }
    *used = old + count;
    if (range != ZR_NULL) {
        range->start = old;
        range->count = count;
    }
    return ZR_TRUE;
}

static TZrBool zr_state_map_append_owner_states(SZrExecIrStateMap *map,
                                                 const TZrUInt32 *states,
                                                 TZrUInt32 count,
                                                 SZrExecIrRange *range) {
    TZrUInt32 old;

    if (map == ZR_NULL || (count != 0u && states == ZR_NULL) ||
        count > UINT32_MAX - map->ownerStateCount) {
        return ZR_FALSE;
    }
    old = map->ownerStateCount;
    if (!zr_state_map_reserve((void **)&map->ownerStatePool,
                              &map->ownerStateCapacity, old, old + count,
                              sizeof(*map->ownerStatePool))) {
        return ZR_FALSE;
    }
    if (count != 0u) {
        memcpy(map->ownerStatePool + old, states,
               (size_t)count * sizeof(*map->ownerStatePool));
    }
    map->ownerStateCount = old + count;
    if (range != ZR_NULL) {
        range->start = old;
        range->count = count;
    }
    return ZR_TRUE;
}

static TZrBool zr_state_map_append_entry(SZrExecIrStateMap *map,
                                         const SZrExecIrStateMapEntry *entry) {
    if (map == ZR_NULL || entry == ZR_NULL || map->entryCount == UINT32_MAX ||
        !zr_state_map_reserve((void **)&map->entries, &map->entryCapacity,
                              map->entryCount, map->entryCount + 1u,
                              sizeof(*map->entries))) {
        return ZR_FALSE;
    }
    map->entries[map->entryCount++] = *entry;
    return ZR_TRUE;
}


static TZrExecIrBlockId zr_state_map_handler_block(const SZrExecIrFunction *function,
                                                   TZrUInt32 instructionId,
                                                   TZrUInt32 boundaryFlags) {
    TZrUInt32 blockIndex;

    if (function == ZR_NULL || instructionId == 0u ||
        instructionId > function->instructionCount ||
        (boundaryFlags & ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW) == 0u) {
        return ZR_EXEC_IR_BLOCK_ID_INVALID;
    }
    for (blockIndex = 0u; blockIndex < function->blockCount; ++blockIndex) {
        const SZrExecIrBlock *block = &function->blocks[blockIndex];
        TZrUInt32 instructionIndex = instructionId - 1u;
        TZrUInt32 successorIndex;
        if (instructionIndex < block->instructionRange.start ||
            instructionIndex - block->instructionRange.start >= block->instructionRange.count) {
            continue;
        }
        for (successorIndex = block->successorRange.start;
             successorIndex < block->successorRange.start + block->successorRange.count;
             ++successorIndex) {
            TZrExecIrBlockId successor = function->successors[successorIndex];
            const SZrExecIrBlock *successorBlock =
                ZrCore_ExecIr_FunctionBlockAtConst(function, successor);
            if (successorBlock != ZR_NULL &&
                (successorBlock->flags & (ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION |
                                          ZR_EXEC_IR_BLOCK_FLAG_CLEANUP)) != 0u) {
                return successor;
            }
        }
        return ZR_EXEC_IR_BLOCK_ID_INVALID;
    }
    return ZR_EXEC_IR_BLOCK_ID_INVALID;
}

static EZrStateMapBuildResult zr_state_map_add_checkpoint(
        SZrExecIrStateMap *map,
        const SZrExecIrFunction *function,
        const SZrStateMapLiveness *liveness,
        const SZrExecIrOwnerAnalysis *ownership,
        const SZrExecIrInstruction *instruction,
        TZrUInt32 instructionId,
        TZrUInt32 boundaryFlags,
        EZrExecIrStateMapPhase phase,
        TZrUInt32 resumeId,
        SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrValueId *live = ZR_NULL;
    TZrExecIrValueId *roots = ZR_NULL;
    TZrUInt32 *owners = ZR_NULL;
    TZrUInt32 liveCount = 0u;
    TZrUInt32 rootCount = 0u;
    TZrUInt32 ownerCount = 0u;
    TZrUInt32 valueIndex;
    TZrBool includeCurrent = (TZrBool)(phase != ZR_EXEC_IR_STATE_BEFORE_EFFECT);
    SZrExecIrStateMapEntry entry;

    if (map == ZR_NULL || function == ZR_NULL || instruction == ZR_NULL ||
        instructionId == 0u || resumeId == 0u || !zr_state_map_phase_valid(phase)) {
        return ZR_STATE_MAP_BUILD_INVALID;
    }
    if (function->valueCount != 0u) {
        if (!zr_state_map_allocation_size_valid(function->valueCount, sizeof(*live)) ||
            !zr_state_map_allocation_size_valid(function->valueCount, sizeof(*owners))) {
            return ZR_STATE_MAP_BUILD_INVALID;
        }
        live = (TZrExecIrValueId *)malloc((size_t)function->valueCount * sizeof(*live));
        roots = (TZrExecIrValueId *)malloc((size_t)function->valueCount * sizeof(*roots));
        owners = (TZrUInt32 *)malloc((size_t)function->valueCount * sizeof(*owners));
        if (live == ZR_NULL || roots == ZR_NULL || owners == ZR_NULL) {
            free(live); free(roots); free(owners);
            return ZR_STATE_MAP_BUILD_OUT_OF_MEMORY;
        }
    }
    for (valueIndex = 0u; valueIndex < function->valueCount; ++valueIndex) {
        const SZrExecIrValue *value = &function->values[valueIndex];
        EZrExecIrStateMapOwnerState ownerState;
        if (!ZrCore_ExecIr_StateMapLivenessContains(liveness, instructionId,
                                            value->id, includeCurrent)) {
            continue;
        }
        ownerState = ZrCore_ExecIr_StateMapOwnerAt(
                function, ownership, liveness, instructionId, value->id, phase);
        if (ownerState == ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT) {
            free(live); free(roots); free(owners);
            return ZR_STATE_MAP_BUILD_INVALID;
        }
        live[liveCount] = value->id;
        owners[ownerCount++] = (TZrUInt32)ownerState;
        ++liveCount;
        if (value->ownership == ZR_EXEC_IR_OWNERSHIP_BORROWED &&
            (boundaryFlags & ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND) != 0u) {
            free(live); free(roots); free(owners);
            zr_state_map_set_diagnostic(diagnostic,
                                        ZR_EXEC_IR_DIAGNOSTIC_BORROWED_ACROSS_SUSPEND,
                                        function, instructionId, instruction->sourceId);
            return ZR_STATE_MAP_BUILD_BORROWED;
        }
        if ((value->ownership == ZR_EXEC_IR_OWNERSHIP_GC ||
             value->ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
             value->ownership == ZR_EXEC_IR_OWNERSHIP_SHARED) &&
            (ZrCore_ExecIr_StateMapOwnerMaskAt(function, ownership, liveness,
                    instructionId, value->id, phase) &
             ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED)) != 0u) {
            roots[rootCount++] = value->id;
        }
    }
    memset(&entry, 0, sizeof(entry));
    entry.sourceId = instruction->sourceId != 0u ? instruction->sourceId : instructionId;
    entry.instructionId = instructionId;
    entry.deoptId = instruction->deoptId;
    entry.resumeId = resumeId;
    entry.cleanupState = phase == ZR_EXEC_IR_STATE_BEFORE_EFFECT
                             ? 0u
                             : (phase == ZR_EXEC_IR_STATE_AFTER_EFFECT ? 1u : 2u);
    entry.boundaryFlags = boundaryFlags;
    entry.phase = phase;
    entry.effectIn = instruction->effectIn;
    entry.effectOut = instruction->effectOut;
    entry.handlerBlockId = zr_state_map_handler_block(function, instructionId,
                                                       boundaryFlags);
    entry.exceptionState = boundaryFlags & ZR_EXEC_IR_STATE_MAP_EXCEPTION_MASK;
    if (!zr_state_map_append_values(map, live, liveCount, &entry.liveValues, ZR_FALSE) ||
        !zr_state_map_append_values(map, roots, rootCount, &entry.rootValues, ZR_TRUE) ||
        !zr_state_map_append_owner_states(map, owners, ownerCount, &entry.ownerStates) ||
        !zr_state_map_append_entry(map, &entry)) {
        free(live); free(roots); free(owners);
        return ZR_STATE_MAP_BUILD_OUT_OF_MEMORY;
    }
    free(live); free(roots); free(owners);
    return ZR_STATE_MAP_BUILD_OK;
}

static TZrBool zr_state_map_resume_used(const SZrExecIrStateMap *map,
                                        TZrUInt32 resumeId) {
    TZrUInt32 index;

    if (map == ZR_NULL || resumeId == 0u) {
        return ZR_FALSE;
    }
    for (index = 0u; index < map->entryCount; ++index) {
        if (map->entries[index].resumeId == resumeId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_state_map_deopt_resume_id(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction,
        TZrExecIrSourceId sourceId,
        TZrUInt32 *resumeId) {
    TZrUInt32 index;
    TZrUInt32 matchingStates = 0u;
    const SZrExecIrDeoptState *matchingState = ZR_NULL;

    if (function == ZR_NULL || instruction == ZR_NULL || resumeId == ZR_NULL ||
        instruction->deoptId == 0u || function->deoptStates == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        if (state->id == instruction->deoptId) {
            ++matchingStates;
            matchingState = state;
        }
    }
    if (matchingStates != 1u || matchingState->sourceId != sourceId ||
        matchingState->resumeId == 0u) {
        return ZR_FALSE;
    }
    *resumeId = matchingState->resumeId;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_BuildStateMaps(SZrExecIrFunction *function,
                                       SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrStateMap candidate;
    SZrExecIrFunction verificationView;
    SZrStateMapLiveness liveness;
    SZrExecIrOwnerAnalysis ownership = {0};
    EZrExecutionDiagnosticCode livenessResult;
    TZrUInt32 instructionId;
    TZrUInt32 resumeId = 1u;

    zr_state_map_clear_diagnostic(diagnostic);
    if (function == ZR_NULL) {
        zr_state_map_set_diagnostic(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
                                    ZR_NULL, 0u, 0u);
        return ZR_FALSE;
    }
    if (function->sealed) {
        zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_SEALED,
                                    function, 0u, 0u);
        return ZR_FALSE;
    }
    /* The old map can describe an earlier IR revision. Verify the new IR
     * independently, retaining the old allocation until candidate publication. */
    verificationView = *function;
    verificationView.stateMap = ZR_NULL;
    if (!ZrCore_ExecIr_VerifyFunction(&verificationView, ZR_EXEC_IR_VERIFY_ALL, diagnostic)) {
        return ZR_FALSE;
    }
    livenessResult = ZrCore_ExecIr_StateMapLivenessBuild(function, &liveness);
    if (livenessResult != ZR_EXECUTION_DIAGNOSTIC_NONE) {
        zr_state_map_set_diagnostic(diagnostic, livenessResult, function, 0u, 0u);
        return ZR_FALSE;
    }
    if (!ZrCore_ExecIr_OwnerAnalysisBuild(function, &ownership, diagnostic)) {
        ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
        ZrCore_ExecIr_StateMapLivenessFree(&liveness);
        return ZR_FALSE;
    }
    ZrCore_ExecIr_StateMapInit(&candidate);
    candidate.functionToken = function->functionToken;
    candidate.signatureHash = function->signatureHash;
    candidate.generation = function->contract.generation;
    for (instructionId = 1u; instructionId <= function->instructionCount; ++instructionId) {
        const SZrExecIrInstruction *instruction = &function->instructions[instructionId - 1u];
        TZrUInt32 boundaryFlags = 0u;
        TZrUInt32 currentResume;
        EZrStateMapBuildResult result;

        if (!ownership.reachable[instructionId - 1u]) {
            continue;
        }
        if (!ZrCore_ExecIr_StateMapBoundaryFlags(instruction, &boundaryFlags)) {
            continue;
        }
        if (instruction->deoptId != 0u) {
            TZrExecIrSourceId sourceId =
                instruction->sourceId != 0u ? instruction->sourceId : instructionId;
            if (!zr_state_map_deopt_resume_id(function, instruction, sourceId,
                                              &currentResume)) {
                ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
                ZrCore_ExecIr_StateMapLivenessFree(&liveness);
                ZrCore_ExecIr_StateMapFree(&candidate);
                zr_state_map_set_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                        function, instructionId, instruction->sourceId);
                return ZR_FALSE;
            }
            if (zr_state_map_resume_used(&candidate, currentResume)) {
                ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
                ZrCore_ExecIr_StateMapLivenessFree(&liveness);
                ZrCore_ExecIr_StateMapFree(&candidate);
                zr_state_map_set_diagnostic(
                        diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                        function, instructionId, instruction->sourceId);
                return ZR_FALSE;
            }
            if (currentResume >= resumeId) {
                if (currentResume == UINT32_MAX) {
                    resumeId = UINT32_MAX;
                } else {
                    resumeId = currentResume + 1u;
                }
            }
        } else {
            if (resumeId == UINT32_MAX) {
                ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
                ZrCore_ExecIr_StateMapLivenessFree(&liveness);
                ZrCore_ExecIr_StateMapFree(&candidate);
                zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                                            function, instructionId, instruction->sourceId);
                return ZR_FALSE;
            }
            currentResume = resumeId++;
        }
        result = zr_state_map_add_checkpoint(&candidate, function, &liveness, &ownership, instruction,
                                             instructionId, boundaryFlags,
                                             ZR_EXEC_IR_STATE_BEFORE_EFFECT,
                                             currentResume, diagnostic);
        if (result == ZR_STATE_MAP_BUILD_OK) {
            result = zr_state_map_add_checkpoint(&candidate, function, &liveness, &ownership, instruction,
                                                 instructionId, boundaryFlags,
                                                 ZR_EXEC_IR_STATE_AFTER_EFFECT,
                                                 currentResume, diagnostic);
        }
        if (result == ZR_STATE_MAP_BUILD_OK &&
            (boundaryFlags & (ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW |
                              ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND |
                              ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP)) != 0u) {
            result = zr_state_map_add_checkpoint(&candidate, function, &liveness, &ownership, instruction,
                                                 instructionId, boundaryFlags,
                                                 ZR_EXEC_IR_STATE_CLEANUP_COMPLETE,
                                                 currentResume, diagnostic);
        }
        if (result != ZR_STATE_MAP_BUILD_OK) {
            ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
            ZrCore_ExecIr_StateMapLivenessFree(&liveness);
            if (result == ZR_STATE_MAP_BUILD_OUT_OF_MEMORY) {
                zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                            function, instructionId, instruction->sourceId);
            } else if (result == ZR_STATE_MAP_BUILD_INVALID) {
                zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                            function, instructionId, instruction->sourceId);
            }
            ZrCore_ExecIr_StateMapFree(&candidate);
            return ZR_FALSE;
        }
    }
    ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
    ZrCore_ExecIr_StateMapLivenessFree(&liveness);
    {
        SZrExecIrStateMap *committed = (SZrExecIrStateMap *)malloc(sizeof(*committed));
        if (committed == ZR_NULL) {
            ZrCore_ExecIr_StateMapFree(&candidate);
            zr_state_map_set_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
                                        function, 0u, 0u);
            return ZR_FALSE;
        }
        *committed = candidate;
        if (function->stateMap != ZR_NULL) {
            ZrCore_ExecIr_StateMapFree(function->stateMap);
            free(function->stateMap);
        }
        function->stateMap = committed;
    }
    return ZR_TRUE;
}

/**
 * @enum EZrStateMapBuildResult
 *
 * ## Summary
 * 表示单个状态检查点构造阶段的内部结果，供 BuildStateMaps 决定继续发布候选、报告分配失败、拒绝挂起时仍存活的借用值，或拒绝不一致的输入状态。
 *
 * ## Value model
 * `ZR_STATE_MAP_BUILD_OK` 表示检查点已追加到候选；`ZR_STATE_MAP_BUILD_OUT_OF_MEMORY` 表示临时缓冲或候选池扩容失败；`ZR_STATE_MAP_BUILD_BORROWED` 表示借用值跨挂起边界存活；`ZR_STATE_MAP_BUILD_INVALID` 表示阶段、参数或 owner 状态不适用于构造。
 *
 * ## Constraints and lifecycle
 * 这是本翻译单元私有的结果类型，不属于公开诊断枚举。调用者必须在失败时丢弃整个候选，不能把已部分追加的池当成完整 map。
 *
 * ## References
 * 各状态由 `zr_state_map_add_checkpoint` 产生，并在 `ZrParser_ExecIr_BuildStateMaps` 中消费。
 */

/**
 * @fn void zr_state_map_set_diagnostic(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, const SZrExecIrFunction *function, TZrUInt32 instructionId, TZrUInt32 sourceId)
 *
 * ## Summary
 * 将一次 map 构造失败写入可选诊断，并在函数块范围能定位该指令时补充 blockId；块定位只是诊断信息，不改变失败判定。
 *
 * ## Parameters
 * @param[out] diagnostic 可为空；非空时由本函数清零后写入 code、函数 token、指令和 source 标识。调用方负责提供独立存储。
 * @param[in] code 本次失败对应的诊断码。
 * @param[in] function 可为空的只读函数视图；仅用于读取 token 和尝试定位指令所属块。
 * @param[in] instructionId 一基指令标识；为零时不尝试定位块。
 * @param[in] sourceId 与失败位置关联的源标识。
 *
 * ## Returns
 * 本函数无返回值；诊断为空时不写入，非空时即使找不到块也保留诊断码与其余位置字段，并以无效 blockId 表示未定位。
 *
 * ## Constraints and lifecycle
 * 诊断存储在同步调用中借用；不得与 function 或其拥有的对象重叠。块查找要求传入的函数和 block 数组处于可读取状态；调用者应先完成相应结构验证。
 *
 * ## References
 * 公共入口在开始时清零诊断，并在验证、分析及检查点失败路径调用本 helper；挂起借用拒绝路径也由 `zr_state_map_add_checkpoint` 调用。
 */

/**
 * @fn TZrBool zr_state_map_reserve(void **storage, TZrUInt32 *capacity, TZrUInt32 count, TZrUInt32 needed, size_t elementSize)
 *
 * ## Summary
 * 为候选 map 的一个连续池按需扩容，采用几何增长减少逐项重分配，并在乘法或容量关系不安全时拒绝请求。
 *
 * ## Parameters
 * @param[in,out] storage 指向当前池指针的槽；成功扩容后更新为新分配地址，失败时保持原指针。
 * @param[in,out] capacity 当前已分配元素容量；仅成功扩容后更新。
 * @param[in] count 当前已使用元素数，必须不大于 needed。
 * @param[in] needed 所需的最小元素容量。
 * @param[in] elementSize 每个元素的非零字节数。
 *
 * ## Returns
 * @return 返回 ZR_TRUE 表示容量已足够或已成功扩容；返回 ZR_FALSE 表示参数/大小关系无效或 realloc 失败。失败时由 realloc 保留原分配，storage 与 capacity 槽未被本 helper 更新。
 *
 * ## Constraints and lifecycle
 * storage 与 capacity 必须指向有效且互不冲突的候选元数据；needed 和 elementSize 的乘积必须可由 size_t 表示。返回值不区分算术拒绝与分配失败；候选的所有权仍由调用者持有。
 *
 * ## References
 * `zr_state_map_append_values`、`zr_state_map_append_owner_states` 与 `zr_state_map_append_entry` 为各自候选池调用本 helper。
 */

/**
 * @fn TZrBool zr_state_map_append_values(SZrExecIrStateMap *map, const TZrExecIrValueId *values, TZrUInt32 count, SZrExecIrRange *range, TZrBool roots)
 *
 * ## Summary
 * 将当前检查点的 live value 或 root value ID 复制进候选 map 对应的连续池，并在成功后记录该段的起点和长度。
 *
 * ## Parameters
 * @param[in,out] map 由上层独占持有、尚未发布的候选状态 map。
 * @param[in] values 要复制的 ID 数组；count 非零时必须非空，数据在本次同步复制期间借用。
 * @param[in] count 要追加的元素数量。
 * @param[out] range 可为空；非空时仅在成功追加后写入新段范围。
 * @param[in] roots 为真时追加到 rootPool，否则追加到 valuePool。
 *
 * ## Returns
 * @return 返回 ZR_TRUE 表示本段复制完成并更新池计数及可选 range；返回 ZR_FALSE 表示参数、计数或分配检查失败。本次失败不回滚同一候选此前其他池或检查点的追加。
 *
 * ## Constraints and lifecycle
 * 本 helper 只适用于尚未发布的候选 map；发生任何追加失败后，公共构造者会释放整个候选，不会把部分池发布给 function。输入数组不得与可能扩容的目标池别名。
 *
 * ## References
 * `zr_state_map_add_checkpoint` 先准备 live/root 临时数组，再按顺序将两者写入候选；池容量由 `zr_state_map_reserve` 管理。
 */

/**
 * @fn TZrBool zr_state_map_append_owner_states(SZrExecIrStateMap *map, const TZrUInt32 *states, TZrUInt32 count, SZrExecIrRange *range)
 *
 * ## Summary
 * 将检查点每个 live value 对应的 owner 状态复制到候选 map 的 ownerStatePool，并在成功后记录该段范围。
 *
 * ## Parameters
 * @param[in,out] map 由上层独占持有、尚未发布的候选状态 map。
 * @param[in] states 与 live value 顺序一一对应的 owner 状态值；count 非零时必须非空，调用期间借用。
 * @param[in] count 要追加的 owner 状态数量。
 * @param[out] range 可为空；非空时仅在成功追加后写入该段范围。
 *
 * ## Returns
 * @return 返回 ZR_TRUE 表示状态数组已复制并更新池计数及可选 range；返回 ZR_FALSE 表示参数、计数或扩容失败，且不会回滚候选中此前完成的其他追加。
 *
 * ## Constraints and lifecycle
 * 本 helper 不独立发布或释放 map。任何后续步骤失败时，唯一上层调用者负责销毁整份候选；调用期间输入数组不得与可能扩容的 ownerStatePool 别名。
 *
 * ## References
 * `zr_state_map_add_checkpoint` 从 Core owner analysis 逐值构造状态数组并调用本 helper；扩容由 `zr_state_map_reserve` 完成。
 */

/**
 * @fn TZrExecIrBlockId zr_state_map_handler_block(const SZrExecIrFunction *function, TZrUInt32 instructionId, TZrUInt32 boundaryFlags)
 *
 * ## Summary
 * 为带 throw 边界的检查点查找包含该指令的块，并返回其 successor 中首个标记为 exception 或 cleanup 的块，供异常状态 map 记录处理目标。
 *
 * ## Parameters
 * @param[in] function 已通过结构验证、其指令区间和 successor 池可读取的函数。
 * @param[in] instructionId 一基指令标识。
 * @param[in] boundaryFlags Core 分类得到的边界标志；不含 THROW 时不查找 handler。
 *
 * ## Returns
 * @return 返回首个符合条件的 successor block ID；参数、边界类型、所属块或候选 successor 不满足查找条件时返回无效 block ID。
 *
 * ## Constraints and lifecycle
 * 该 helper 只读取函数池，不拥有或延长其生命周期。它按 successor 池顺序选择第一个带 exception/cleanup 标志的目标，不验证目标策略或重排控制流；调用前由公共入口完成 ExecIR 验证。
 *
 * ## References
 * `zr_state_map_add_checkpoint` 将结果写入 `SZrExecIrStateMapEntry.handlerBlockId`；边界分类来自 `ZrCore_ExecIr_StateMapBoundaryFlags`。
 */

/**
 * @fn EZrStateMapBuildResult zr_state_map_add_checkpoint(SZrExecIrStateMap *map, const SZrExecIrFunction *function, const SZrStateMapLiveness *liveness, const SZrExecIrOwnerAnalysis *ownership, const SZrExecIrInstruction *instruction, TZrUInt32 instructionId, TZrUInt32 boundaryFlags, EZrExecIrStateMapPhase phase, TZrUInt32 resumeId, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在一个已识别的可恢复边界和指定执行阶段，为当前指令投影 live values、managed roots、owner states 及边界元数据，并把完整 entry 追加到尚未发布的候选 map。
 *
 * ## Parameters
 * @param[in,out] map 公共 builder 创建并独占持有的候选；失败时可能已有部分池追加。
 * @param[in] function 已验证的只读 ExecIR 函数及其池，调用期间借用。
 * @param[in] liveness 与 function 对应的 liveness 分析结果；调用期间借用。
 * @param[in] ownership 与 function 对应的 owner 分析结果；调用期间借用。
 * @param[in] instruction 边界指令，必须对应 instructionId。
 * @param[in] instructionId 一基指令标识。
 * @param[in] boundaryFlags 已识别的边界类别，用于记录异常/挂起/清理元数据并拒绝挂起时仍存活的 borrowed value。
 * @param[in] phase 合法的 BEFORE_EFFECT、AFTER_EFFECT 或 CLEANUP_COMPLETE 阶段；该阶段决定本次 liveness 查询是否包含当前指令。
 * @param[in] resumeId 非零恢复标识，由公共 builder 分配或从唯一 deopt 状态复用。
 * @param[out] diagnostic 可为空；发现借用值跨挂起时写入具体诊断，其他内部结果由公共 builder 转为公开诊断。
 *
 * ## Returns
 * @return 返回 OK 表示 entry 已完整追加；OUT_OF_MEMORY、BORROWED 或 INVALID 表示未得到完整 entry。失败时临时数组由本函数释放，但先前已成功追加的 candidate pools 不会局部回滚。
 *
 * ## Constraints and lifecycle
 * 所有分析对象及函数池必须在本次调用期间有效且与该 function 对应。root pool 仅收集所选 managed ownership 且 owner mask 包含 INITIALIZED 的 live values；borrowed value 仅在其 live 且边界含 SUSPEND 时拒绝。候选始终由 `ZrParser_ExecIr_BuildStateMaps` 持有；调用方在任一步失败后必须释放整个 candidate，不能继续发布部分数据。多池追加依次发生，后续池或 entry 扩容失败可留下先前池已增长的候选。
 *
 * ## References
 * 唯一生产调用者是 `ZrParser_ExecIr_BuildStateMaps`，分别请求 BEFORE/AFTER，并对 throw、suspend 或 cleanup 边界请求 CLEANUP_COMPLETE；Core liveness 与 owner-state 查询提供每个 ID 的活跃性和状态。
 */

/**
 * @fn TZrBool zr_state_map_deopt_resume_id(const SZrExecIrFunction *function, const SZrExecIrInstruction *instruction, TZrExecIrSourceId sourceId, TZrUInt32 *resumeId)
 *
 * ## Summary
 * 在 deopt instruction 关联的状态表中确认恰有一条身份匹配且可恢复的记录，以复用其 resumeId，避免为同一 deopt 身份另造恢复编号。
 *
 * ## Parameters
 * @param[in] function 包含 deoptStates 的只读函数；其数组在调用期间有效。
 * @param[in] instruction 必须带非零 deoptId 的指令。
 * @param[in] sourceId 本次边界实际采用的源标识，与记录 sourceId 精确匹配。
 * @param[out] resumeId 非空输出槽；仅在找到唯一匹配且其 resumeId 非零时写入。
 *
 * ## Returns
 * @return 返回 ZR_TRUE 表示找到唯一匹配并写出 resumeId；返回 ZR_FALSE 表示参数、数组、匹配数量、sourceId 或 resumeId 不满足条件，此时输出槽不应读取。
 *
 * ## Constraints and lifecycle
 * 本 helper 不分配或转移状态表所有权；重复 deopt identity 即使指向不同恢复编号也被拒绝，由公共 builder 负责生成状态 map 诊断。
 *
 * ## References
 * `ZrParser_ExecIr_BuildStateMaps` 仅在带 deoptId 的可达边界调用，并在失败时释放候选后报告 STATE_MAP_INVALID。
 */
