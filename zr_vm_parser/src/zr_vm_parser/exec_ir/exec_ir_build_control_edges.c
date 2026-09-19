#include "exec_ir_internal.h"

#include <string.h>

static void control_edge_diagnostic(
        SZrExecIrDiagnostic *diagnostic,
        const SZrExecIrFunction *output,
        TZrUInt32 blockId,
        TZrSemanticInstructionId instructionId) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
    diagnostic->functionToken = output != ZR_NULL ? output->functionToken : 0u;
    diagnostic->blockId = blockId;
    diagnostic->instructionId = instructionId;
}

static TZrBool semantic_opcode_can_invoke(EZrSemanticIrOpcode opcode) {
    return (TZrBool)(opcode == ZR_SEMANTIC_IR_CALL_TYPED ||
                    opcode == ZR_SEMANTIC_IR_CALL_VIRTUAL ||
                    opcode == ZR_SEMANTIC_IR_CALL_DYNAMIC ||
                    opcode == ZR_SEMANTIC_IR_CALL_META ||
                    opcode == ZR_SEMANTIC_IR_ITER_INIT ||
                    opcode == ZR_SEMANTIC_IR_ITER_MOVE_NEXT ||
                    opcode == ZR_SEMANTIC_IR_ITER_CURRENT);
}

static TZrBool semantic_opcode_requires_invoke_edges(
        EZrSemanticIrOpcode opcode) {
    return (TZrBool)(opcode == ZR_SEMANTIC_IR_ITER_INIT ||
                    opcode == ZR_SEMANTIC_IR_ITER_MOVE_NEXT ||
                    opcode == ZR_SEMANTIC_IR_ITER_CURRENT);
}

TZrBool zr_parser_exec_ir_has_typed_invoke_edges(
        const SZrSemanticIrFunction *semantic,
        const SZrParserCfgBlock *block) {
    const SZrParserCfgEdge *normal;
    const SZrParserCfgEdge *exception;
    const SZrSemanticIrInstruction *tail;

    if (semantic == ZR_NULL || block == ZR_NULL ||
        !block->outgoingEdges.isValid || block->outgoingEdges.length != 2u ||
        block->instructionCount == 0u) {
        return ZR_FALSE;
    }
    normal = (const SZrParserCfgEdge *)ZrCore_Array_Get(
            (SZrArray *)&block->outgoingEdges, 0u);
    exception = (const SZrParserCfgEdge *)ZrCore_Array_Get(
            (SZrArray *)&block->outgoingEdges, 1u);
    if (normal->kind != ZR_PARSER_CFG_EDGE_NORMAL ||
        exception->kind != ZR_PARSER_CFG_EDGE_EXCEPTION ||
        normal->toBlockId == exception->toBlockId) {
        return ZR_FALSE;
    }
    tail = (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
            (SZrArray *)&semantic->instructions,
            block->firstInstructionIndex + block->instructionCount - 1u);
    return semantic_opcode_can_invoke(tail->opcode);
}

static TZrSemanticInstructionId control_edge_site(
        const SZrSemanticIrFunction *semantic,
        const SZrParserCfgBlock *block) {
    const SZrSemanticIrInstruction *tail;

    if (block->instructionCount == 0u) {
        return ZR_SEMANTIC_INSTRUCTION_ID_INVALID;
    }
    tail = (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
            (SZrArray *)&semantic->instructions,
            block->firstInstructionIndex + block->instructionCount - 1u);
    return tail->id;
}

static TZrBool cleanup_edge_is_representable(
        const SZrSemanticIrFunction *semantic,
        const SZrParserCfgBlock *source,
        const SZrParserCfgBlock *destination,
        TZrUInt32 edgeCount) {
    const SZrSemanticIrInstruction *tail;

    if (edgeCount != 1u || source->instructionCount == 0u ||
        source->terminatorKind != ZR_PARSER_CFG_TERMINATOR_BRANCH ||
        (source->kind != ZR_PARSER_CFG_BLOCK_CLEANUP &&
         destination->kind != ZR_PARSER_CFG_BLOCK_CLEANUP)) {
        return ZR_FALSE;
    }
    tail = (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
            (SZrArray *)&semantic->instructions,
            source->firstInstructionIndex + source->instructionCount - 1u);
    return (TZrBool)(tail->opcode == ZR_SEMANTIC_IR_BRANCH &&
                    tail->operandCount == 0u);
}

