#include "zr_vm_parser/aot_ir_projection_descriptor.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void descriptor_clear(SZrAotIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
}

void ZrParser_AotIrProjection_FreeDescriptor(
        SZrAotIrProjectionDescriptor *descriptor) {
    if (descriptor == ZR_NULL) return;
    free(descriptor->instructions);
    free(descriptor->blocks);
    free((void *)descriptor->function.successorPool);
    free(descriptor->phiIncoming);
    free(descriptor->frameSlots);
    free(descriptor->sourceMaps);
    free(descriptor->constants);
    free(descriptor->layouts);
    memset(descriptor, 0, sizeof(*descriptor));
}

static TZrBool descriptor_alloc(void **storage, TZrUInt32 count,
                                size_t elementSize) {
    if (count == 0u) return ZR_TRUE;
    *storage = calloc((size_t)count, elementSize);
    return (TZrBool)(*storage != ZR_NULL);
}

static TZrBool descriptor_edge_range_valid(SZrExecIrRange range,
                                           TZrUInt32 count) {
    return (TZrBool)(range.start <= count &&
                     range.count <= count - range.start);
}

TZrBool ZrParser_AotIrProjection_BuildDescriptor(
        const SZrAotIrProjection *projection,
        const SZrAotIrTargetContract *target,
        const SZrExecutionContract *moduleContract,
        SZrAotIrProjectionDescriptor *descriptor,
        SZrAotIrDiagnostic *diagnostic) {
    SZrAotIrProjectionDescriptor candidate;
    TZrUInt32 *edgePool = ZR_NULL;
    TZrUInt32 i, edgeCount;
    descriptor_clear(diagnostic);
    if (descriptor == ZR_NULL || projection == ZR_NULL || target == ZR_NULL ||
        moduleContract == ZR_NULL ||
        projection->bindingRowsSchemaVersion !=
                ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY ||
        projection->ownershipTag != ZR_EXEC_IR_PROJECTION_TAG ||
        projection->functionId == ZR_AOT_IR_ID_INVALID ||
        projection->instructions == ZR_NULL || projection->instructionCount == 0u ||
        projection->blocks == ZR_NULL || projection->blockCount == 0u ||
        (projection->constantCount != 0u && projection->constants == ZR_NULL) ||
        (projection->layoutCount != 0u && projection->layouts == ZR_NULL) ||
        projection->functionToken == 0u || projection->signatureHash == 0u ||
        projection->frameLayoutHash == 0u) {
        if (diagnostic != ZR_NULL) diagnostic->status = ZR_AOT_IR_INVALID_ARGUMENT;
        return ZR_FALSE;
    }
    if (projection->successorCount > UINT32_MAX - projection->predecessorCount ||
        (projection->successorCount != 0u && projection->successors == ZR_NULL) ||
        (projection->predecessorCount != 0u && projection->predecessors == ZR_NULL)) {
        if (diagnostic != ZR_NULL) diagnostic->status = ZR_AOT_IR_INVALID_RANGE;
        return ZR_FALSE;
    }
    for (i = 0u; i < projection->instructionCount; ++i) {
        if (!descriptor_edge_range_valid(
                projection->instructions[i].successorRange,
                projection->successorCount)) {
            if (diagnostic != ZR_NULL) diagnostic->status = ZR_AOT_IR_INVALID_RANGE;
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < projection->blockCount; ++i) {
        if (!descriptor_edge_range_valid(
                projection->blocks[i].successors,
                projection->successorCount) ||
            !descriptor_edge_range_valid(
                projection->blocks[i].predecessors,
                projection->predecessorCount)) {
            if (diagnostic != ZR_NULL) diagnostic->status = ZR_AOT_IR_INVALID_RANGE;
            return ZR_FALSE;
        }
    }
    edgeCount = projection->successorCount + projection->predecessorCount;
    memset(&candidate, 0, sizeof(candidate));
    candidate.owner = projection;
    candidate.function.id = projection->functionId;
    candidate.function.functionToken = projection->functionToken;
    candidate.function.contract = projection->contract;
    candidate.function.signatureHash = projection->signatureHash;
    candidate.function.callableAbi = projection->callableAbi;
    candidate.function.frameLayout.logicalSlotCount = projection->logicalSlotCount;
    candidate.function.frameLayout.storageSlotCount = projection->storageSlotCount;
    candidate.function.frameLayout.parameterPrefixBytes = projection->parameterPrefixBytes;
    candidate.function.frameLayout.returnAreaOffset = projection->returnAreaOffset;
    candidate.function.frameLayout.frameByteSize = projection->frameByteSize;
    candidate.function.frameLayout.frameByteAlign = projection->frameByteAlign;
    candidate.function.frameLayout.layoutHash = projection->frameLayoutHash;
    candidate.function.blockCount = projection->blockCount;
    candidate.function.instructionCount = projection->instructionCount;
    candidate.function.operandPool = projection->operands;
    candidate.function.operandCount = projection->operandCount;
    candidate.function.resultPool = projection->results;
    candidate.function.resultCount = projection->resultCount;
    candidate.function.successorCount = edgeCount;
    candidate.function.memoryTokenPool = projection->memoryTokens;
    candidate.function.memoryTokenCount = projection->memoryTokenCount;
    candidate.function.valueSlotPool = projection->valueSlots;
    candidate.function.valueSlotCount = projection->valueSlotCount;
    candidate.function.logicalStateMap = projection->stateMapPresent
            ? &projection->stateMap : ZR_NULL;
    candidate.function.gcMapHash = projection->gcMapCount;
    candidate.function.exceptionMapHash = projection->deoptStateCount;
    candidate.function.debugMapHash = projection->sourceMapCount;
    if (!descriptor_alloc((void **)&edgePool, edgeCount, sizeof(*edgePool))) {
        if (diagnostic != ZR_NULL) diagnostic->status = ZR_AOT_IR_INVALID_ARGUMENT;
        return ZR_FALSE;
    }
    /* A built descriptor owns this in-memory AOTIR edge view. */
    candidate.function.successorPool = edgePool;
    if (!descriptor_alloc((void **)&candidate.instructions,
                          projection->instructionCount,
                          sizeof(*candidate.instructions)) ||
        !descriptor_alloc((void **)&candidate.blocks, projection->blockCount,
                          sizeof(*candidate.blocks)) ||
        !descriptor_alloc((void **)&candidate.phiIncoming,
                          projection->phiIncomingCount,
                          sizeof(*candidate.phiIncoming)) ||
        !descriptor_alloc((void **)&candidate.frameSlots,
                          projection->frameSlotCount,
                          sizeof(*candidate.frameSlots)) ||
        !descriptor_alloc((void **)&candidate.sourceMaps,
                          projection->sourceMapCount,
                          sizeof(*candidate.sourceMaps)) ||
        !descriptor_alloc((void **)&candidate.constants,
                          projection->constantCount,
                          sizeof(*candidate.constants)) ||
        !descriptor_alloc((void **)&candidate.layouts,
                          projection->layoutCount,
                          sizeof(*candidate.layouts))) {
        ZrParser_AotIrProjection_FreeDescriptor(&candidate);
        if (diagnostic != ZR_NULL) diagnostic->status = ZR_AOT_IR_INVALID_ARGUMENT;
        return ZR_FALSE;
    }
    if (projection->successorCount != 0u) {
        memcpy(edgePool, projection->successors,
               (size_t)projection->successorCount * sizeof(*edgePool));
    }
    if (projection->predecessorCount != 0u) {
        memcpy(edgePool + projection->successorCount,
               projection->predecessors,
               (size_t)projection->predecessorCount * sizeof(*edgePool));
    }
    for (i = 0u; i < projection->instructionCount; ++i) {
        const SZrExecBcInstruction *source = &projection->instructions[i];
        SZrAotIrInstruction *destination = &candidate.instructions[i];
        destination->id = source->pc + 1u;
        destination->opcode = source->opcode;
        destination->flags = source->flags;
        destination->results.offset = source->results.offset;
        destination->results.count = source->results.count;
        destination->operands.offset = source->operands.offset;
        destination->operands.count = source->operands.count;
        destination->successors.offset = source->successorRange.offset;
        destination->successors.count = source->successorRange.count;
        destination->phiIncoming.offset = source->phiRange.offset;
        destination->phiIncoming.count = source->phiRange.count;
        destination->effectIn = source->effectIn;
        destination->effectOut = source->effectOut;
        destination->sourceId = source->sourceId;
        destination->deoptId = source->deoptId;
        destination->layoutId = source->layoutId;
        destination->bindingRow = source->bindingRow;
        destination->typeToken = source->typeToken;
        destination->matchTypeToken = source->matchTypeToken;
        destination->memoryIn.offset = source->memoryIn.offset;
        destination->memoryIn.count = source->memoryIn.count;
        destination->memoryOut.offset = source->memoryOut.offset;
        destination->memoryOut.count = source->memoryOut.count;
    }
    for (i = 0u; i < projection->blockCount; ++i) {
        const SZrExecBcBlock *source = &projection->blocks[i];
        SZrAotIrBlock *destination = &candidate.blocks[i];
        destination->id = source->id;
        destination->flags = source->flags;
        destination->instructions.offset = source->instructions.offset;
        destination->instructions.count = source->instructions.count;
        destination->predecessors.offset = source->predecessors.count != 0u
                ? projection->successorCount + source->predecessors.offset
                : 0u;
        destination->predecessors.count = source->predecessors.count;
        destination->successors.offset = source->successors.offset;
        destination->successors.count = source->successors.count;
        destination->terminatorInstructionId = source->terminatorInstructionId;
    }
    for (i = 0u; i < projection->phiIncomingCount; ++i) {
        candidate.phiIncoming[i].predecessorBlockId =
                projection->phiIncomings[i].predecessor;
        candidate.phiIncoming[i].valueId = projection->phiIncomings[i].value;
    }
    for (i = 0u; i < projection->frameSlotCount; ++i) {
        const SZrExecIrFrameSlot *source = &projection->frameSlots[i];
        candidate.frameSlots[i].slotId = source->slotId;
        candidate.frameSlots[i].byteOffset = source->byteOffset;
        candidate.frameSlots[i].byteSize = source->byteSize;
        candidate.frameSlots[i].byteAlign = source->byteAlign;
        candidate.frameSlots[i].typeToken = source->typeToken;
        candidate.frameSlots[i].kind = (TZrUInt32)source->kind;
    }
    for (i = 0u; i < projection->sourceMapCount; ++i) {
        const SZrExecIrProjectionSourceMap *source = &projection->sourceMaps[i];
        candidate.sourceMaps[i].sourceId = source->sourceId;
        candidate.sourceMaps[i].instructionId = source->pc + 1u;
        candidate.sourceMaps[i].startOffset = source->startOffset;
        candidate.sourceMaps[i].endOffset = source->endOffset;
        candidate.sourceMaps[i].startLine = source->startLine;
        candidate.sourceMaps[i].startColumn = source->startColumn;
        candidate.sourceMaps[i].endLine = source->endLine;
        candidate.sourceMaps[i].endColumn = source->endColumn;
    }
    if (projection->constantCount != 0u) {
        memcpy(candidate.constants, projection->constants,
               (size_t)projection->constantCount * sizeof(*candidate.constants));
    }
    if (projection->layoutCount != 0u) {
        memcpy(candidate.layouts, projection->layouts,
               (size_t)projection->layoutCount * sizeof(*candidate.layouts));
    }
    candidate.function.instructions = candidate.instructions;
    candidate.function.blocks = candidate.blocks;
    candidate.function.phiIncomingPool = candidate.phiIncoming;
    candidate.function.frameSlots = candidate.frameSlots;
    candidate.function.frameSlotCount = projection->frameSlotCount;
    candidate.function.sourceMaps = candidate.sourceMaps;
    candidate.function.sourceMapCount = projection->sourceMapCount;
    candidate.function.gcMap = projection->gcMapPresent
            ? &projection->gcMap : ZR_NULL;
    candidate.function.gcRootPool = projection->gcRoots;
    candidate.function.gcRootCount = projection->gcRootCount;
    candidate.function.deoptStates = projection->deoptStates;
    candidate.function.deoptStateCount = projection->deoptStateCount;
    candidate.function.deoptValuePool = projection->deoptValues;
    candidate.function.deoptValueCount = projection->deoptValueCount;
    candidate.function.deoptAggregates = projection->deoptAggregates;
    candidate.function.deoptAggregateCount = projection->deoptAggregateCount;
    candidate.function.deoptAggregateFields = projection->deoptAggregateFields;
    candidate.function.deoptAggregateFieldCount =
            projection->deoptAggregateFieldCount;
    candidate.module.schemaVersion = ZR_AOT_IR_SCHEMA_VERSION;
    candidate.module.target = *target;
    candidate.module.contract = *moduleContract;
    candidate.module.moduleHash = moduleContract->moduleHash;
    candidate.module.constantPool = candidate.constants;
    candidate.module.constantCount = projection->constantCount;
    candidate.module.layoutPool = candidate.layouts;
    candidate.module.layoutCount = projection->layoutCount;
    candidate.module.functions = &candidate.function;
    candidate.module.functionCount = 1u;
    if (ZrCore_AotIr_ValidateModule(&candidate.module, diagnostic) != ZR_AOT_IR_OK) {
        ZrParser_AotIrProjection_FreeDescriptor(&candidate);
        return ZR_FALSE;
    }
    ZrParser_AotIrProjection_FreeDescriptor(descriptor);
    *descriptor = candidate;
    descriptor->module.functions = &descriptor->function;
    return ZR_TRUE;
}
