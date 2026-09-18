#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"

#include "exec_ir_internal.h"

#include <stdlib.h>
#include <string.h>

static void diag_missing(SZrExecIrDiagnostic *d, const SZrExecIrFunction *f,
                         TZrUInt32 block, TZrUInt32 instruction) {
    if (d != ZR_NULL) {
        memset(d, 0, sizeof(*d));
        d->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        d->functionToken = f != ZR_NULL ? f->functionToken : 0u;
        d->blockId = block;
        d->instructionId = instruction;
    }
}

static EZrExecIrOpcode map_opcode(EZrSemanticIrOpcode op) {
    switch (op) {
        case ZR_SEMANTIC_IR_CONSTANT: return ZR_EXEC_IR_OPCODE_CONSTANT;
        case ZR_SEMANTIC_IR_CONVERT: return ZR_EXEC_IR_OPCODE_CONVERT;
        case ZR_SEMANTIC_IR_PLACE_BASE: return ZR_EXEC_IR_OPCODE_PLACE_BASE;
        case ZR_SEMANTIC_IR_PLACE_PROJECT: return ZR_EXEC_IR_OPCODE_PLACE_PROJECT;
        case ZR_SEMANTIC_IR_LOAD: return ZR_EXEC_IR_OPCODE_LOAD;
        case ZR_SEMANTIC_IR_STORE: return ZR_EXEC_IR_OPCODE_STORE;
        case ZR_SEMANTIC_IR_MOVE: return ZR_EXEC_IR_OPCODE_MOVE;
        case ZR_SEMANTIC_IR_COPY: return ZR_EXEC_IR_OPCODE_COPY;
        case ZR_SEMANTIC_IR_DROP: return ZR_EXEC_IR_OPCODE_DROP;
        case ZR_SEMANTIC_IR_CALL_TYPED:
        case ZR_SEMANTIC_IR_CALL_VIRTUAL:
        case ZR_SEMANTIC_IR_CALL_DYNAMIC:
        case ZR_SEMANTIC_IR_CALL_META: return ZR_EXEC_IR_OPCODE_CALL;
        case ZR_SEMANTIC_IR_BRANCH: return ZR_EXEC_IR_OPCODE_BRANCH;
        case ZR_SEMANTIC_IR_SWITCH: return ZR_EXEC_IR_OPCODE_SWITCH;
        case ZR_SEMANTIC_IR_RETURN: return ZR_EXEC_IR_OPCODE_RETURN;
        case ZR_SEMANTIC_IR_THROW: return ZR_EXEC_IR_OPCODE_THROW;
        case ZR_SEMANTIC_IR_YIELD_SUSPEND: return ZR_EXEC_IR_OPCODE_SUSPEND;
        case ZR_SEMANTIC_IR_GC_NEW: return ZR_EXEC_IR_OPCODE_ALLOC;
        case ZR_SEMANTIC_IR_INITIALIZE: return ZR_EXEC_IR_OPCODE_STORE;
        default: return ZR_EXEC_IR_OPCODE_INVALID;
    }
}

static TZrBool canonical_array_shape(const SZrArray *array, TZrSize elementSize) {
    return (TZrBool)(array->length <= UINT32_MAX &&
                     array->length <= array->capacity &&
                     (array->length == 0u ||
                      (array->isValid && array->head != ZR_NULL &&
                       array->elementSize == elementSize)));
}

