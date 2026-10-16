#include "exec_ir/source/exec_ir_source_frame_internal.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/canonical_type.h"

#include <stdlib.h>
#include <string.h>

TZrBool source_frame_fail(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic,
                          EZrExecutionDiagnosticCode code, const SZrExecIrValue *value) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        diagnostic->functionToken = function != ZR_NULL ? function->functionToken : 0u;
        if (value != ZR_NULL) {
            diagnostic->actualVersion = value->id;
            diagnostic->instructionId = value->definition;
            if (value->definition != 0u && value->definition <= function->instructionCount)
                diagnostic->sourceId = function->instructions[value->definition - 1u].sourceId;
        }
    }
    return ZR_FALSE;
}

TZrBool source_frame_count_fits(size_t count, size_t width) {
    return (TZrBool) (width != 0u && count <= SIZE_MAX / width);
}

/* Capacity spans establish arithmetic and independence, not readability.
 * Readable allocations remain the caller's explicit precondition. */
static TZrBool source_frame_span(SZrSourceFrameStorage *storage, const void *pointer, size_t capacity, size_t width) {
    uintptr_t start = (uintptr_t) pointer, end;
    size_t bytes, i;
    if (!source_frame_count_fits(capacity, width)) {
        storage->error = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
        return ZR_FALSE;
    }
    if (pointer == ZR_NULL) {
        if (capacity == 0u)
            return ZR_TRUE;
        storage->error = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
        return ZR_FALSE;
    }
    if (capacity == 0u) {
        storage->error = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
        return ZR_FALSE;
    }
    bytes = capacity * width;
    if ((uintmax_t) bytes > (uintmax_t) UINTPTR_MAX || start > UINTPTR_MAX - (uintptr_t) bytes) {
        storage->error = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
        return ZR_FALSE;
    }
    end = start + (uintptr_t) bytes;
    for (i = 0u; i < storage->count; ++i) {
        if (start < storage->spans[i].end && storage->spans[i].start < end) {
            storage->error = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
            return ZR_FALSE;
        }
    }
    if (storage->count == SOURCE_FRAME_SPAN_LIMIT) {
        storage->error = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
        return ZR_FALSE;
    }
    storage->spans[storage->count].start = start;
    storage->spans[storage->count++].end = end;
    return ZR_TRUE;
}

static TZrBool source_frame_pool(SZrSourceFrameStorage *storage, const void *pointer, size_t count, size_t capacity,
                                 size_t width) {
    if (count > capacity) {
        storage->error = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
        return ZR_FALSE;
    }
    return source_frame_span(storage, pointer, capacity, width);
}

