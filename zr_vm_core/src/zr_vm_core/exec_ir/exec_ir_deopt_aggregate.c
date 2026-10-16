#include "exec_ir_deopt_aggregate.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static TZrBool zr_deopt_range_valid(SZrExecIrRange range, TZrUInt32 count) {
    return (TZrBool)(range.start <= count && range.count <= count - range.start);
}

static TZrBool zr_deopt_size_valid(TZrUInt32 count, size_t size) {
    return (TZrBool)((TZrUInt64)count <= (TZrUInt64)(SIZE_MAX / size));
}

static TZrBool zr_deopt_storage_valid(const void *storage, TZrUInt32 count,
                                      TZrUInt32 capacity, size_t size) {
    return (TZrBool)(count <= capacity &&
            ((capacity == 0u) == (storage == ZR_NULL)) &&
            (TZrUInt64)capacity <= (TZrUInt64)(SIZE_MAX / size));
}

static void zr_deopt_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                 EZrExecutionDiagnosticCode code,
                                 const SZrExecIrFunction *function,
                                 const SZrExecIrDeoptState *state,
                                 const SZrExecIrStateMapEntry *entry,
                                 TZrUInt32 expected, TZrUInt32 actual) {
    TZrUInt32 index;
    if (diagnostic == ZR_NULL) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
    diagnostic->expectedVersion = expected;
    diagnostic->actualVersion = actual;
    diagnostic->expectedHash = expected;
    diagnostic->actualHash = actual;
    if (entry != ZR_NULL) {
        diagnostic->sourceId = entry->sourceId;
        diagnostic->instructionId = entry->instructionId;
    } else if (state != ZR_NULL) {
        diagnostic->sourceId = state->sourceId;
        if (state->id != 0u && function->instructions != ZR_NULL &&
            function->instructionCount <= function->instructionCapacity) {
            for (index = 0u; index < function->instructionCount; ++index) {
                if (function->instructions[index].deoptId == state->id) {
                    diagnostic->instructionId = index + 1u;
                    break;
                }
            }
        }
    }
    if (diagnostic->instructionId != 0u && function->blocks != ZR_NULL &&
        function->blockCount <= function->blockCapacity) {
        for (index = 0u; index < function->blockCount; ++index) {
            const SZrExecIrBlock *block = &function->blocks[index];
            TZrUInt32 position = diagnostic->instructionId - 1u;
            if (position >= block->instructions.start &&
                position - block->instructions.start < block->instructions.count) {
                diagnostic->blockId = block->id;
                break;
            }
        }
    }
}

/* State ranges have been checked before this source-context lookup. */
static const SZrExecIrDeoptState *zr_deopt_aggregate_state(
        const SZrExecIrFunction *function, TZrUInt32 aggregateIndex) {
    TZrUInt32 index;
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        if (aggregateIndex >= state->aggregates.start &&
            aggregateIndex - state->aggregates.start < state->aggregates.count) {
            return state;
        }
    }
    return ZR_NULL;
}

static const SZrExecIrDeoptState *zr_deopt_field_state(
        const SZrExecIrFunction *function, TZrUInt32 fieldIndex) {
    TZrUInt32 index;
    for (index = 0u; index < function->deoptAggregateCount; ++index) {
        SZrExecIrRange fields = function->deoptAggregates[index].fields;
        if (fieldIndex >= fields.start && fieldIndex - fields.start < fields.count) {
            const SZrExecIrDeoptState *state = zr_deopt_aggregate_state(function, index);
            if (state != ZR_NULL) return state;
        }
    }
    return ZR_NULL;
}

static TZrBool zr_deopt_field_valid(const SZrExecIrFunction *function,
                                    TZrUInt32 index,
                                    SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrDeoptAggregateField *field = &function->deoptAggregateFields[index];
    EZrExecutionDiagnosticCode code = ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
    TZrUInt32 expected = 0u, actual = 0u;
    switch (field->kind) {
        case ZR_EXEC_IR_DEOPT_FIELD_UNINITIALIZED:
            if (field->valueId == 0u && field->aggregateId == 0u) return ZR_TRUE;
            actual = field->valueId != 0u ? field->valueId : field->aggregateId;
            break;
        case ZR_EXEC_IR_DEOPT_FIELD_VALUE:
            if (field->aggregateId != 0u) {
                actual = field->aggregateId;
            } else if (field->valueId == 0u || field->valueId > function->valueCount ||
                       function->values[field->valueId - 1u].id != field->valueId) {
                code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE;
                expected = function->valueCount;
                actual = field->valueId;
            } else {
                return ZR_TRUE;
            }
            break;
        case ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE:
            if (field->valueId == 0u && field->aggregateId != 0u) return ZR_TRUE;
            expected = field->valueId == 0u ? 1u : 0u;
            actual = field->valueId;
            break;
        default:
            expected = ZR_EXEC_IR_DEOPT_FIELD_KIND_COUNT;
            actual = (TZrUInt32)field->kind;
            break;
    }
    zr_deopt_diagnostic(diagnostic, code, function,
                         zr_deopt_field_state(function, index), ZR_NULL,
                         expected, actual);
    return ZR_FALSE;
}