static TZrBool validate_semantic_places(
        const SZrSemanticIrFunction *semantic,
        SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 i;

    if (!canonical_array_shape(
                &semantic->places.places, sizeof(SZrParserPlace))) {
        diag_missing(diagnostic, ZR_NULL, 0u, 0u);
        if (diagnostic != ZR_NULL)
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
        return ZR_FALSE;
    }
    for (i = 0u; i < (TZrUInt32)semantic->places.places.length; ++i) {
        const SZrParserPlace *place = (const SZrParserPlace *)ZrCore_Array_Get(
                (SZrArray *)&semantic->places.places, i);
        if (place == ZR_NULL || place->id != i + 1u ||
            place->parentId > semantic->places.places.length ||
            !canonical_array_shape(
                    &place->projections, sizeof(SZrParserPlaceProjection))) {
            diag_missing(diagnostic, ZR_NULL, 0u, 0u);
            if (diagnostic != ZR_NULL) {
                diagnostic->code = place != ZR_NULL && place->id != i + 1u
                    ? ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE
                    : ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                diagnostic->functionToken =
                        (TZrMetadataToken)semantic->symbolId;
                diagnostic->expectedVersion = i + 1u;
                diagnostic->actualVersion = place != ZR_NULL ? place->id : 0u;
            }
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrExecIrValueId place_value_id(TZrExecIrValueId firstPlaceValue,
                                       TZrPlaceId placeId) {
    return firstPlaceValue + placeId - 1u;
}

static const SZrParserPlace *semantic_place_at(
        const SZrSemanticIrFunction *semantic,
        TZrPlaceId placeId) {
    if (semantic == ZR_NULL || placeId == ZR_PLACE_ID_INVALID ||
        placeId > semantic->places.places.length) {
        return ZR_NULL;
    }
    return (const SZrParserPlace *)ZrCore_Array_Get(
            (SZrArray *)&semantic->places.places, placeId - 1u);
}

static TZrBool append_place_values(const SZrSemanticIrFunction *semantic,
                                   SZrExecIrFunction *output,
                                   TZrExecIrValueId *firstPlaceValue,
                                   TZrExecIrValueId *firstPlaceProvenance) {
    TZrUInt32 i;

    *firstPlaceValue = output->valueCount + 1u;
    for (i = 0u; i < (TZrUInt32)semantic->places.places.length; ++i) {
        const SZrParserPlace *place = (const SZrParserPlace *)ZrCore_Array_Get(
                (SZrArray *)&semantic->places.places, i);
        if (ZrCore_ExecIr_FunctionAddValue(
                    output, (TZrMetadataToken)place->typeId,
                    ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                    ZR_EXEC_IR_NULLABILITY_UNKNOWN) ==
            ZR_EXEC_IR_VALUE_ID_INVALID) {
            return ZR_FALSE;
        }
    }
    *firstPlaceProvenance = output->valueCount + 1u;
    for (i = 0u; i < (TZrUInt32)semantic->places.places.length; ++i) {
        const SZrParserPlace *place = (const SZrParserPlace *)ZrCore_Array_Get(
                (SZrArray *)&semantic->places.places, i);
        if (ZrCore_ExecIr_FunctionAddExternalValue(
                    output, (TZrMetadataToken)place->typeId,
                    ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                    ZR_EXEC_IR_NULLABILITY_UNKNOWN) ==
            ZR_EXEC_IR_VALUE_ID_INVALID) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool validate_semantic_ids(const SZrSemanticIrFunction *semantic,
                                     SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 i;
    for (i = 0u; i < semantic->values.length; ++i) {
        const SZrSemanticIrValue *value = (const SZrSemanticIrValue *)
            ZrCore_Array_Get((SZrArray *)&semantic->values, i);
        if (value->id != i + 1u) {
            diag_missing(diagnostic, ZR_NULL, 0u, 0u);
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE;
                diagnostic->functionToken = (TZrMetadataToken)semantic->symbolId;
                diagnostic->expectedVersion = i + 1u;
                diagnostic->actualVersion = value->id;
            }
            return ZR_FALSE;
        }
    }
    for (i = 0u; i < semantic->instructions.length; ++i) {
        const SZrSemanticIrInstruction *instruction = (const SZrSemanticIrInstruction *)
            ZrCore_Array_Get((SZrArray *)&semantic->instructions, i);
        if (instruction->id != i + 1u) {
            diag_missing(diagnostic, ZR_NULL, 0u, instruction->id);
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                diagnostic->functionToken = (TZrMetadataToken)semantic->symbolId;
                diagnostic->sourceId = instruction->id;
                diagnostic->expectedVersion = i + 1u;
                diagnostic->actualVersion = instruction->id;
            }
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/* CFG blocks may be ordered differently from the source instruction pool,
 * but their slices must partition it without duplication or omission. */
static TZrBool validate_instruction_owners(const SZrSemanticIrFunction *semantic,
                                          SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 total = (TZrUInt32)semantic->instructions.length;
    TZrUInt32 covered = 0u, i, j;
    unsigned char *claimed = total != 0u ? (unsigned char *)calloc(total, 1u) : ZR_NULL;
    if (total != 0u && claimed == ZR_NULL) {
        diag_missing(diagnostic, ZR_NULL, 0u, 0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->functionToken = (TZrMetadataToken)semantic->symbolId;
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
        }
        return ZR_FALSE;
    }
    for (i = 0u; i < semantic->cfg.blocks.length; ++i) {
        const SZrParserCfgBlock *block = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&semantic->cfg.blocks, i);
        if (block->instructionCount != 0u &&
            (block->firstInstructionIndex > total ||
             block->instructionCount > total - block->firstInstructionIndex)) {
            diag_missing(diagnostic, ZR_NULL, i + 1u, 0u);
            if (diagnostic != ZR_NULL) {
                diagnostic->functionToken = (TZrMetadataToken)semantic->symbolId;
                diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
            }
            free(claimed);
            return ZR_FALSE;
        }
        for (j = 0u; j < block->instructionCount; ++j) {
            TZrUInt32 index = block->firstInstructionIndex + j;
            if (claimed[index] != 0u) {
                const SZrSemanticIrInstruction *instruction = (const SZrSemanticIrInstruction *)
                    ZrCore_Array_Get((SZrArray *)&semantic->instructions, index);
                diag_missing(diagnostic, ZR_NULL, i + 1u, instruction->id);
                if (diagnostic != ZR_NULL) {
                    diagnostic->functionToken = (TZrMetadataToken)semantic->symbolId;
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                    diagnostic->expectedVersion = 1u;
                    diagnostic->actualVersion = 2u;
                }
                free(claimed);
                return ZR_FALSE;
            }
            claimed[index] = 1u;
            ++covered;
        }
    }
    if (covered != total) {
        for (i = 0u; i < total; ++i) {
            if (claimed[i] == 0u) {
                const SZrSemanticIrInstruction *instruction = (const SZrSemanticIrInstruction *)
                    ZrCore_Array_Get((SZrArray *)&semantic->instructions, i);
                diag_missing(diagnostic, ZR_NULL, 0u, instruction->id);
                if (diagnostic != ZR_NULL) {
                    diagnostic->functionToken = (TZrMetadataToken)semantic->symbolId;
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                    diagnostic->expectedVersion = total;
                    diagnostic->actualVersion = covered;
                }
                break;
            }
        }
        free(claimed);
        return ZR_FALSE;
    }
    free(claimed);
    return ZR_TRUE;
}

static TZrBool append_source(SZrExecIrFunction *f, const SZrSemanticIrInstruction *in) {
    SZrExecIrSourceMap *p;
    if (f->sourceMapCount == f->sourceMapCapacity) {
        TZrUInt32 cap = f->sourceMapCapacity == 0u ? 8u : f->sourceMapCapacity * 2u;
        p = (SZrExecIrSourceMap *)realloc(f->sourceMaps, (size_t)cap * sizeof(*p));
        if (p == ZR_NULL) return ZR_FALSE;
        f->sourceMaps = p; f->sourceMapCapacity = cap;
    }
    p = &f->sourceMaps[f->sourceMapCount++];
    p->sourceId = in->id; p->instructionId = in->id;
    p->startOffset = (TZrUInt32)in->sourceRange.start.offset;
    p->endOffset = (TZrUInt32)in->sourceRange.end.offset;
    p->startLine = (TZrUInt32)in->sourceRange.start.line;
    p->startColumn = (TZrUInt32)in->sourceRange.start.column;
    p->endLine = (TZrUInt32)in->sourceRange.end.line;
    p->endColumn = (TZrUInt32)in->sourceRange.end.column;
    return ZR_TRUE;
}

static TZrBool has_typed_invoke_edges(const SZrSemanticIrFunction *semantic,
                                     const SZrParserCfgBlock *block) {
    const SZrParserCfgEdge *normal, *exception;
    const SZrSemanticIrInstruction *tail;
    if (!block->outgoingEdges.isValid || block->outgoingEdges.length != 2u ||
        block->instructionCount == 0u) return ZR_FALSE;
    normal = (const SZrParserCfgEdge *)ZrCore_Array_Get(
        (SZrArray *)&block->outgoingEdges, 0u);
    exception = (const SZrParserCfgEdge *)ZrCore_Array_Get(
        (SZrArray *)&block->outgoingEdges, 1u);
    if (normal->kind != ZR_PARSER_CFG_EDGE_NORMAL ||
        exception->kind != ZR_PARSER_CFG_EDGE_EXCEPTION ||
        normal->toBlockId == exception->toBlockId) return ZR_FALSE;
    tail = (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
        (SZrArray *)&semantic->instructions,
        block->firstInstructionIndex + block->instructionCount - 1u);
    return (TZrBool)(tail->opcode == ZR_SEMANTIC_IR_CALL_TYPED ||
                    tail->opcode == ZR_SEMANTIC_IR_CALL_VIRTUAL ||
                    tail->opcode == ZR_SEMANTIC_IR_CALL_DYNAMIC ||
                    tail->opcode == ZR_SEMANTIC_IR_CALL_META);
}

static TZrBool validate_semantic_cfg_edges(const SZrSemanticIrFunction *semantic,
                                          SZrExecIrFunction *output,
                                          SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 i;
    for (i = 0u; i < output->blockCount; ++i) {
        const SZrParserCfgBlock *block = (const SZrParserCfgBlock *)
            ZrCore_Array_Get((SZrArray *)&semantic->cfg.blocks, i);
        TZrUInt32 count, j;
        if (block->id != i) {
            diag_missing(diagnostic, output, i + 1u, 0u);
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK;
                diagnostic->expectedVersion = i + 1u;
                diagnostic->actualVersion = block->id == UINT32_MAX
                    ? UINT32_MAX : block->id + 1u;
            }
            return ZR_FALSE;
        }
        if (block->outgoingEdges.isValid) {
            if (block->outgoingEdges.length > UINT32_MAX ||
                block->outgoingEdges.length > block->outgoingEdges.capacity ||
                (block->outgoingEdges.length != 0u &&
                 (block->outgoingEdges.head == ZR_NULL ||
                  block->outgoingEdges.elementSize != sizeof(SZrParserCfgEdge)))) {
                diag_missing(diagnostic, output, i + 1u, 0u);
                if (diagnostic != ZR_NULL)
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                return ZR_FALSE;
            }
            count = (TZrUInt32)block->outgoingEdges.length;
        } else {
            if (block->successorCount > ZR_PARSER_CFG_INLINE_SUCCESSOR_CAPACITY) {
                diag_missing(diagnostic, output, i + 1u, 0u);
                if (diagnostic != ZR_NULL)
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                return ZR_FALSE;
            }
            count = block->successorCount;
        }
        if (has_typed_invoke_edges(semantic, block)) {
            TZrUInt32 prior;
            for (prior = 0u; prior + 1u < block->instructionCount; ++prior) {
                const SZrSemanticIrInstruction *instruction =
                    (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->instructions,
                        block->firstInstructionIndex + prior);
                const SZrExecIrOpcodeInfo *info = ZrCore_ExecIr_OpcodeInfo(
                    map_opcode(instruction->opcode));
                if (info != ZR_NULL &&
                    (info->flags & (ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW |
                                    ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND)) != 0u) {
                    diag_missing(diagnostic, output, i + 1u, instruction->id);
                    if (diagnostic != ZR_NULL) {
                        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
                        diagnostic->sourceId = instruction->id;
                    }
                    return ZR_FALSE;
                }
            }
        }
        for (j = 0u; j < count; ++j) {
            if (block->outgoingEdges.isValid) {
                const SZrParserCfgEdge *edge = (const SZrParserCfgEdge *)
                    ZrCore_Array_Get((SZrArray *)&block->outgoingEdges, j);
                if (edge->fromBlockId != i) {
                    diag_missing(diagnostic, output, i + 1u, 0u);
                    if (diagnostic != ZR_NULL) {
                        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK;
                        diagnostic->expectedVersion = i + 1u;
                        diagnostic->actualVersion = edge->fromBlockId == UINT32_MAX
                            ? UINT32_MAX : edge->fromBlockId + 1u;
                    }
                    return ZR_FALSE;
                }
                if (edge->kind < ZR_PARSER_CFG_EDGE_NORMAL ||
                    edge->kind >= ZR_PARSER_CFG_EDGE_ENUM_MAX) {
                    diag_missing(diagnostic, output, i + 1u, 0u);
                    if (diagnostic != ZR_NULL) {
                        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                        diagnostic->expectedVersion = ZR_PARSER_CFG_EDGE_ENUM_MAX - 1u;
                        diagnostic->actualVersion = (TZrUInt32)edge->kind;
                    }
                    return ZR_FALSE;
                }
                if (edge->kind >= ZR_PARSER_CFG_EDGE_EXCEPTION &&
                    !(edge->kind == ZR_PARSER_CFG_EDGE_EXCEPTION && j == 1u &&
                      has_typed_invoke_edges(semantic, block))) {
                    TZrSemanticInstructionId site = 0u;
                    if (block->instructionCount != 0u) {
                        const SZrSemanticIrInstruction *terminator =
                            (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                                (SZrArray *)&semantic->instructions,
                                block->firstInstructionIndex + block->instructionCount - 1u);
                        site = terminator->id;
                    }
                    diag_missing(diagnostic, output, i + 1u, site);
                    if (diagnostic != ZR_NULL) {
                        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
                        diagnostic->sourceId = site;
                        diagnostic->expectedVersion = ZR_PARSER_CFG_EDGE_SWITCH_DEFAULT;
                        diagnostic->actualVersion = (TZrUInt32)edge->kind;
                    }
                    return ZR_FALSE;
                }
            }
            TZrUInt32 destination = block->outgoingEdges.isValid
                ? ((const SZrParserCfgEdge *)ZrCore_Array_Get(
                       (SZrArray *)&block->outgoingEdges, j))->toBlockId
                : block->successors[j];
            if (destination >= output->blockCount) {
                diag_missing(diagnostic, output, i + 1u, 0u);
                if (diagnostic != ZR_NULL) {
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK;
                    diagnostic->expectedVersion = output->blockCount;
                    diagnostic->actualVersion = destination == UINT32_MAX
                        ? UINT32_MAX : destination + 1u;
                }
                return ZR_FALSE;
            }
        }
        if (count != 0u &&
            (block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_RETURN ||
             block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_THROW ||
             block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_SUSPEND ||
             block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_CLEANUP_DISPATCH ||
             block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_EXIT)) {
            TZrSemanticInstructionId site = 0u;
            if (block->instructionCount != 0u) {
                const SZrSemanticIrInstruction *tail =
                    (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                        (SZrArray *)&semantic->instructions,
                        block->firstInstructionIndex + block->instructionCount - 1u);
                site = tail->id;
            }
            diag_missing(diagnostic, output, i + 1u, site);
            if (diagnostic != ZR_NULL) {
                diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
                diagnostic->sourceId = site;
                diagnostic->expectedVersion = ZR_PARSER_CFG_TERMINATOR_NONE;
                diagnostic->actualVersion = (TZrUInt32)block->terminatorKind;
            }
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool append_cfg_predecessors(const SZrSemanticIrFunction *semantic,
                                      SZrExecIrFunction *output,
                                      SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 *counts = ZR_NULL;
    TZrUInt32 *cursor = ZR_NULL;
    TZrExecIrBlockId *rows = ZR_NULL;
    SZrExecIrRange appended = {0u, 0u};
    TZrUInt32 total = 0u;
    TZrUInt32 i;
    TZrBool result = ZR_FALSE;

    counts = (TZrUInt32 *)calloc(output->blockCount, sizeof(*counts));
    cursor = (TZrUInt32 *)calloc(output->blockCount, sizeof(*cursor));
    if (counts == ZR_NULL || cursor == ZR_NULL) goto oom;
    for (i = 0u; i < output->blockCount; ++i) {
        const SZrParserCfgBlock *block = (const SZrParserCfgBlock *)
            ZrCore_Array_Get((SZrArray *)&semantic->cfg.blocks, i);
        TZrUInt32 edgeCount = block->outgoingEdges.isValid
                                 ? (TZrUInt32)block->outgoingEdges.length
                                 : block->successorCount;
        TZrUInt32 j;
        for (j = 0u; j < edgeCount; ++j) {
            TZrUInt32 destination = block->outgoingEdges.isValid
                ? ((const SZrParserCfgEdge *)ZrCore_Array_Get(
                       (SZrArray *)&block->outgoingEdges, j))->toBlockId
                : block->successors[j];
            if (total == UINT32_MAX || counts[destination] == UINT32_MAX) {
                diag_missing(diagnostic, output, i + 1u, 0u);
                if (diagnostic != ZR_NULL)
                    diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
                goto cleanup;
            }
            ++counts[destination];
            ++total;
        }
    }
#if SIZE_MAX <= UINT32_MAX
    if (total > SIZE_MAX / sizeof(*rows)) {
        diag_missing(diagnostic, output, 0u, 0u);
        if (diagnostic != ZR_NULL)
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW;
        goto cleanup;
    }
#endif
    if (total != 0u) {
        rows = (TZrExecIrBlockId *)malloc((TZrSize)total * sizeof(*rows));
        if (rows == ZR_NULL) goto oom;
    }
    for (i = 1u; i < output->blockCount; ++i)
        cursor[i] = cursor[i - 1u] + counts[i - 1u];
    for (i = 0u; i < output->blockCount; ++i) {
        const SZrParserCfgBlock *block = (const SZrParserCfgBlock *)
            ZrCore_Array_Get((SZrArray *)&semantic->cfg.blocks, i);
        TZrUInt32 edgeCount = block->outgoingEdges.isValid
                                 ? (TZrUInt32)block->outgoingEdges.length
                                 : block->successorCount;
        TZrUInt32 j;
        for (j = 0u; j < edgeCount; ++j) {
            TZrUInt32 destination = block->outgoingEdges.isValid
                ? ((const SZrParserCfgEdge *)ZrCore_Array_Get(
                       (SZrArray *)&block->outgoingEdges, j))->toBlockId
                : block->successors[j];
            rows[cursor[destination]++] = i + 1u;
        }
    }
    if (total != 0u &&
        !ZrCore_ExecIr_FunctionAppendPredecessors(output, rows, total, &appended))
        goto oom;
    for (i = 0u; i < output->blockCount; ++i) {
        output->blocks[i].predecessorRange.start =
            appended.start + cursor[i] - counts[i];
        output->blocks[i].predecessorRange.count = counts[i];
    }
    result = ZR_TRUE;
    goto cleanup;

oom:
    diag_missing(diagnostic, output, 0u, 0u);
    if (diagnostic != ZR_NULL) diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
cleanup:
    free(counts);
    free(cursor);
    free(rows);
    return result;
}

static TZrBool verify_unpublished_ssa(SZrExecIrFunction *output,
                                     SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrFunctionId savedId = output->id;
    TZrMetadataToken savedToken = output->functionToken;
    TZrBool valid;
    /* The core verifier requires published identities. This isolated
     * candidate may not have either identity yet; keep both unchanged after
     * verification so BuildModule can assign the real contract. */
    if (savedId == ZR_EXEC_IR_FUNCTION_ID_INVALID) output->id = 1u;
    if (savedToken == 0u) output->functionToken = 1u;
    valid = ZrCore_ExecIr_VerifyFunction(output, ZR_EXEC_IR_VERIFY_SSA, diagnostic);
    output->id = savedId;
    output->functionToken = savedToken;
    return valid;
}

static TZrBool build_impl(const struct SZrSemanticIrFunction *semanticFunction,
                              const SZrExecIrBuildOptions *options,
                              SZrExecIrFunction *output,
                              SZrExecIrDiagnostic *diagnostic) {
    const SZrSemanticIrFunction *s = semanticFunction;
    TZrExecIrValueId firstPlaceValue = ZR_EXEC_IR_VALUE_ID_INVALID;
    TZrExecIrValueId firstPlaceProvenance = ZR_EXEC_IR_VALUE_ID_INVALID;
    TZrUInt32 i;
    (void)options;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (s == ZR_NULL || output == ZR_NULL || !s->instructions.isValid || !s->cfg.blocks.isValid) {
        diag_missing(diagnostic, output, 0u, 0u); return ZR_FALSE;
    }
    if (!canonical_array_shape(&s->cfg.blocks, sizeof(SZrParserCfgBlock)) ||
        !canonical_array_shape(&s->instructions, sizeof(SZrSemanticIrInstruction)) ||
        !canonical_array_shape(&s->values, sizeof(SZrSemanticIrValue)) ||
        !canonical_array_shape(&s->valueOperands, sizeof(TZrValueId))) {
        diag_missing(diagnostic, output, 0u, 0u);
        if (diagnostic != ZR_NULL)
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
        return ZR_FALSE;
    }
    if (!validate_semantic_places(s, diagnostic)) return ZR_FALSE;
    if (!validate_semantic_ids(s, diagnostic) ||
        !validate_instruction_owners(s, diagnostic)) return ZR_FALSE;
    ZrCore_ExecIr_FunctionInit(output);
    output->functionToken = (TZrMetadataToken)s->symbolId;
    output->signatureHash = (TZrUInt64)s->callableTypeId;
    output->contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    output->contract.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    output->contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    output->contract.targetToken = output->functionToken;
    output->contract.signatureHash = output->signatureHash;

    for (i = 0u; i < (TZrUInt32)s->values.length; ++i) {
        const SZrSemanticIrValue *v = (const SZrSemanticIrValue *)ZrCore_Array_Get((SZrArray *)&s->values, i);
        if (ZrCore_ExecIr_FunctionAddValue(output, (TZrMetadataToken)v->typeId,
                    ZR_EXEC_IR_OWNERSHIP_UNKNOWN, ZR_EXEC_IR_NULLABILITY_UNKNOWN) == ZR_EXEC_IR_VALUE_ID_INVALID) {
            diag_missing(diagnostic, output, 0u, 0u); ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE;
        }
    }
    if (!append_place_values(s, output, &firstPlaceValue,
                             &firstPlaceProvenance)) {
        diag_missing(diagnostic, output, 0u, 0u);
        if (diagnostic != ZR_NULL)
            diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
        ZrCore_ExecIr_FreeFunction(output);
        return ZR_FALSE;
    }
    if (!zr_parser_exec_ir_mark_place_values(
                s, output, firstPlaceValue, diagnostic)) {
        ZrCore_ExecIr_FreeFunction(output);
        return ZR_FALSE;
    }
    for (i = 0u; i < (TZrUInt32)s->cfg.blocks.length; ++i) {
        const SZrParserCfgBlock *b = (const SZrParserCfgBlock *)ZrCore_Array_Get((SZrArray *)&s->cfg.blocks, i);
        TZrUInt32 id = ZrCore_ExecIr_FunctionAddBlock(output,
                (i == s->cfg.entryBlockId ? ZR_EXEC_IR_BLOCK_FLAG_ENTRY : 0u) |
                (b->kind == ZR_PARSER_CFG_BLOCK_CLEANUP ? ZR_EXEC_IR_BLOCK_FLAG_CLEANUP : 0u));
        if (id == ZR_EXEC_IR_BLOCK_ID_INVALID) { ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE; }
    }
    output->entryBlockId = s->cfg.entryBlockId < s->cfg.blocks.length
                               ? s->cfg.entryBlockId + 1u : ZR_EXEC_IR_BLOCK_ID_INVALID;
    if (!validate_semantic_cfg_edges(s, output, diagnostic)) return ZR_FALSE;
    for (i = 0u; i < (TZrUInt32)s->cfg.blocks.length; ++i) {
        const SZrParserCfgBlock *b = (const SZrParserCfgBlock *)ZrCore_Array_Get(
            (SZrArray *)&s->cfg.blocks, i);
        if (has_typed_invoke_edges(s, b)) {
            const SZrParserCfgEdge *exception = (const SZrParserCfgEdge *)ZrCore_Array_Get(
                (SZrArray *)&b->outgoingEdges, 1u);
            if (exception->kind == ZR_PARSER_CFG_EDGE_EXCEPTION)
                output->blocks[exception->toBlockId].flags |=
                    ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION;
        }
    }
    for (i = 0u; i < (TZrUInt32)s->cfg.blocks.length; ++i) {
        const SZrParserCfgBlock *b = (const SZrParserCfgBlock *)ZrCore_Array_Get((SZrArray *)&s->cfg.blocks, i);
        SZrExecIrBlock *db = ZrCore_ExecIr_FunctionBlockAt(output, i + 1u);
        TZrUInt32 j;
        db->instructionRange.start = output->instructionCount;
        TZrUInt32 edgeCount = b->outgoingEdges.isValid ? (TZrUInt32)b->outgoingEdges.length : b->successorCount;
        db->successorRange.start = output->successorCount;
        for (j = 0u; j < edgeCount; ++j) {
            TZrUInt32 succ;
            if (b->outgoingEdges.isValid) {
                const SZrParserCfgEdge *edge = (const SZrParserCfgEdge *)ZrCore_Array_Get((SZrArray *)&b->outgoingEdges, j);
                succ = edge->toBlockId;
            } else {
                succ = b->successors[j];
            }
            TZrExecIrBlockId sid = succ + 1u;
            if (!ZrCore_ExecIr_FunctionAppendSuccessors(output, &sid, 1u, ZR_NULL)) {
                diag_missing(diagnostic, output, db->id, 0u);
                if (diagnostic != ZR_NULL) diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
                ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE;
            }
            ++db->successorRange.count;
        }
        if (b->instructionCount != 0u && (b->firstInstructionIndex > s->instructions.length || b->instructionCount > s->instructions.length - b->firstInstructionIndex)) {
            diag_missing(diagnostic, output, db->id, 0u); if (diagnostic != ZR_NULL) diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE; ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE;
        }
        if (b->instructionCount != 0u) {
            for (j = 0u; j < b->instructionCount; ++j) {
                const SZrSemanticIrInstruction *in = (const SZrSemanticIrInstruction *)ZrCore_Array_Get((SZrArray *)&s->instructions, b->firstInstructionIndex + j);
                SZrExecIrInstruction x; SZrExecIrRange rr = {0u, 0u}, orr = {0u, 0u};
                TZrExecIrValueId loweredOperands[2];
                TZrExecIrValueId loweredResult = ZR_EXEC_IR_VALUE_ID_INVALID;
                const TZrExecIrValueId *operandValues = ZR_NULL;
                TZrUInt32 operandCount = in->operandCount;
                TZrUInt32 resultCount =
                        in->resultValueId != ZR_VALUE_ID_INVALID ? 1u : 0u;
                const SZrExecIrOpcodeInfo *info;
                TZrBool isTerminator;
                memset(&x, 0, sizeof(x)); x.opcode = (TZrUInt16)map_opcode(in->opcode); x.sourceId = in->id;
                if (in->opcode == ZR_SEMANTIC_IR_BRANCH && in->operandCount != 0u)
                    x.opcode = ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH;
                if (x.opcode == ZR_EXEC_IR_OPCODE_CALL &&
                    j == b->instructionCount - 1u &&
                    has_typed_invoke_edges(s, b))
                    x.opcode = ZR_EXEC_IR_OPCODE_INVOKE;
                if (x.opcode == ZR_EXEC_IR_OPCODE_INVALID) { diag_missing(diagnostic, output, db->id, in->id); ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE; }
                info = ZrCore_ExecIr_OpcodeInfo((EZrExecIrOpcode)x.opcode);
                isTerminator = (TZrBool)(info != ZR_NULL &&
                    (info->flags & ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR) != 0u);
                x.typeToken = (TZrExecIrTypeToken)in->typeId;
                if (in->operandStart > s->valueOperands.length ||
                    in->operandCount > s->valueOperands.length - in->operandStart) {
                    diag_missing(diagnostic, output, db->id, in->id);
                    if (diagnostic != ZR_NULL) diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                    ZrCore_ExecIr_FreeFunction(output);
                    return ZR_FALSE;
                }
                if (in->operandCount != 0u) {
                    operandValues = (const TZrExecIrValueId *)ZrCore_Array_Get(
                            (SZrArray *)&s->valueOperands, in->operandStart);
                }
                switch (in->opcode) {
                    case ZR_SEMANTIC_IR_PLACE_BASE:
                        if (semantic_place_at(s, in->placeId) != ZR_NULL) {
                            loweredResult = place_value_id(
                                    firstPlaceValue, in->placeId);
                            loweredOperands[0] = place_value_id(
                                    firstPlaceProvenance, in->placeId);
                            operandValues = loweredOperands;
                            operandCount = 1u;
                            resultCount = 1u;
                        }
                        break;
                    case ZR_SEMANTIC_IR_PLACE_PROJECT: {
                        const SZrParserPlace *place = semantic_place_at(
                                s, in->placeId);
                        if (place != ZR_NULL &&
                            place->parentId != ZR_PLACE_ID_INVALID) {
                            loweredResult = place_value_id(
                                    firstPlaceValue, in->placeId);
                            loweredOperands[0] = place_value_id(
                                    firstPlaceValue, place->parentId);
                            loweredOperands[1] =
                                    in->auxiliaryValueId != ZR_VALUE_ID_INVALID
                                        ? in->auxiliaryValueId
                                        : place_value_id(
                                                firstPlaceProvenance,
                                                in->placeId);
                            operandValues = loweredOperands;
                            operandCount = 2u;
                            resultCount = 1u;
                        }
                        break;
                    }
                    case ZR_SEMANTIC_IR_LOAD:
                        if (semantic_place_at(s, in->placeId) != ZR_NULL) {
                            loweredOperands[0] = place_value_id(
                                    firstPlaceValue, in->placeId);
                            operandValues = loweredOperands;
                            operandCount = 1u;
                        }
                        break;
                    case ZR_SEMANTIC_IR_STORE:
                    case ZR_SEMANTIC_IR_INITIALIZE:
                        if (semantic_place_at(s, in->placeId) != ZR_NULL) {
                            loweredOperands[0] = place_value_id(
                                    firstPlaceValue, in->placeId);
                            loweredOperands[1] = in->valueId;
                            operandValues = loweredOperands;
                            operandCount = 2u;
                            resultCount = 0u;
                        }
                        break;
                    case ZR_SEMANTIC_IR_CONVERT:
                    case ZR_SEMANTIC_IR_MOVE:
                    case ZR_SEMANTIC_IR_COPY:
                    case ZR_SEMANTIC_IR_DROP:
                        if (operandCount == 0u &&
                            in->valueId != ZR_VALUE_ID_INVALID) {
                            loweredOperands[0] = in->valueId;
                            operandValues = loweredOperands;
                            operandCount = 1u;
                        }
                        break;
                    default:
                        break;
                }
                if ((operandCount < info->minimumOperands ||
                     operandCount > info->maximumOperands) ||
                    (info->resultArity != ZR_EXEC_IR_VARIADIC &&
                     resultCount != info->resultArity)) {
                    TZrBool wrongOperands = (TZrBool)(
                        operandCount < info->minimumOperands ||
                        operandCount > info->maximumOperands);
                    diag_missing(diagnostic, output, db->id, in->id);
                    if (diagnostic != ZR_NULL) {
                        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                        diagnostic->sourceId = in->id;
                        diagnostic->expectedVersion = wrongOperands
                            ? (operandCount < info->minimumOperands
                                   ? info->minimumOperands
                                   : info->maximumOperands)
                            : info->resultArity;
                        diagnostic->actualVersion = wrongOperands
                            ? operandCount : resultCount;
                    }
                    ZrCore_ExecIr_FreeFunction(output);
                    return ZR_FALSE;
                }
                if (isTerminator != (TZrBool)(j == b->instructionCount - 1u)) {
                    diag_missing(diagnostic, output, db->id, in->id);
                    if (diagnostic != ZR_NULL) diagnostic->code = isTerminator
                        ? ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE
                        : ZR_EXEC_IR_DIAGNOSTIC_MISSING_TERMINATOR;
                    ZrCore_ExecIr_FreeFunction(output);
                    return ZR_FALSE;
                }
                if ((x.opcode == ZR_EXEC_IR_OPCODE_BRANCH && db->successorRange.count != 1u) ||
                    (x.opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH &&
                     db->successorRange.count != 2u) ||
                    (x.opcode == ZR_EXEC_IR_OPCODE_INVOKE &&
                     db->successorRange.count != 2u) ||
                    (x.opcode == ZR_EXEC_IR_OPCODE_SWITCH && db->successorRange.count == 0u) ||
                    (x.opcode == ZR_EXEC_IR_OPCODE_RETURN && db->successorRange.count != 0u)) {
                    diag_missing(diagnostic, output, db->id, in->id);
                    if (diagnostic != ZR_NULL) {
                        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE;
                        diagnostic->expectedVersion = x.opcode == ZR_EXEC_IR_OPCODE_RETURN
                            ? 0u : x.opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH ||
                                   x.opcode == ZR_EXEC_IR_OPCODE_INVOKE ? 2u : 1u;
                        diagnostic->actualVersion = db->successorRange.count;
                    }
                    ZrCore_ExecIr_FreeFunction(output);
                    return ZR_FALSE;
                }
                if (x.opcode == ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH) {
                    /* Oracle and projection interpret successor 0 as true and
                     * successor 1 as false. Inline rows carry no edge kinds. */
                    TZrUInt32 edgeIndex;
                    for (edgeIndex = 0u; edgeIndex < 2u; ++edgeIndex) {
                        EZrParserCfgEdgeKind expected = edgeIndex == 0u
                            ? ZR_PARSER_CFG_EDGE_TRUE_BRANCH
                            : ZR_PARSER_CFG_EDGE_FALSE_BRANCH;
                        EZrParserCfgEdgeKind actual = b->outgoingEdges.isValid
                            ? ((const SZrParserCfgEdge *)ZrCore_Array_Get(
                                (SZrArray *)&b->outgoingEdges, edgeIndex))->kind
                            : ZR_PARSER_CFG_EDGE_NORMAL;
                        if (actual != expected) {
                            diag_missing(diagnostic, output, db->id, in->id);
                            if (diagnostic != ZR_NULL) {
                                diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
                                diagnostic->sourceId = in->id;
                                diagnostic->expectedVersion = expected;
                                diagnostic->actualVersion = actual;
                            }
                            ZrCore_ExecIr_FreeFunction(output);
                            return ZR_FALSE;
                        }
                    }
                }
                if (isTerminator)
                    x.successorRange = db->successorRange;
                if (resultCount != 0u) {
                    TZrExecIrValueId v = loweredResult !=
                                                 ZR_EXEC_IR_VALUE_ID_INVALID
                                             ? loweredResult
                                             : in->resultValueId;
                    if (!ZrCore_ExecIr_FunctionAppendResults(output, &v, 1u, &rr)) { diag_missing(diagnostic, output, db->id, in->id); ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE; }
                }
                if (operandCount != 0u) {
                    if (!ZrCore_ExecIr_FunctionAppendOperands(
                                output, operandValues, operandCount, &orr)) {
                        diag_missing(diagnostic, output, db->id, in->id);
                        if (diagnostic != ZR_NULL) diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
                        ZrCore_ExecIr_FreeFunction(output);
                        return ZR_FALSE;
                    }
                }
                x.results = rr; x.operands = orr;
                { TZrExecIrInstructionId execId = 0u;
                if (!ZrCore_ExecIr_FunctionAppendInstruction(output, &x, &execId) || !append_source(output, in)) { diag_missing(diagnostic, output, db->id, in->id); ZrCore_ExecIr_FreeFunction(output); return ZR_FALSE; }
                output->sourceMaps[output->sourceMapCount - 1u].instructionId = execId; }
            }
            db->instructionRange.count = b->instructionCount;
            db->terminatorInstructionId = db->instructionRange.start + db->instructionRange.count;
        }
    }
    return append_cfg_predecessors(s, output, diagnostic) &&
           ZrParser_ExecIr_ComputeDominators(output, diagnostic) &&
           ZrParser_ExecIr_BuildSsa(output, diagnostic) &&
           verify_unpublished_ssa(output, diagnostic);
}

TZrBool ZrParser_ExecIr_Build(const struct SZrSemanticIrFunction *semanticFunction,
                              const SZrExecIrBuildOptions *options,
                              SZrExecIrFunction *output,
                              SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrFunction temporary;
    TZrBool ok;
    if (output == ZR_NULL) return ZR_FALSE;
    ZrCore_ExecIr_FunctionInit(&temporary);
    ok = build_impl(semanticFunction, options, &temporary, diagnostic);
    if (!ok) { ZrCore_ExecIr_FreeFunction(&temporary); return ZR_FALSE; }
    ZrCore_ExecIr_FreeFunction(output);
    *output = temporary;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_BuildModule(const SZrExecIrBuildInput *input,
                                    SZrExecIrModule *output,
                                    SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrFunction prepared;
    SZrExecIrFunction *slot;
    SZrExecutionContract contract;
    TZrExecIrFunctionId id;
    if (input == ZR_NULL || output == ZR_NULL || input->semanticFunction == ZR_NULL ||
        input->functionToken == 0u) {
        diag_missing(diagnostic, ZR_NULL, 0u, 0u);
        return ZR_FALSE;
    }
    ZrCore_ExecIr_FunctionInit(&prepared);
    if (!ZrParser_ExecIr_Build(input->semanticFunction, &input->options,
                               &prepared, diagnostic)) {
        if (diagnostic != ZR_NULL) diagnostic->functionToken = input->functionToken;
        return ZR_FALSE;
    }
    /* Do not reserve a published module slot until the whole function is
     * ready: a failed build must not leave a zero-ID function behind. */
    if (!ZrCore_ExecIr_ModuleAddFunction(output, input->functionToken,
                                         input->signatureHash, &id)) {
        ZrCore_ExecIr_FreeFunction(&prepared);
        diag_missing(diagnostic, ZR_NULL, 0u, 0u);
        if (diagnostic != ZR_NULL) {
            diagnostic->functionToken = input->functionToken;
            diagnostic->code = output->functionCount == UINT32_MAX
                                   ? ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW
                                   : ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
        }
        return ZR_FALSE;
    }
    slot = ZrCore_ExecIr_ModuleFunctionAt(output, id);
    contract = slot->contract;
    prepared.id = id;
    prepared.functionToken = input->functionToken;
    prepared.signatureHash = input->signatureHash;
    prepared.contract = contract;
    *slot = prepared;
    return ZR_TRUE;
}