static TZrBool cleanup_dispatch_is_representable(
        const SZrSemanticIrFunction *semantic,
        const SZrParserCfgBlock *source,
        TZrUInt32 edgeCount) {
    const SZrSemanticIrInstruction *tail;
    TZrUInt32 edgeIndex;

    if (source->kind != ZR_PARSER_CFG_BLOCK_CLEANUP ||
        source->terminatorKind != ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH ||
        !source->outgoingEdges.isValid || edgeCount < 2u ||
        source->instructionCount == 0u) {
        return ZR_FALSE;
    }
    tail = (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
            (SZrArray *)&semantic->instructions,
            source->firstInstructionIndex + source->instructionCount - 1u);
    if (tail->opcode != ZR_SEMANTIC_IR_SWITCH ||
        tail->operandCount != 1u) {
        return ZR_FALSE;
    }
    for (edgeIndex = 0u; edgeIndex < edgeCount; edgeIndex++) {
        const SZrParserCfgEdge *edge =
                (const SZrParserCfgEdge *)ZrCore_Array_Get(
                        (SZrArray *)&source->outgoingEdges, edgeIndex);
        EZrParserCfgEdgeKind expected = edgeIndex + 1u == edgeCount
                ? ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT
                : ZR_PARSER_CFG_EDGE_SWITCH_CASE;

        if (edge == ZR_NULL || edge->kind != expected) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool reject_cleanup_dispatch(
        const SZrSemanticIrFunction *semantic,
        SZrExecIrFunction *output,
        SZrExecIrDiagnostic *diagnostic,
        TZrUInt32 sourceIndex,
        const SZrParserCfgBlock *source,
        TZrUInt32 edgeCount) {
    TZrSemanticInstructionId site = control_edge_site(semantic, source);

    control_edge_diagnostic(diagnostic, output, sourceIndex + 1u, site);
    if (diagnostic != ZR_NULL) {
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        diagnostic->sourceId = site;
        if (!source->outgoingEdges.isValid) {
            diagnostic->expectedVersion = ZR_PARSER_CFG_TERMINATOR_NONE;
            diagnostic->actualVersion =
                    ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH;
        } else if (source->kind != ZR_PARSER_CFG_BLOCK_CLEANUP) {
            diagnostic->expectedVersion = ZR_PARSER_CFG_BLOCK_CLEANUP;
            diagnostic->actualVersion = (TZrUInt32)source->kind;
        } else if (edgeCount < 2u) {
            diagnostic->expectedVersion = 2u;
            diagnostic->actualVersion = edgeCount;
        } else if (source->instructionCount == 0u) {
            diagnostic->expectedVersion = 1u;
            diagnostic->actualVersion = 0u;
        } else {
            const SZrSemanticIrInstruction *tail =
                    (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                            (SZrArray *)&semantic->instructions,
                            source->firstInstructionIndex +
                                    source->instructionCount - 1u);

            if (tail->opcode != ZR_SEMANTIC_IR_SWITCH) {
                diagnostic->expectedVersion = ZR_SEMANTIC_IR_SWITCH;
                diagnostic->actualVersion = (TZrUInt32)tail->opcode;
            } else if (tail->operandCount != 1u) {
                diagnostic->expectedVersion = 1u;
                diagnostic->actualVersion = tail->operandCount;
            } else {
                TZrUInt32 edgeIndex;

                for (edgeIndex = 0u; edgeIndex < edgeCount; edgeIndex++) {
                    const SZrParserCfgEdge *edge =
                            (const SZrParserCfgEdge *)ZrCore_Array_Get(
                                    (SZrArray *)&source->outgoingEdges,
                                    edgeIndex);
                    EZrParserCfgEdgeKind expected =
                            edgeIndex + 1u == edgeCount
                            ? ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT
                            : ZR_PARSER_CFG_EDGE_SWITCH_CASE;

                    if (edge == ZR_NULL || edge->kind != expected) {
                        diagnostic->expectedVersion = (TZrUInt32)expected;
                        diagnostic->actualVersion = edge == ZR_NULL
                                ? UINT32_MAX
                                : (TZrUInt32)edge->kind;
                        break;
                    }
                }
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool reject_control_edge(
        const SZrSemanticIrFunction *semantic,
        SZrExecIrFunction *output,
        SZrExecIrDiagnostic *diagnostic,
        TZrUInt32 sourceIndex,
        const SZrParserCfgBlock *source,
        const SZrParserCfgBlock *destination,
        const SZrParserCfgEdge *edge,
        TZrUInt32 edgeCount) {
    TZrSemanticInstructionId site = control_edge_site(semantic, source);

    control_edge_diagnostic(diagnostic, output, sourceIndex + 1u, site);
    if (diagnostic != ZR_NULL) {
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        diagnostic->sourceId = site;
        if (edge->kind == ZR_PARSER_CFG_EDGE_CLEANUP) {
            if (source->kind != ZR_PARSER_CFG_BLOCK_CLEANUP &&
                destination->kind != ZR_PARSER_CFG_BLOCK_CLEANUP) {
                diagnostic->expectedVersion = ZR_PARSER_CFG_BLOCK_CLEANUP;
                diagnostic->actualVersion = (TZrUInt32)destination->kind;
            } else if (edgeCount != 1u) {
                diagnostic->expectedVersion = 1u;
                diagnostic->actualVersion = edgeCount;
            } else if (source->terminatorKind !=
                       ZR_PARSER_CFG_TERMINATOR_BRANCH) {
                diagnostic->expectedVersion =
                        ZR_PARSER_CFG_TERMINATOR_BRANCH;
                diagnostic->actualVersion =
                        (TZrUInt32)source->terminatorKind;
            } else if (source->instructionCount == 0u) {
                diagnostic->expectedVersion = 1u;
                diagnostic->actualVersion = 0u;
            } else {
                const SZrSemanticIrInstruction *tail =
                        (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                                (SZrArray *)&semantic->instructions,
                                source->firstInstructionIndex +
                                        source->instructionCount - 1u);
                diagnostic->expectedVersion = tail->opcode !=
                                ZR_SEMANTIC_IR_BRANCH
                        ? ZR_SEMANTIC_IR_BRANCH
                        : 0u;
                diagnostic->actualVersion = tail->opcode !=
                                ZR_SEMANTIC_IR_BRANCH
                        ? (TZrUInt32)tail->opcode
                        : tail->operandCount;
            }
        } else {
            diagnostic->expectedVersion = ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT;
            diagnostic->actualVersion = (TZrUInt32)edge->kind;
        }
    }
    return ZR_FALSE;
}

TZrBool zr_parser_exec_ir_validate_cfg_edges(
        const SZrSemanticIrFunction *semantic,
        SZrExecIrFunction *output,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 sourceIndex;

    for (sourceIndex = 0u; sourceIndex < output->blockCount; ++sourceIndex) {
        const SZrParserCfgBlock *block = (const SZrParserCfgBlock *)
                ZrCore_Array_Get(
                        (SZrArray *)&semantic->cfg.blocks, sourceIndex);
        TZrUInt32 edgeCount;
        TZrUInt32 edgeIndex;

        if (block->id != sourceIndex) {
            control_edge_diagnostic(
                    diagnostic, output, sourceIndex + 1u, 0u);
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK;
                diagnostic->expectedVersion = sourceIndex + 1u;
                diagnostic->actualVersion = block->id == UINT32_MAX
                        ? UINT32_MAX
                        : block->id + 1u;
            }
            return ZR_FALSE;
        }
        if (block->outgoingEdges.isValid) {
            if (block->outgoingEdges.length > UINT32_MAX ||
                block->outgoingEdges.length > block->outgoingEdges.capacity ||
                (block->outgoingEdges.length != 0u &&
                 (block->outgoingEdges.head == ZR_NULL ||
                  block->outgoingEdges.elementSize !=
                          sizeof(SZrParserCfgEdge)))) {
                control_edge_diagnostic(
                        diagnostic, output, sourceIndex + 1u, 0u);
                if (diagnostic != ZR_NULL) {
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                }
                return ZR_FALSE;
            }
            edgeCount = (TZrUInt32)block->outgoingEdges.length;
        } else {
            if (block->successorCount >
                ZR_PARSER_CFG_INLINE_SUCCESSOR_CAPACITY) {
                control_edge_diagnostic(
                        diagnostic, output, sourceIndex + 1u, 0u);
                if (diagnostic != ZR_NULL) {
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                }
                return ZR_FALSE;
            }
            edgeCount = block->successorCount;
        }
        if (block->instructionCount != 0u) {
            TZrUInt32 instructionIndex;
            TZrSemanticInstructionId iteratorSite = 0u;

            for (instructionIndex = 0u;
                 instructionIndex < block->instructionCount;
                 ++instructionIndex) {
                const SZrSemanticIrInstruction *instruction =
                        (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                                (SZrArray *)&semantic->instructions,
                                block->firstInstructionIndex +
                                        instructionIndex);
                if (semantic_opcode_requires_invoke_edges(
                            instruction->opcode)) {
                    iteratorSite = instruction->id;
                }
            }
            if (iteratorSite != 0u &&
                !zr_parser_exec_ir_has_typed_invoke_edges(semantic, block)) {
                TZrUInt32 actualKind = UINT32_MAX;

                if (block->outgoingEdges.isValid && edgeCount > 1u) {
                    const SZrParserCfgEdge *edge =
                            (const SZrParserCfgEdge *)ZrCore_Array_Get(
                                    (SZrArray *)&block->outgoingEdges, 1u);
                    actualKind = (TZrUInt32)edge->kind;
                }
                control_edge_diagnostic(
                        diagnostic, output, sourceIndex + 1u, iteratorSite);
                if (diagnostic != ZR_NULL) {
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
                    diagnostic->sourceId = iteratorSite;
                    diagnostic->expectedVersion =
                            ZR_PARSER_CFG_EDGE_EXCEPTION;
                    diagnostic->actualVersion = actualKind;
                }
                return ZR_FALSE;
            }
        }
        for (edgeIndex = 0u; edgeIndex < edgeCount; ++edgeIndex) {
            const SZrParserCfgEdge *edge = block->outgoingEdges.isValid
                    ? (const SZrParserCfgEdge *)ZrCore_Array_Get(
                              (SZrArray *)&block->outgoingEdges, edgeIndex)
                    : ZR_NULL;
            TZrUInt32 destinationIndex = edge != ZR_NULL
                    ? edge->toBlockId
                    : block->successors[edgeIndex];
            const SZrParserCfgBlock *destination;

            if (edge != ZR_NULL && edge->fromBlockId != sourceIndex) {
                control_edge_diagnostic(
                        diagnostic, output, sourceIndex + 1u, 0u);
                if (diagnostic != ZR_NULL) {
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK;
                    diagnostic->expectedVersion = sourceIndex + 1u;
                    diagnostic->actualVersion = edge->fromBlockId == UINT32_MAX
                            ? UINT32_MAX
                            : edge->fromBlockId + 1u;
                }
                return ZR_FALSE;
            }
            if (edge != ZR_NULL &&
                (edge->kind < ZR_PARSER_CFG_EDGE_NORMAL ||
                 edge->kind >= ZR_PARSER_CFG_EDGE_ENUM_MAX)) {
                control_edge_diagnostic(
                        diagnostic, output, sourceIndex + 1u, 0u);
                if (diagnostic != ZR_NULL) {
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                    diagnostic->expectedVersion =
                            ZR_PARSER_CFG_EDGE_ENUM_MAX - 1u;
                    diagnostic->actualVersion = (TZrUInt32)edge->kind;
                }
                return ZR_FALSE;
            }
            if (destinationIndex >= output->blockCount) {
                control_edge_diagnostic(
                        diagnostic, output, sourceIndex + 1u, 0u);
                if (diagnostic != ZR_NULL) {
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK;
                    diagnostic->expectedVersion = output->blockCount;
                    diagnostic->actualVersion = destinationIndex == UINT32_MAX
                            ? UINT32_MAX
                            : destinationIndex + 1u;
                }
                return ZR_FALSE;
            }
            destination = (const SZrParserCfgBlock *)ZrCore_Array_Get(
                    (SZrArray *)&semantic->cfg.blocks, destinationIndex);
            if (edge != ZR_NULL &&
                edge->kind >= ZR_PARSER_CFG_EDGE_EXCEPTION &&
                !(edge->kind == ZR_PARSER_CFG_EDGE_EXCEPTION &&
                  edgeIndex == 1u &&
                  zr_parser_exec_ir_has_typed_invoke_edges(
                          semantic, block)) &&
                !(edge->kind == ZR_PARSER_CFG_EDGE_CLEANUP &&
                  cleanup_edge_is_representable(
                          semantic, block, destination, edgeCount))) {
                return reject_control_edge(
                        semantic, output, diagnostic, sourceIndex,
                        block, destination, edge, edgeCount);
            }
        }
        if (block->terminatorKind ==
                    ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH &&
            !cleanup_dispatch_is_representable(
                    semantic, block, edgeCount)) {
            return reject_cleanup_dispatch(
                    semantic, output, diagnostic, sourceIndex,
                    block, edgeCount);
        }
        if (edgeCount != 0u &&
            (block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_RETURN ||
             block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_THROW ||
             block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_SUSPEND ||
             block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_EXIT)) {
            TZrSemanticInstructionId site = control_edge_site(semantic, block);

            control_edge_diagnostic(
                    diagnostic, output, sourceIndex + 1u, site);
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
                diagnostic->sourceId = site;
                diagnostic->expectedVersion = ZR_PARSER_CFG_TERMINATOR_NONE;
                diagnostic->actualVersion =
                        (TZrUInt32)block->terminatorKind;
            }
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
