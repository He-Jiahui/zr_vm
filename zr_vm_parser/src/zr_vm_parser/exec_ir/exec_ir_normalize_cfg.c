#include "exec_ir_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void normalize_diagnostic(SZrExecIrDiagnostic *diagnostic,
                                 const SZrSemanticIrFunction *semantic,
                                 EZrExecutionDiagnosticCode code) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = code;
    diagnostic->functionToken = semantic != ZR_NULL
            ? (TZrMetadataToken)semantic->symbolId
            : 0u;
}

static TZrBool semantic_call_can_invoke(EZrSemanticIrOpcode opcode) {
    return (TZrBool)(opcode == ZR_SEMANTIC_IR_CALL_TYPED ||
                    opcode == ZR_SEMANTIC_IR_CALL_VIRTUAL ||
                    opcode == ZR_SEMANTIC_IR_CALL_DYNAMIC ||
                    opcode == ZR_SEMANTIC_IR_CALL_META);
}

static TZrUInt32 block_edge_count(const SZrParserCfgBlock *block) {
    return block->outgoingEdges.isValid
            ? (TZrUInt32)block->outgoingEdges.length
            : block->successorCount;
}

static const SZrParserCfgEdge *block_edge_at(
        const SZrParserCfgBlock *block,
        TZrUInt32 index) {
    return block->outgoingEdges.isValid
            ? (const SZrParserCfgEdge *)ZrCore_Array_Get(
                      (SZrArray *)&block->outgoingEdges, index)
            : ZR_NULL;
}

static TZrBool block_has_typed_invoke_edges(
        const SZrSemanticIrFunction *semantic,
        const SZrParserCfgBlock *block) {
    const SZrParserCfgEdge *normal;
    const SZrParserCfgEdge *exception;
    const SZrSemanticIrInstruction *tail;

    if (!block->outgoingEdges.isValid ||
        block->outgoingEdges.length != 2u ||
        block->instructionCount == 0u) {
        return ZR_FALSE;
    }
    normal = block_edge_at(block, 0u);
    exception = block_edge_at(block, 1u);
    tail = (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
            (SZrArray *)&semantic->instructions,
            block->firstInstructionIndex + block->instructionCount - 1u);
    return (TZrBool)(normal->kind == ZR_PARSER_CFG_EDGE_NORMAL &&
                    exception->kind == ZR_PARSER_CFG_EDGE_EXCEPTION &&
                    normal->toBlockId != exception->toBlockId &&
                    semantic_call_can_invoke(tail->opcode));
}

static TZrUInt32 block_prior_invoke_count(
        const SZrSemanticIrFunction *semantic,
        const SZrParserCfgBlock *block) {
    TZrUInt32 count = 0u;
    TZrUInt32 index;

    if (!block_has_typed_invoke_edges(semantic, block)) {
        return 0u;
    }
    for (index = 0u; index + 1u < block->instructionCount; ++index) {
        const SZrSemanticIrInstruction *instruction =
                (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->instructions,
                        block->firstInstructionIndex + index);
        if (semantic_call_can_invoke(instruction->opcode)) {
            ++count;
        }
    }
    return count;
}

static void init_edge_array(SZrArray *array,
                            SZrParserCfgEdge *edges,
                            TZrUInt32 count) {
    memset(array, 0, sizeof(*array));
    array->head = (TZrBytePtr)edges;
    array->length = count;
    array->capacity = count;
    array->elementSize = sizeof(*edges);
    array->isValid = ZR_TRUE;
}

static void init_block_array(SZrArray *array,
                             SZrParserCfgBlock *blocks,
                             TZrUInt32 count) {
    memset(array, 0, sizeof(*array));
    array->head = (TZrBytePtr)blocks;
    array->length = count;
    array->capacity = count;
    array->elementSize = sizeof(*blocks);
    array->isValid = ZR_TRUE;
}

static void init_split_block(SZrParserCfgBlock *destination,
                             const SZrParserCfgBlock *source,
                             TZrUInt32 id,
                             TZrUInt32 firstInstructionIndex,
                             TZrUInt32 instructionCount) {
    *destination = *source;
    destination->id = id;
    destination->firstInstructionIndex = firstInstructionIndex;
    destination->instructionCount = instructionCount;
    destination->predecessorCount = 0u;
    destination->successorCount = 0u;
}