TZrBool source_frame_storage(const SZrExecIrFunction *function, const SZrSemanticContext *context,
                             const SZrExecIrLayout *layouts, TZrUInt32 layoutCount, SZrExecIrDiagnostic *diagnostic,
                             SZrSourceFrameStorage *storage) {
#define POOL(owner, field, count, capacity)                                                                            \
    do {                                                                                                               \
        if (!source_frame_pool(storage, (owner)->field, (owner)->count, (owner)->capacity, sizeof(*(owner)->field)))   \
            return ZR_FALSE;                                                                                           \
    } while (0)
    if (!source_frame_span(storage, function, 1u, sizeof(*function)) ||
        !source_frame_span(storage, context, 1u, sizeof(*context)) ||
        (layoutCount != 0u && !source_frame_span(storage, layouts, layoutCount, sizeof(*layouts))) ||
        (diagnostic != ZR_NULL && !source_frame_span(storage, diagnostic, 1u, sizeof(*diagnostic))))
        return ZR_FALSE;
    if (!context->canonicalTypes.isValid || context->canonicalTypes.elementSize != sizeof(SZrCanonicalTypeNode)) {
        storage->error = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
        return ZR_FALSE;
    }
    /* Register borrowed canonical allocation before dereferencing owners. */
    if (!source_frame_pool(storage, context->canonicalTypes.head, context->canonicalTypes.length,
                           context->canonicalTypes.capacity, sizeof(SZrCanonicalTypeNode)))
        return ZR_FALSE;
    POOL(function, values, valueCount, valueCapacity);
    POOL(function, instructions, instructionCount, instructionCapacity);
    POOL(function, blocks, blockCount, blockCapacity);
    POOL(function, operandPool, operandCount, operandCapacity);
    POOL(function, resultPool, resultCount, resultCapacity);
    POOL(function, memoryTokenPool, memoryTokenCount, memoryTokenCapacity);
    POOL(function, phiPool, phiCount, phiCapacity);
    POOL(function, phiIncoming, phiIncomingCount, phiIncomingCapacity);
    POOL(function, predecessors, predecessorCount, predecessorCapacity);
    POOL(function, successors, successorCount, successorCapacity);
    POOL(function, gcRoots, gcRootCount, gcRootCapacity);
    POOL(function, deoptStates, deoptStateCount, deoptStateCapacity);
    POOL(function, deoptValues, deoptValueCount, deoptValueCapacity);
    POOL(function, deoptAggregates, deoptAggregateCount, deoptAggregateCapacity);
    POOL(function, deoptAggregateFields, deoptAggregateFieldCount, deoptAggregateFieldCapacity);
    POOL(function, sourceMaps, sourceMapCount, sourceMapCapacity);
    POOL(function, bindingRows, bindingRowCount, bindingRowCapacity);
    /* Register every owner object before following any nested pointer. */
#define OWNER(field)                                                                                                   \
    do {                                                                                                               \
        if (function->field != ZR_NULL && !source_frame_span(storage, function->field, 1u, sizeof(*function->field)))  \
            return ZR_FALSE;                                                                                           \
    } while (0)
    OWNER(frameLayout);
    OWNER(gcMap);
    OWNER(stateMap);
#undef OWNER
    if (function->gcMapCount > function->gcMapCapacity || (function->gcMapCount != 0u && function->gcMap == ZR_NULL)) {
        storage->error = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
        return ZR_FALSE;
    }
    if (function->frameLayout != ZR_NULL)
        POOL(function->frameLayout, slots, slotCount, slotCapacity);
    if (function->gcMap != ZR_NULL) {
        POOL(function->gcMap, entries, entryCount, entryCapacity);
        POOL(function->gcMap, slotIndexPool, slotIndexCount, slotIndexCapacity);
        POOL(function->gcMap, inlineRefOffsetPool, inlineRefOffsetCount, inlineRefOffsetCapacity);
    }
    if (function->stateMap != ZR_NULL) {
        POOL(function->stateMap, entries, entryCount, entryCapacity);
        POOL(function->stateMap, valuePool, valueCount, valueCapacity);
        POOL(function->stateMap, rootPool, rootCount, rootCapacity);
        POOL(function->stateMap, ownerStatePool, ownerStateCount, ownerStateCapacity);
    }
#undef POOL
    return ZR_TRUE;
}

