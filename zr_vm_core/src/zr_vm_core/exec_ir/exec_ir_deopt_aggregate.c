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