TZrBool zr_parser_exec_ir_normalize_exception_cfg(
        const SZrSemanticIrFunction *semantic,
        SZrParserExecIrNormalizedCfg *normalized,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 oldBlockCount;
    TZrUInt32 splitCount = 0u;
    TZrUInt32 edgeCount = 0u;
    TZrUInt32 *firstBlocks = ZR_NULL;
    TZrUInt32 newBlockCount;
    TZrUInt32 edgeCursor = 0u;
    TZrUInt32 blockIndex;

    if (normalized == ZR_NULL || semantic == ZR_NULL) {
        normalize_diagnostic(diagnostic, semantic,
                             ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
        return ZR_FALSE;
    }
    memset(normalized, 0, sizeof(*normalized));
    normalized->semantic = *semantic;
    oldBlockCount = (TZrUInt32)semantic->cfg.blocks.length;

    for (blockIndex = 0u; blockIndex < oldBlockCount; ++blockIndex) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->cfg.blocks, blockIndex);
        TZrUInt32 additions = block_prior_invoke_count(semantic, block);
        TZrUInt32 originalEdges = block_edge_count(block);
        if (additions > UINT32_MAX / 2u ||
            splitCount > UINT32_MAX - additions ||
            edgeCount > UINT32_MAX - originalEdges ||
            edgeCount + originalEdges > UINT32_MAX - additions * 2u) {
            normalize_diagnostic(diagnostic, semantic,
                                 ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
            return ZR_FALSE;
        }
        splitCount += additions;
        edgeCount += originalEdges + additions * 2u;
    }
    if (splitCount == 0u) {
        return ZR_TRUE;
    }
    if (oldBlockCount > UINT32_MAX - splitCount) {
        normalize_diagnostic(diagnostic, semantic,
                             ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
        return ZR_FALSE;
    }
    newBlockCount = oldBlockCount + splitCount;
#if SIZE_MAX <= UINT32_MAX
    if (newBlockCount > SIZE_MAX / sizeof(*normalized->blocks) ||
        edgeCount > SIZE_MAX / sizeof(*normalized->edges) ||
        oldBlockCount > SIZE_MAX / sizeof(*firstBlocks)) {
        normalize_diagnostic(diagnostic, semantic,
                             ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
        return ZR_FALSE;
    }
#endif
    firstBlocks = (TZrUInt32 *)calloc(oldBlockCount, sizeof(*firstBlocks));
    normalized->blocks = (SZrParserCfgBlock *)calloc(
            newBlockCount, sizeof(*normalized->blocks));
    normalized->edges = edgeCount != 0u
            ? (SZrParserCfgEdge *)calloc(edgeCount, sizeof(*normalized->edges))
            : ZR_NULL;
    if (firstBlocks == ZR_NULL || normalized->blocks == ZR_NULL ||
        (edgeCount != 0u && normalized->edges == ZR_NULL)) {
        normalize_diagnostic(diagnostic, semantic,
                             ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
        goto fail;
    }

    newBlockCount = 0u;
    for (blockIndex = 0u; blockIndex < oldBlockCount; ++blockIndex) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->cfg.blocks, blockIndex);
        firstBlocks[blockIndex] = newBlockCount;
        newBlockCount += block_prior_invoke_count(semantic, block) + 1u;
    }

    for (blockIndex = 0u; blockIndex < oldBlockCount; ++blockIndex) {
        const SZrParserCfgBlock *source =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->cfg.blocks, blockIndex);
        const SZrParserCfgEdge *exception =
                block_has_typed_invoke_edges(semantic, source)
                        ? block_edge_at(source, 1u)
                        : ZR_NULL;
        TZrUInt32 destinationId = firstBlocks[blockIndex];
        TZrUInt32 segmentStart = source->firstInstructionIndex;
        TZrUInt32 instructionIndex;

        for (instructionIndex = 0u;
             instructionIndex + 1u < source->instructionCount;
             ++instructionIndex) {
            TZrUInt32 absoluteIndex =
                    source->firstInstructionIndex + instructionIndex;
            const SZrSemanticIrInstruction *instruction =
                    (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                            (SZrArray *)&semantic->instructions, absoluteIndex);
            SZrParserCfgBlock *destination;
            SZrParserCfgEdge *edges;
            if (!semantic_call_can_invoke(instruction->opcode)) {
                continue;
            }
            destination = &normalized->blocks[destinationId];
            init_split_block(destination, source, destinationId, segmentStart,
                             absoluteIndex - segmentStart + 1u);
            destination->terminatorKind = ZR_PARSER_CFG_TERMINATOR_NONE;
            destination->isTerminator = ZR_TRUE;
            edges = &normalized->edges[edgeCursor];
            edges[0].fromBlockId = destinationId;
            edges[0].toBlockId = destinationId + 1u;
            edges[0].kind = ZR_PARSER_CFG_EDGE_NORMAL;
            edges[0].sourceNode = block_edge_at(source, 0u)->sourceNode;
            edges[1].fromBlockId = destinationId;
            edges[1].toBlockId = firstBlocks[exception->toBlockId];
            edges[1].kind = ZR_PARSER_CFG_EDGE_EXCEPTION;
            edges[1].sourceNode = exception->sourceNode;
            init_edge_array(&destination->outgoingEdges, edges, 2u);
            edgeCursor += 2u;
            segmentStart = absoluteIndex + 1u;
            ++destinationId;
        }

        {
            SZrParserCfgBlock *destination = &normalized->blocks[destinationId];
            TZrUInt32 sourceEdgeCount = block_edge_count(source);
            TZrUInt32 edgeIndex;
            init_split_block(
                    destination, source, destinationId, segmentStart,
                    source->firstInstructionIndex + source->instructionCount -
                            segmentStart);
            if (source->outgoingEdges.isValid) {
                SZrParserCfgEdge *edges = sourceEdgeCount != 0u
                        ? &normalized->edges[edgeCursor]
                        : ZR_NULL;
                for (edgeIndex = 0u; edgeIndex < sourceEdgeCount; ++edgeIndex) {
                    edges[edgeIndex] = *block_edge_at(source, edgeIndex);
                    edges[edgeIndex].fromBlockId = destinationId;
                    edges[edgeIndex].toBlockId =
                            firstBlocks[edges[edgeIndex].toBlockId];
                }
                init_edge_array(&destination->outgoingEdges, edges,
                                sourceEdgeCount);
                edgeCursor += sourceEdgeCount;
            } else {
                memset(&destination->outgoingEdges, 0,
                       sizeof(destination->outgoingEdges));
                destination->successorCount = source->successorCount;
                for (edgeIndex = 0u; edgeIndex < source->successorCount;
                     ++edgeIndex) {
                    destination->successors[edgeIndex] =
                            firstBlocks[source->successors[edgeIndex]];
                }
            }
        }
    }

    normalized->semantic.cfg = semantic->cfg;
    init_block_array(&normalized->semantic.cfg.blocks, normalized->blocks,
                     newBlockCount);
    normalized->semantic.cfg.entryBlockId =
            semantic->cfg.entryBlockId < oldBlockCount
                    ? firstBlocks[semantic->cfg.entryBlockId]
                    : semantic->cfg.entryBlockId;
    if (semantic->cfg.exitBlockId < oldBlockCount) {
        normalized->semantic.cfg.exitBlockId =
                firstBlocks[semantic->cfg.exitBlockId];
    }
    normalized->changed = ZR_TRUE;
    free(firstBlocks);
    return ZR_TRUE;

fail:
    free(firstBlocks);
    zr_parser_exec_ir_free_normalized_cfg(normalized);
    return ZR_FALSE;
}

void zr_parser_exec_ir_free_normalized_cfg(
        SZrParserExecIrNormalizedCfg *normalized) {
    if (normalized == ZR_NULL) {
        return;
    }
    free(normalized->blocks);
    free(normalized->edges);
    memset(normalized, 0, sizeof(*normalized));
}