TZrBool source_frame_body_metadata(const SZrExecIrFunction *function) {
    TZrUInt32 i;
    if (function->gcMap != ZR_NULL || function->gcMapCount != 0u || function->gcRootCount != 0u ||
        function->memoryTokenCount != 0u || function->phiCount != 0u || function->phiIncomingCount != 0u ||
        function->deoptStateCount != 0u || function->deoptValueCount != 0u || function->deoptAggregateCount != 0u ||
        function->deoptAggregateFieldCount != 0u || function->bindingRowCount != 0u ||
        function->bindingRowsSchemaVersion != ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY ||
        function->instructionCount == 0u || function->valueCount == 0u)
        return ZR_FALSE;
    if (function->stateMap != ZR_NULL &&
        (function->stateMap->entryCount != 0u || function->stateMap->valueCount != 0u ||
         function->stateMap->rootCount != 0u || function->stateMap->ownerStateCount != 0u))
        return ZR_FALSE;
    for (i = 0u; i < function->blockCount; ++i)
        if ((function->blocks[i].flags & (ZR_EXEC_IR_BLOCK_FLAG_CLEANUP | ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION)) != 0u)
            return ZR_FALSE;
    for (i = 0u; i < function->instructionCount; ++i) {
        const SZrExecIrInstruction *instruction = &function->instructions[i];
        if ((instruction->opcode != ZR_EXEC_IR_OPCODE_CONSTANT && instruction->opcode != ZR_EXEC_IR_OPCODE_NOP &&
             instruction->opcode != ZR_EXEC_IR_OPCODE_RETURN) ||
            instruction->flags != 0u || instruction->matchTypeToken != 0u ||
            (instruction->opcode != ZR_EXEC_IR_OPCODE_CONSTANT && instruction->layoutId != 0u) ||
            instruction->memoryIn.count != 0u || instruction->memoryOut.count != 0u ||
            instruction->effectIn != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ||
            instruction->effectOut != ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID || instruction->deoptId != 0u ||
            instruction->bindingRow != 0u)
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

static const SZrExecIrLayout *source_frame_row(const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
                                               TZrExecIrTypeToken typeToken) {
    TZrUInt32 i;
    for (i = 0u; i < layoutCount; ++i)
        if (layouts[i].typeToken == typeToken)
            return &layouts[i];
    return ZR_NULL;
}

TZrBool source_frame_rows(const SZrExecIrLayout *layouts, TZrUInt32 count) {
    TZrUInt32 i, j;
    for (i = 0u; i < count; ++i) {
        const SZrExecIrLayout *row = &layouts[i];
        if (row->id == 0u || row->typeToken == 0u || row->layoutHash == 0u || row->byteSize == 0u ||
            row->byteAlign == 0u || (row->byteAlign & (row->byteAlign - 1u)) != 0u)
            return ZR_FALSE;
        for (j = 0u; j < i; ++j)
            if (layouts[j].id == row->id || layouts[j].typeToken == row->typeToken)
                return ZR_FALSE;
    }
    return ZR_TRUE;
}

/* Prove actual logical IDs, physical bijection and row geometry. Core alone
 * does not establish frame offsets, geometry hash or nonoverlap. */
TZrBool source_frame_geometry(const SZrExecIrFunction *function, const SZrExecIrPackedValue *values,
                              const SZrExecIrPackedFrameLayout *packed, TZrUInt32 frameByteLimit) {
    const SZrExecIrFrameLayout *frame = &packed->frame;
    TZrUInt32 i, j;
    if (frame->slotCount != function->valueCount || frame->logicalSlotCount != function->valueCount ||
        frame->storageSlotCount != function->valueCount || frame->slotCapacity < frame->slotCount ||
        frame->slots == ZR_NULL || packed->logicalValueIds == ZR_NULL || packed->logicalToPhysical == ZR_NULL ||
        packed->slotClasses == ZR_NULL || frame->parameterPrefixCount != 0u || frame->parameterCount != 0u ||
        frame->localCount != function->valueCount || frame->layoutHash == 0u || frame->frameByteAlign == 0u ||
        (frame->frameByteAlign & (frame->frameByteAlign - 1u)) != 0u ||
        frame->frameByteSize % frame->frameByteAlign != 0u || frame->returnBufferOffset != frame->frameByteSize ||
        (frameByteLimit != 0u && frame->frameByteSize > frameByteLimit))
        return ZR_FALSE;
    for (i = 0u; i < function->valueCount; ++i) {
        const SZrExecIrPackedValue *value = &values[i];
        const SZrExecIrFrameSlot *slot;
        TZrUInt32 physical = packed->logicalToPhysical[i], end;
        if (packed->logicalValueIds[i] != function->values[i].id || physical >= frame->slotCount ||
            packed->slotClasses[i] != ZR_EXEC_IR_PACKED_SLOT_SCALAR)
            return ZR_FALSE;
        slot = &frame->slots[physical];
        if (slot->slotId != value->valueId || slot->typeToken != value->typeToken ||
            slot->kind != ZR_EXEC_IR_FRAME_SLOT_VALUE || slot->byteSize != value->byteSize ||
            slot->byteAlign != value->byteAlign || slot->byteAlign > frame->frameByteAlign ||
            slot->byteOffset % slot->byteAlign != 0u || slot->byteSize > UINT32_MAX - slot->byteOffset)
            return ZR_FALSE;
        end = slot->byteOffset + slot->byteSize;
        if (end > frame->frameByteSize || end > frame->returnBufferOffset)
            return ZR_FALSE;
        for (j = 0u; j < i; ++j) {
            const SZrExecIrFrameSlot *prior = &frame->slots[packed->logicalToPhysical[j]];
            if (packed->logicalToPhysical[j] == physical ||
                (slot->byteOffset < prior->byteOffset + prior->byteSize && prior->byteOffset < end))
                return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool source_frame_admit_values(const SZrExecIrFunction *function, const SZrSemanticContext *context,
                                  const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
                                  SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 i;
    for (i = 0u; i < function->valueCount; ++i) {
        const SZrExecIrValue *value = &function->values[i];
        const SZrCanonicalTypeNode *node = ZrParser_CanonicalType_Find(context, value->typeToken);
        if (value->flags != 0u || value->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            value->nullability != ZR_EXEC_IR_NULLABILITY_NONNULL ||
            value->definition == ZR_EXEC_IR_INSTRUCTION_ID_INVALID || value->definition > function->instructionCount ||
            function->instructions[value->definition - 1u].typeToken != value->typeToken || node == ZR_NULL ||
            node->id != value->typeToken || node->kind != ZR_CANONICAL_TYPE_PRIMITIVE || node->structuralHash == 0u ||
            node->data.primitive.valueType != ZR_VALUE_TYPE_INT64 ||
            source_frame_row(layouts, layoutCount, value->typeToken) == ZR_NULL)
            return source_frame_fail(function, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, value);
    }
    return ZR_TRUE;
}

TZrBool source_frame_prepare_request(const SZrExecIrFunction *function, const SZrExecIrLayout *layouts,
                                     TZrUInt32 layoutCount, TZrUInt32 frameByteLimit, SZrExecIrPackedValue **valuesOut,
                                     SZrExecIrPackedFrameRequest *requestOut, SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrPackedValue *values;
    TZrUInt32 i;
    values = (SZrExecIrPackedValue *) calloc(function->valueCount, sizeof(*values));
    if (values == ZR_NULL)
        return source_frame_fail(function, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, ZR_NULL);
    for (i = 0u; i < function->valueCount; ++i) {
        const SZrExecIrValue *value = &function->values[i];
        const SZrExecIrLayout *row = source_frame_row(layouts, layoutCount, value->typeToken);
        values[i].valueId = value->id;
        values[i].typeToken = value->typeToken;
        values[i].slotClass = ZR_EXEC_IR_PACKED_SLOT_SCALAR;
        values[i].byteSize = row->byteSize;
        values[i].byteAlign = row->byteAlign;
        values[i].liveEnd = function->instructionCount;
    }
    memset(requestOut, 0, sizeof(*requestOut));
    requestOut->functionToken = function->functionToken;
    requestOut->values = values;
    requestOut->valueCount = function->valueCount;
    requestOut->frameByteLimit = frameByteLimit;
    *valuesOut = values;
    return ZR_TRUE;
}

static TZrBool source_frame_mismatch(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic,
                                     TZrUInt32 expected, TZrUInt32 actual) {
    source_frame_fail(function, diagnostic, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, ZR_NULL);
    if (diagnostic != ZR_NULL) {
        diagnostic->expectedVersion = expected;
        diagnostic->actualVersion = actual;
    }
    return ZR_FALSE;
}

static TZrBool source_frame_hash_mismatch(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic,
                                          TZrUInt64 expected, TZrUInt64 actual) {
    source_frame_fail(function, diagnostic, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, ZR_NULL);
    if (diagnostic != ZR_NULL) {
        diagnostic->expectedHash = expected;
        diagnostic->actualHash = actual;
    }
    return ZR_FALSE;
}

/* Capacity, spare storage and padding are deliberately not equality fields. */
static TZrBool source_frame_compare(const SZrExecIrFunction *function, const SZrExecIrFrameLayout *expected,
                                    SZrExecIrDiagnostic *diagnostic) {
    const SZrExecIrFrameLayout *actual = function->frameLayout;
    TZrUInt32 i;
#define FIELD(field)                                                                                                   \
    do {                                                                                                               \
        if (expected->field != actual->field)                                                                          \
            return source_frame_mismatch(function, diagnostic, expected->field, actual->field);                        \
    } while (0)
    FIELD(storageSlotCount);
    FIELD(logicalSlotCount);
    FIELD(parameterPrefixCount);
    FIELD(returnBufferOffset);
    FIELD(frameByteSize);
    FIELD(frameByteAlign);
    FIELD(parameterCount);
    FIELD(localCount);
    FIELD(slotCount);
#undef FIELD
    if (expected->layoutHash != actual->layoutHash)
        return source_frame_hash_mismatch(function, diagnostic, expected->layoutHash, actual->layoutHash);
    if (expected->layoutHash != function->contract.layoutHash)
        return source_frame_hash_mismatch(function, diagnostic, expected->layoutHash, function->contract.layoutHash);
    for (i = 0u; i < expected->slotCount; ++i) {
        const SZrExecIrFrameSlot *expectedSlot = &expected->slots[i];
        const SZrExecIrFrameSlot *actualSlot = &actual->slots[i];
#define SLOT(field)                                                                                                    \
    do {                                                                                                               \
        if (expectedSlot->field != actualSlot->field)                                                                  \
            return source_frame_mismatch(function, diagnostic, expectedSlot->field, actualSlot->field);                \
    } while (0)
        SLOT(slotId);
        SLOT(byteOffset);
        SLOT(byteSize);
        SLOT(byteAlign);
        SLOT(typeToken);
        SLOT(kind);
#undef SLOT
    }
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_ValidatePrimitiveSourceFrame(const SZrExecIrFunction *function,
                                                     const SZrSemanticContext *context, const SZrExecIrLayout *layouts,
                                                     TZrUInt32 layoutCount, TZrUInt32 frameByteLimit,
                                                     SZrExecIrDiagnostic *diagnostic) {
    SZrSourceFrameStorage storage = {0};
    SZrExecIrPackedFrameRequest request = {0};
    SZrExecIrPackedFrameLayout packed;
    SZrExecIrPackedFrameLayout view;
    SZrExecIrPackedValue *values = ZR_NULL;
    TZrTypeId previous = ZR_SEMANTIC_ID_INVALID;
    TZrSize i;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || context == ZR_NULL || (layoutCount != 0u && layouts == ZR_NULL))
        return source_frame_fail(ZR_NULL, diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, ZR_NULL);
    if (!source_frame_storage(function, context, layouts, layoutCount, diagnostic, &storage))
        return source_frame_fail(function, diagnostic, storage.error, ZR_NULL);
    if (!source_frame_count_fits(function->valueCount, sizeof(SZrExecIrPackedValue)) ||
        !source_frame_count_fits(function->valueCount, sizeof(TZrUInt32)) ||
        !source_frame_count_fits(function->valueCount, sizeof(TZrExecIrValueId)) ||
        !source_frame_count_fits(function->valueCount, sizeof(SZrExecIrFrameSlot)) ||
        !source_frame_count_fits(function->valueCount, sizeof(EZrExecIrPackedSlotClass)))
        return source_frame_fail(function, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, ZR_NULL);
    if (diagnostic != ZR_NULL)
        memset(diagnostic, 0, sizeof(*diagnostic));
    if (!ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, diagnostic))
        return ZR_FALSE;
    if (!source_frame_body_metadata(function) || !source_frame_rows(layouts, layoutCount))
        return source_frame_fail(function, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, ZR_NULL);
    for (i = 0u; i < context->canonicalTypes.length; ++i) {
        const SZrCanonicalTypeNode *node =
                (const SZrCanonicalTypeNode *) ZrCore_Array_Get((SZrArray *) &context->canonicalTypes, i);
        if (node->id == ZR_SEMANTIC_ID_INVALID || node->id <= previous)
            return source_frame_fail(function, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, ZR_NULL);
        previous = node->id;
    }
    if (!source_frame_admit_values(function, context, layouts, layoutCount, diagnostic))
        return ZR_FALSE;
    if (function->frameLayout == ZR_NULL)
        return source_frame_mismatch(function, diagnostic, 1u, 0u);
    if (!source_frame_prepare_request(function, layouts, layoutCount, frameByteLimit, &values, &request, diagnostic))
        return ZR_FALSE;
    ZrParser_ExecIr_PackedFrameLayoutInit(&packed);
    if (!ZrParser_ExecIr_LayoutPackedFrame(&request, &packed, diagnostic))
        goto cleanup;
    if (!source_frame_compare(function, &packed.frame, diagnostic))
        goto cleanup;
    /* Only packed owns regenerated storage; view borrows every reachable array. */
    view = packed;
    view.frame = *function->frameLayout;
    if (!source_frame_geometry(function, values, &view, frameByteLimit)) {
        source_frame_fail(function, diagnostic, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH, ZR_NULL);
        goto cleanup;
    }
    success = ZR_TRUE;
    if (diagnostic != ZR_NULL)
        memset(diagnostic, 0, sizeof(*diagnostic));
cleanup:
    ZrParser_ExecIr_PackedFrameLayoutFree(&packed);
    free(values);
    return success;
}