static TZrBool zr_deopt_state_graph_valid(const SZrExecIrFunction *function,
                                          const SZrExecIrDeoptState *state,
                                          SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index, other, fieldIndex;
    for (index = 0u; index < state->aggregates.count; ++index) {
        const SZrExecIrDeoptAggregate *aggregate =
                &function->deoptAggregates[state->aggregates.start + index];
        for (other = 0u; other < index; ++other) {
            if (function->deoptAggregates[state->aggregates.start + other].identityId ==
                aggregate->identityId) {
                zr_deopt_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
                                     function, state, ZR_NULL, 1u, aggregate->identityId);
                return ZR_FALSE;
            }
        }
    }
    /* First establish unique identities, then resolve edges. A duplicate can
     * otherwise hide behind a missing edge to the identity it replaced. */
    for (index = 0u; index < state->aggregates.count; ++index) {
        const SZrExecIrDeoptAggregate *aggregate =
                &function->deoptAggregates[state->aggregates.start + index];
        /* Resolve IDs without traversing edges: forward edges and cycles are legal. */
        for (fieldIndex = 0u; fieldIndex < aggregate->fields.count; ++fieldIndex) {
            const SZrExecIrDeoptAggregateField *field =
                    &function->deoptAggregateFields[aggregate->fields.start + fieldIndex];
            TZrUInt32 matches = 0u;
            if (field->kind != ZR_EXEC_IR_DEOPT_FIELD_AGGREGATE) continue;
            for (other = 0u; other < state->aggregates.count; ++other) {
                if (function->deoptAggregates[state->aggregates.start + other].identityId ==
                    field->aggregateId) ++matches;
            }
            if (matches != 1u) {
                zr_deopt_diagnostic(diagnostic,
                        matches > 1u ? ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION
                                     : ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                        function, state, ZR_NULL, 1u, matches);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

TZrBool zr_exec_ir_deopt_aggregates_validate(
        const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index, fieldIndex, other;
    if (function == ZR_NULL ||
        !zr_deopt_storage_valid(function->values, function->valueCount,
                                  function->valueCapacity, sizeof(*function->values)) ||
        !zr_deopt_storage_valid(function->instructions, function->instructionCount,
                                  function->instructionCapacity, sizeof(*function->instructions)) ||
        !zr_deopt_storage_valid(function->deoptStates, function->deoptStateCount,
                                  function->deoptStateCapacity, sizeof(*function->deoptStates)) ||
        !zr_deopt_storage_valid(function->deoptAggregates, function->deoptAggregateCount,
                                  function->deoptAggregateCapacity, sizeof(*function->deoptAggregates)) ||
        !zr_deopt_storage_valid(function->deoptAggregateFields, function->deoptAggregateFieldCount,
                                  function->deoptAggregateFieldCapacity,
                                  sizeof(*function->deoptAggregateFields))) {
        zr_deopt_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                             function, ZR_NULL, ZR_NULL, 0u, 0u);
        return ZR_FALSE;
    }
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        if (!zr_deopt_range_valid(state->aggregates, function->deoptAggregateCount)) {
            zr_deopt_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                    function, state, ZR_NULL, function->deoptAggregateCount,
                    state->aggregates.start > function->deoptAggregateCount
                            ? state->aggregates.start : state->aggregates.count);
            return ZR_FALSE;
        }
    }
    for (index = 0u; index < function->deoptAggregateCount; ++index) {
        const SZrExecIrDeoptAggregate *aggregate = &function->deoptAggregates[index];
        const SZrExecIrDeoptState *state = zr_deopt_aggregate_state(function, index);
        if (!zr_deopt_range_valid(aggregate->fields, function->deoptAggregateFieldCount)) {
            zr_deopt_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                    function, state, ZR_NULL, function->deoptAggregateFieldCount,
                    aggregate->fields.start > function->deoptAggregateFieldCount
                            ? aggregate->fields.start : aggregate->fields.count);
            return ZR_FALSE;
        }
        if (aggregate->identityId == 0u || aggregate->typeToken == 0u || aggregate->layoutId == 0u) {
            zr_deopt_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                 function, state, ZR_NULL, 1u, 0u);
            return ZR_FALSE;
        }
        for (fieldIndex = 0u; fieldIndex < aggregate->fields.count; ++fieldIndex) {
            TZrUInt32 logicalIndex =
                    function->deoptAggregateFields[aggregate->fields.start + fieldIndex].fieldIndex;
            for (other = 0u; other < fieldIndex; ++other) {
                if (function->deoptAggregateFields[aggregate->fields.start + other].fieldIndex ==
                    logicalIndex) {
                    zr_deopt_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
                                         function, state, ZR_NULL, 1u, logicalIndex);
                    return ZR_FALSE;
                }
            }
        }
    }
    for (index = 0u; index < function->deoptAggregateFieldCount; ++index) {
        if (!zr_deopt_field_valid(function, index, diagnostic)) return ZR_FALSE;
    }
    for (index = 0u; index < function->deoptStateCount; ++index) {
        if (!zr_deopt_state_graph_valid(function, &function->deoptStates[index], diagnostic)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrCore_ExecIr_ValidateDeoptAggregates(
        const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic) {
    return zr_exec_ir_deopt_aggregates_validate(function, diagnostic);
}

TZrBool ZrCore_ExecIr_DeoptAggregateValueReferenced(
        const SZrExecIrFunction *function, TZrExecIrValueId valueId) {
    TZrUInt32 index;
    if (function == ZR_NULL ||
        !zr_deopt_storage_valid(function->deoptAggregateFields,
                function->deoptAggregateFieldCount,
                function->deoptAggregateFieldCapacity,
                sizeof(*function->deoptAggregateFields))) return ZR_TRUE;
    for (index = 0u; index < function->deoptAggregateFieldCount; ++index) {
        const SZrExecIrDeoptAggregateField *field = &function->deoptAggregateFields[index];
        if (field->kind == ZR_EXEC_IR_DEOPT_FIELD_VALUE && field->valueId == valueId)
            return ZR_TRUE;
    }
    return ZR_FALSE;
}

TZrBool ZrCore_ExecIr_DeoptAggregateValueReferencedAt(
        const SZrExecIrFunction *function, TZrExecIrDeoptId deoptId,
        TZrExecIrValueId valueId) {
    TZrUInt32 stateIndex, aggregateIndex, fieldIndex;
    if (deoptId == 0u) return ZR_FALSE;
    if (function == ZR_NULL ||
        !zr_deopt_storage_valid(function->deoptStates, function->deoptStateCount,
                function->deoptStateCapacity, sizeof(*function->deoptStates)) ||
        !zr_deopt_storage_valid(function->deoptAggregates, function->deoptAggregateCount,
                function->deoptAggregateCapacity, sizeof(*function->deoptAggregates)) ||
        !zr_deopt_storage_valid(function->deoptAggregateFields, function->deoptAggregateFieldCount,
                function->deoptAggregateFieldCapacity, sizeof(*function->deoptAggregateFields)))
        return ZR_TRUE;
    for (stateIndex = 0u; stateIndex < function->deoptStateCount; ++stateIndex) {
        const SZrExecIrDeoptState *state = &function->deoptStates[stateIndex];
        if (state->id != deoptId) continue;
        if (!zr_deopt_range_valid(state->aggregates, function->deoptAggregateCount)) return ZR_TRUE;
        for (aggregateIndex = 0u; aggregateIndex < state->aggregates.count; ++aggregateIndex) {
            const SZrExecIrDeoptAggregate *aggregate =
                    &function->deoptAggregates[state->aggregates.start + aggregateIndex];
            if (!zr_deopt_range_valid(aggregate->fields, function->deoptAggregateFieldCount))
                return ZR_TRUE;
            for (fieldIndex = 0u; fieldIndex < aggregate->fields.count; ++fieldIndex) {
                const SZrExecIrDeoptAggregateField *field =
                        &function->deoptAggregateFields[aggregate->fields.start + fieldIndex];
                if (field->kind == ZR_EXEC_IR_DEOPT_FIELD_VALUE && field->valueId == valueId)
                    return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static void zr_deopt_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 index;
    for (index = 0u; index < 4u; ++index) {
        *hash ^= (TZrUInt64)((value >> (index * 8u)) & 0xffu);
        *hash *= UINT64_C(1099511628211);
    }
}

TZrUInt64 ZrCore_ExecIr_DeoptAggregateHash(const SZrExecIrFunction *function) {
    TZrUInt64 hash = UINT64_C(1469598103934665603);
    TZrUInt32 index;
    if (!zr_exec_ir_deopt_aggregates_validate(function, ZR_NULL)) return 0u;
    zr_deopt_hash_u32(&hash, function->deoptStateCount);
    for (index = 0u; index < function->deoptStateCount; ++index) {
        const SZrExecIrDeoptState *state = &function->deoptStates[index];
        zr_deopt_hash_u32(&hash, state->id);
        zr_deopt_hash_u32(&hash, state->aggregates.start);
        zr_deopt_hash_u32(&hash, state->aggregates.count);
    }
    zr_deopt_hash_u32(&hash, function->deoptAggregateCount);
    for (index = 0u; index < function->deoptAggregateCount; ++index) {
        const SZrExecIrDeoptAggregate *aggregate = &function->deoptAggregates[index];
        zr_deopt_hash_u32(&hash, aggregate->identityId);
        zr_deopt_hash_u32(&hash, aggregate->typeToken);
        zr_deopt_hash_u32(&hash, aggregate->layoutId);
        zr_deopt_hash_u32(&hash, aggregate->fields.start);
        zr_deopt_hash_u32(&hash, aggregate->fields.count);
    }
    zr_deopt_hash_u32(&hash, function->deoptAggregateFieldCount);
    for (index = 0u; index < function->deoptAggregateFieldCount; ++index) {
        const SZrExecIrDeoptAggregateField *field = &function->deoptAggregateFields[index];
        zr_deopt_hash_u32(&hash, field->fieldIndex);
        zr_deopt_hash_u32(&hash, (TZrUInt32)field->kind);
        zr_deopt_hash_u32(&hash, field->valueId);
        zr_deopt_hash_u32(&hash, field->aggregateId);
    }
    return hash != 0u ? hash : UINT64_C(1);
}

TZrBool zr_exec_ir_deopt_aggregates_live(
        const SZrExecIrFunction *function, const SZrExecIrDeoptState *state,
        const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index, fieldIndex, liveIndex;
    for (index = 0u; index < state->aggregates.count; ++index) {
        const SZrExecIrDeoptAggregate *aggregate =
                &function->deoptAggregates[state->aggregates.start + index];
        for (fieldIndex = 0u; fieldIndex < aggregate->fields.count; ++fieldIndex) {
            const SZrExecIrDeoptAggregateField *field =
                    &function->deoptAggregateFields[aggregate->fields.start + fieldIndex];
            if (field->kind != ZR_EXEC_IR_DEOPT_FIELD_VALUE) continue;
            for (liveIndex = 0u; liveIndex < entry->liveValues.count; ++liveIndex) {
                if (map->valuePool[entry->liveValues.start + liveIndex] == field->valueId) break;
            }
            if (liveIndex == entry->liveValues.count) {
                zr_deopt_diagnostic(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
                                     function, state, entry, field->valueId, 0u);
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

TZrBool zr_exec_ir_deopt_aggregates_prepare(
        const SZrExecIrFunction *function, const SZrExecIrStateMapEntry *entry,
        SZrExecIrMaterializedState *prepared, SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrDeoptState *state = ZR_NULL;
    TZrUInt32 index, fieldCount = 0u, offset = 0u;
    EZrExecutionDiagnosticCode failure = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
    if (entry->deoptId == 0u) return ZR_TRUE;
    for (index = 0u; index < function->deoptStateCount; ++index) {
        if (function->deoptStates[index].id == entry->deoptId) {
            state = &function->deoptStates[index];
            break;
        }
    }
    if (state == ZR_NULL) {
        failure = ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
        goto failed;
    }
    for (index = 0u; index < state->aggregates.count; ++index) {
        TZrUInt32 count = function->deoptAggregates[state->aggregates.start + index].fields.count;
        if (count > UINT32_MAX - fieldCount) goto failed;
        fieldCount += count;
    }
    if (!zr_deopt_size_valid(state->aggregates.count, sizeof(*prepared->aggregates)) ||
        !zr_deopt_size_valid(fieldCount, sizeof(*prepared->aggregateFields))) goto failed;
    failure = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
    if (state->aggregates.count != 0u) {
        prepared->aggregates = malloc((size_t)state->aggregates.count * sizeof(*prepared->aggregates));
        if (prepared->aggregates == ZR_NULL) goto failed;
    }
    if (fieldCount != 0u) {
        prepared->aggregateFields = malloc((size_t)fieldCount * sizeof(*prepared->aggregateFields));
        if (prepared->aggregateFields == ZR_NULL) goto failed;
    }
    for (index = 0u; index < state->aggregates.count; ++index) {
        const SZrExecIrDeoptAggregate *source =
                &function->deoptAggregates[state->aggregates.start + index];
        prepared->aggregates[index] = *source;
        prepared->aggregates[index].fields.start = offset;
        if (source->fields.count != 0u) {
            memcpy(&prepared->aggregateFields[offset],
                    &function->deoptAggregateFields[source->fields.start],
                    (size_t)source->fields.count * sizeof(*prepared->aggregateFields));
        }
        offset += source->fields.count;
    }
    prepared->aggregateCount = prepared->aggregateCapacity = state->aggregates.count;
    prepared->aggregateFieldCount = prepared->aggregateFieldCapacity = fieldCount;
    return ZR_TRUE;
failed:
    zr_deopt_diagnostic(diagnostic, failure, function, state, entry, 0u, 0u);
    return ZR_FALSE;
}

/**
 * @fn static TZrBool zr_deopt_range_valid(SZrExecIrRange range, TZrUInt32 count)
 *
 * ## Summary
 * 以减法式边界检查配方选区，避免先算 start+count 回绕。
 *
 * ## Parameters
 * @param[in] range 零基起始与长度。
 * @param[in] count 所属池的逻辑元素数。
 *
 * ## Returns
 * @return start<=count且长度<=count-start为TRUE，否则FALSE。
 *
 * ## Constraints and lifecycle
 * 纯值查询，无分配/输出，允许零长度start==count；caller另核实际池形状/可读性。
 * 失败/资源边界：只返回FALSE，不写诊断或修范围。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:7。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:196。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:207。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:280。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:284。
 */

/**
 * @fn static TZrBool zr_deopt_size_valid(TZrUInt32 count, size_t size)
 *
 * ## Summary
 * 检查待分配recipe元素数乘宽度能由size_t表示。
 *
 * ## Parameters
 * @param[in] count 待分配元素数。
 * @param[in] size 严格大于零的宽度；现有两caller均传sizeof。
 *
 * ## Returns
 * @return count<=SIZE_MAX/size为TRUE，否则FALSE。
 *
 * ## Constraints and lifecycle
 * size!=0是前提，函数无零除保护；检查不分配或保证malloc成功。
 * 失败/资源边界：仅FALSE，由上层诊断CAPACITY_OVERFLOW。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:11。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:383。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:384。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:410。
 */

/**
 * @fn static TZrBool zr_deopt_storage_valid(const void *storage, TZrUInt32 count, TZrUInt32 capacity, size_t size)
 *
 * ## Summary
 * 核对侧表count/capacity、NULL配对和容量字节尺寸。
 *
 * ## Parameters
 * @param[in] storage 仅作指针值检查，不解引用。
 * @param[in] count 逻辑数量。
 * @param[in] capacity 记录容量。
 * @param[in] size 严格大于零的元素宽度；caller均传sizeof。
 *
 * ## Returns
 * @return count<=capacity、capacity为零与storage为NULL等价且容量乘积可表示时TRUE，否则FALSE。
 *
 * ## Constraints and lifecycle
 * count为零仍核配对；无分配/释放/所有权转移，不证明实际内存可读或freeability。
 * 失败/资源边界：只返回FALSE，参数和诊断不变。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:15。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:179。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:187。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:252。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:270。
 */

/**
 * @fn static void zr_deopt_diagnostic(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code, const SZrExecIrFunction *function, const SZrExecIrDeoptState *state, const SZrExecIrStateMapEntry *entry, TZrUInt32 expected, TZrUInt32 actual)
 *
 * ## Summary
 * 重置独立诊断，按entry优先、否则state的顺序补充有限位置上下文。
 *
 * ## Parameters
 * @param[out] diagnostic 可空；非空先整体清零。
 * @param[in] code 调用者所选错误码。
 * @param[in] function 不解析位置的早拒绝分支可空；读取state指令/block上下文时须非空且实际池可读。
 * @param[in] state 可空借用state，entry为空时以sourceId/id反查指令。
 * @param[in] entry 可空借用checkpoint，非空优先复制其sourceId/instructionId。
 * @param[in] expected 同时写expectedVersion/expectedHash的32位值。
 * @param[in] actual 同时写actualVersion/actualHash的32位值。
 *
 * ## Returns
 * 无C返回值；空diagnostic不写，否则发布能确定的诊断字段。
 *
 * ## Constraints and lifecycle
 * diagnostic 可空；非空须独立可写，不覆盖借用输入/池。非空调用始终先清零并重建诊断；上层校验成功而未调用本助手时，才可能保留旧诊断。state反查取instruction.deoptId首个匹配；block取首个覆盖instructionId-1的项。只核部分形状，实际池另须可读；未定位字段保持零。无分配、保留借用或同步。
 * 失败/资源边界：不修输入；不保证任意NULL function与非空state/entry组合安全。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:22。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:29。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:30。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:32。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:33。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:35。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:37。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:42。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:52。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:57。
 */

/**
 * @fn static const SZrExecIrDeoptState *zr_deopt_aggregate_state( const SZrExecIrFunction *function, TZrUInt32 aggregateIndex)
 *
 * ## Summary
 * 为aggregate下标寻找首个包含它的state，供错误定位。
 *
 * ## Parameters
 * @param[in] function 非空函数，state池及其aggregate ranges已有效可读。
 * @param[in] aggregateIndex 全局aggregate零基下标。
 *
 * ## Returns
 * @return 返回首个匹配的借用state；不存在为NULL。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。重叠选区仅按数组首项定位，不在此拒绝；state池替换/重分配/释放使借用失效，内容修改改变所见。无复制/所有权转移。
 * 失败/资源边界：无诊断，只有NULL。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:67。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:72。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:74。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:86。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:206。
 */

/**
 * @fn static const SZrExecIrDeoptState *zr_deopt_field_state( const SZrExecIrFunction *function, TZrUInt32 fieldIndex)
 *
 * ## Summary
 * 经aggregate.fields选区为field寻找可定位的state。
 *
 * ## Parameters
 * @param[in] function 非空，相关池/range已验证并可读。
 * @param[in] fieldIndex 全局field零基下标。
 *
 * ## Returns
 * @return 首个包含field且能定位state的aggregate对应state借用；否则NULL。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。包含field但无state的aggregate会跳过继续找；不拒共享/重叠范围。借用随state池替换/释放失效，无所有权转移。
 * 失败/资源边界：仅NULL，不制造不存在的位置。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:80。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:85。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:86。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:87。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:127。
 */

/**
 * @fn static TZrBool zr_deopt_field_valid(const SZrExecIrFunction *function, TZrUInt32 index, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 检查全局field的kind、互斥ID及VALUE引用合法性。
 *
 * ## Parameters
 * @param[in] function 非空且池形状/range已有效，index在fieldCount内。
 * @param[in] index 全局field零基下标。
 * @param[out] diagnostic 可空的独立失败诊断。
 *
 * ## Returns
 * @return 本field的payload组合有效为TRUE，否则FALSE。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。UNINITIALIZED两ID均零；VALUE要求aggregateId零、valueId在1..valueCount且values[id-1].id匹配；AGGREGATE要求valueId零、aggregateId非零。不在此解析目标identity或验证ownership/liveness/运行时类型。diagnostic 可空；非空须独立可写，不覆盖借用输入/池。失败先清零再写；成功不主动清除旧内容。
 * 失败/资源边界：非法VALUE引用用INVALID_VALUE，其它kind/payload用STATE_MAP_INVALID，经field→aggregate→state定位；输入不变。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:93。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:100。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:104。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:107。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:116。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:121。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:126。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:233。
 */

/**
 * @fn static TZrBool zr_deopt_state_graph_valid(const SZrExecIrFunction *function, const SZrExecIrDeoptState *state, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 检查同一state选区内identity唯一及AGGREGATE边恰一命中。
 *
 * ## Parameters
 * @param[in] function 非空且所有池/range及field局部payload已验证。
 * @param[in] state 非空借用state，本次aggregate选区。
 * @param[out] diagnostic 可空的独立失败诊断。
 *
 * ## Returns
 * @return 无重复identity且每条边在同一选区恰一命中为TRUE，否则FALSE。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。先完整查identity重复再查缺边。无DFS/拓扑排序；允许前向边、别名和循环，不要求跨state/全局identity唯一。diagnostic 可空；非空须独立可写，不覆盖借用输入/池。失败先清零再写；成功不主动清除旧内容。
 * 失败/资源边界：重复为DUPLICATE_DEFINITION，边0命中为STATE_MAP_INVALID、多命中为DUPLICATE_DEFINITION；不修图。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:132。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:139。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:142。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:148。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:153。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:159。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:164。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:236。
 */

/**
 * @fn TZrBool zr_exec_ir_deopt_aggregates_validate( const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 依次预检recipe侧表形状、选区、身份/字段索引、payload及各state图闭合。
 *
 * ## Parameters
 * @param[in] function 可空且空值拒绝；非空记录/池及诊断定位所需存储须可读。
 * @param[out] diagnostic 可空独立诊断，失败重写、成功保留旧内容。
 *
 * ## Returns
 * @return 所有本模块检查通过为TRUE，首个错误为FALSE。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。所有全局aggregates/fields均检查，包括未选中的项。identityId/typeToken/layoutId非零；每aggregate的fieldIndex唯一、每state的identityId唯一。先range后索引，允许合法别名/循环。不替代完整CFG/SSA、deopt state唯一ID/source/resume、owner/liveness或运行时类型/layout验证。diagnostic 可空；非空须独立可写，不覆盖借用输入/池。失败先清零再写；成功不主动清除旧内容。
 * 失败/资源边界：NULL/形状/身份等为STATE_MAP_INVALID，range为INVALID_RANGE，重复为DUPLICATE_DEFINITION，VALUE引用为INVALID_VALUE；无分配，输入和已发布map不变。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:175。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:178。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:196。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:207。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:214。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:222。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:233。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:236。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:573。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:403。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_ValidateDeoptAggregates( const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 向Parser投影/优化入口公开同一recipe预检，原样转发返回和诊断。
 *
 * ## Parameters
 * @param[in] function 可空且空值拒绝，其余被读记录/池须稳定可读。
 * @param[out] diagnostic 可空独立诊断，失败重写、成功不清空。
 *
 * ## Returns
 * @return 直接返回私有验证函数结果。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。diagnostic 可空；非空须独立可写，不覆盖借用输入/池。失败先清零再写；成功不主动清除旧内容。验证边界为形状/range/字段payload与state内图闭合，不增加CFG、owner或执行语义。
 * 失败/资源边界：只诊断，不分配/修复/替换函数或map。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:243。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:245。
 * zr_vm_core/include/zr_vm_core/exec_ir.h:707。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c:133。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c:237。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_DeoptAggregateValueReferenced( const SZrExecIrFunction *function, TZrExecIrValueId valueId)
 *
 * ## Summary
 * 查询整个逻辑field池的VALUE用途；存储不确定时保守保留值。
 *
 * ## Parameters
 * @param[in] function 可空，空值/field池形状非法时保守返回TRUE。
 * @param[in] valueId 按原数值匹配，不自行核非零、范围或values[id-1]。
 *
 * ## Returns
 * @return 匹配VALUE field或无法由形状安全排除引用时TRUE；形状合格且无匹配时FALSE。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。包括未被state/指令选中的字段，不判断可达性或payload合法性，不解析AGGREGATE边。无诊断/分配/修改，需要精确语义的caller先预检。
 * 失败/资源边界：NULL/畸形field池返回TRUE，并不证明真实用途。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:248。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:251。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:258。
 * zr_vm_core/include/zr_vm_core/exec_ir.h:710。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_allocation.c:221。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_match.c:142。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_DeoptAggregateValueReferencedAt( const SZrExecIrFunction *function, TZrExecIrDeoptId deoptId, TZrExecIrValueId valueId)
 *
 * ## Summary
 * 按deoptId查询匹配state选区内的VALUE用途，局部不确定性保守视为引用。
 *
 * ## Parameters
 * @param[in] function 可空；deoptId非零且空值/相关池形状非法时TRUE。
 * @param[in] deoptId 零先于输入检查返回FALSE；非零扫描全部同ID state。
 * @param[in] valueId 原数值匹配的SSA ID，不核合法范围。
 *
 * ## Returns
 * @return 零deoptId为FALSE；匹配VALUE或形状/命中范围不确定时TRUE；结束无匹配为FALSE。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。只核三个池形状与命中state/aggregate范围，不拒重复state ID、不验证未匹配range；多个同ID state合并扫描。无匹配state时不遍历其它选区。无分配/诊断/写入。
 * 失败/资源边界：不确定时TRUE，不是精确引用保证。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:264。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:268。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:270。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:279。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:280。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:284。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:289。
 * zr_vm_core/include/zr_vm_core/exec_ir.h:713。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c:914。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c:948。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c:987。
 */

/**
 * @fn static void zr_deopt_hash_u32(TZrUInt64 *hash, TZrUInt32 value)
 *
 * ## Summary
 * 按固定低字节到高字节顺序把32位值混入64位recipe hash。
 *
 * ## Parameters
 * @param[in,out] hash 非空可写累积hash，原位更新，不覆盖未读元数据。
 * @param[in] value 待编码32位标量。
 *
 * ## Returns
 * 无C返回值；四次xor-byte/乘固定常数后更新hash。
 *
 * ## Constraints and lifecycle
 * 使用无符号64位回绕，显式字节序不依赖宿主内存端序。无分配/保留借用/同步，不提供加密或无碰撞保证。
 * 失败/资源边界：无可恢复失败；无效hash指针违反前提。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:297。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:299。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:300。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:301。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:309。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:331。
 */

/**
 * @fn TZrUInt64 ZrCore_ExecIr_DeoptAggregateHash(const SZrExecIrFunction *function)
 *
 * ## Summary
 * 生成recipe逻辑表及选区顺序的确定非零变化标记，供Parser缓存/函数hash使用。
 *
 * ## Parameters
 * @param[in] function 可空/畸形，预检失败为零；其余实际被读池须稳定可读。
 *
 * ## Returns
 * @return 预检失败为0；有效混合值恰为0时映射为1，其余返回混合值。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。混入stateCount及各id/aggregate range、aggregateCount及各identity/type/layout/field range、fieldCount及各fieldIndex/kind/valueId/aggregateId。包括逻辑全局表和顺序；不直接混入state.sourceId/resumeId/cleanup、函数epoch或运行时payload。无分配，空diagnostic预检，不等价全函数身份或无碰撞证明。
 * 失败/资源边界：只返回0，不修输入或提供诊断。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:305。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:306。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:308。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:312。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:319。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:328。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:333。
 * zr_vm_core/include/zr_vm_core/exec_ir.h:717。
 * zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c:83。
 * tests/parser/test_ssa_deopt_aggregates.c:438。
 * tests/parser/test_ssa_deopt_aggregates.c:460。
 */

/**
 * @fn TZrBool zr_exec_ir_deopt_aggregates_live( const SZrExecIrFunction *function, const SZrExecIrDeoptState *state, const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 确认所选state的每个VALUE field出现在checkpoint已验证的liveValues中。
 *
 * ## Parameters
 * @param[in] function 非空稳定函数，recipe预检已成功。
 * @param[in] state 非空state，其aggregate/field范围可安全遍历。
 * @param[in] map 非空图，valuePool及entry.liveValues范围已有效。
 * @param[in] entry 非空checkpoint，caller已核其identity/phase与state匹配。
 * @param[out] diagnostic 可空独立诊断。
 *
 * ## Returns
 * @return 所有VALUE依赖存在为TRUE，首次遗漏为FALSE。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。只核存在性，不核唯一出现/root/owner/availability；AGGREGATE和UNINITIALIZED不当SSA value，不递归展开图。diagnostic 可空；非空须独立可写，不覆盖借用输入/池。失败先清零再写；成功不主动清除旧内容。
 * 失败/资源边界：遗漏写STATE_MAP_INVALID、expected=field.valueId/actual=0并优先entry位置；不改输入/target。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:336。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:347。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:348。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:351。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:352。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.h:11。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:369。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:400。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:503。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:504。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:62。
 */

/**
 * @fn TZrBool zr_exec_ir_deopt_aggregates_prepare( const SZrExecIrFunction *function, const SZrExecIrStateMapEntry *entry, SZrExecIrMaterializedState *prepared, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 把checkpoint选中的recipe复制为紧凑、独立拥有的两份逻辑数组，附加到未发布prepared。
 *
 * ## Parameters
 * @param[in] function 非空稳定函数，recipe和checkpoint语义验证已完成。
 * @param[in] entry 非空已验证checkpoint；非零deoptId应有唯一匹配state。
 * @param[in,out] prepared 非空Init后的未发布候选；其它三池可已准备，两recipe指针/count/capacity须为空。
 * @param[out] diagnostic 可空，须独立于prepared/function/entry和所有池。
 *
 * ## Returns
 * @return 无需recipe(deoptId0)或复制成功为TRUE；无state、累计/字节尺寸溢出或recipe malloc失败为FALSE。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。非零ID取首个state，唯一性由上游保证。先核UInt32累计与size_t尺寸，再分别malloc，按选区顺序复制并将fields.start重基为紧凑offset；identity/payload原样保留，不构造运行时对象。不能传已发布target或已有recipe池。成功新池由prepared拥有，唯一上层负责提交。
 * 失败/资源边界：失败可能已写第一份malloc指针但count/capacity尚未赋值；helper不free/回滚，上层必须MaterializedStateFree候选，不能消费/重试半成品。deoptId0不改prepared。错误为STATE_MAP_INVALID/CAPACITY_OVERFLOW/OUT_OF_MEMORY；旧target不在此helper处理。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:361。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:367。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:368。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:374。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:378。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:380。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:383。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:387。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:391。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:398。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:406。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:728。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:812。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:813。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:818。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:819。
 * tests/parser/test_ssa_deopt_aggregates.c:385。
 * tests/parser/test_ssa_deopt_aggregates.c:392。
 * tests/parser/test_ssa_deopt_aggregates.c:401。
 */
