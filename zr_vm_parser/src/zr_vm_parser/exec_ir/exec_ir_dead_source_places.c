#include "zr_vm_parser/exec_ir_dead_source_places.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_core/exec_ir_state_map.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    DEAD_SOURCE_LITERAL_INSTRUCTIONS = 3,
    DEAD_SOURCE_LITERAL_VALUES = 1,
    DEAD_SOURCE_LITERAL_PLACES = 1
};

typedef struct SZrDeadSourcePlaceProof {
    TZrExecIrInstructionId placeInstructionId;
    TZrExecIrValueId addressId;
    TZrExecIrValueId provenanceId;
} SZrDeadSourcePlaceProof;

static TZrBool dead_source_fail(
        const SZrExecIrFunction *input, SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code, TZrExecIrInstructionId instructionId) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        diagnostic->functionToken = input != ZR_NULL ? input->functionToken : 0u;
        diagnostic->instructionId = instructionId;
        if (input != ZR_NULL && instructionId != 0u &&
            instructionId <= input->instructionCount && input->instructions != ZR_NULL)
            diagnostic->sourceId = input->instructions[instructionId - 1u].sourceId;
    }
    return ZR_FALSE;
}

static TZrBool dead_source_array_valid(const SZrArray *array, size_t elementSize) {
    return (TZrBool)(array->isValid && array->elementSize == elementSize &&
            array->length <= array->capacity && array->length <= UINT32_MAX &&
            array->capacity <= SIZE_MAX / elementSize &&
            (array->capacity == 0u || array->head != ZR_NULL));
}

static TZrBool dead_source_size_valid(size_t count, size_t elementSize) {
    return (TZrBool)(count <= SIZE_MAX / elementSize);
}

/* Check native storage sizes before any verifier or clone may dereference it. */
static TZrBool dead_source_storage_valid(
        const SZrExecIrFunction *input, SZrExecIrDiagnostic *diagnostic) {
#define CHECK_STORAGE(field, count, capacity) \
    do { \
        if (!dead_source_size_valid(input->capacity, sizeof(*input->field))) \
            return dead_source_fail(input, diagnostic, \
                    ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u); \
        if (input->count > input->capacity || \
            (input->count != 0u && input->field == ZR_NULL)) \
            return dead_source_fail(input, diagnostic, \
                    ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u); \
    } while (0)
    CHECK_STORAGE(values, valueCount, valueCapacity);
    CHECK_STORAGE(instructions, instructionCount, instructionCapacity);
    CHECK_STORAGE(blocks, blockCount, blockCapacity);
    CHECK_STORAGE(operandPool, operandCount, operandCapacity);
    CHECK_STORAGE(resultPool, resultCount, resultCapacity);
    CHECK_STORAGE(memoryTokenPool, memoryTokenCount, memoryTokenCapacity);
    CHECK_STORAGE(phiPool, phiCount, phiCapacity);
    CHECK_STORAGE(phiIncoming, phiIncomingCount, phiIncomingCapacity);
    CHECK_STORAGE(predecessors, predecessorCount, predecessorCapacity);
    CHECK_STORAGE(successors, successorCount, successorCapacity);
    CHECK_STORAGE(gcRoots, gcRootCount, gcRootCapacity);
    CHECK_STORAGE(deoptStates, deoptStateCount, deoptStateCapacity);
    CHECK_STORAGE(deoptValues, deoptValueCount, deoptValueCapacity);
    CHECK_STORAGE(deoptAggregates, deoptAggregateCount, deoptAggregateCapacity);
    CHECK_STORAGE(deoptAggregateFields, deoptAggregateFieldCount, deoptAggregateFieldCapacity);
    CHECK_STORAGE(sourceMaps, sourceMapCount, sourceMapCapacity);
    CHECK_STORAGE(bindingRows, bindingRowCount, bindingRowCapacity);
#undef CHECK_STORAGE
    return ZR_TRUE;
}

/* The 29 spans are the function object, its 17 arrays, three side-table
 * objects and their eight nested arrays. Use allocation capacity, not live
 * count: FreeFunction also frees empty reserved pools. */
enum { DEAD_SOURCE_STORAGE_SPANS = 29 };
typedef struct SZrDeadSourceStorageSpan {
    uintptr_t start;
    size_t bytes;
} SZrDeadSourceStorageSpan;
typedef struct SZrDeadSourceStorage {
    SZrDeadSourceStorageSpan spans[DEAD_SOURCE_STORAGE_SPANS];
    size_t count;
} SZrDeadSourceStorage;

static TZrBool dead_source_spans_overlap(
        const SZrDeadSourceStorageSpan *left,
        const SZrDeadSourceStorageSpan *right) {
    return (TZrBool)(left->start < right->start + right->bytes &&
            right->start < left->start + left->bytes);
}

static TZrBool dead_source_add_storage(
        SZrDeadSourceStorage *storage, const void *pointer,
        size_t capacity, size_t elementSize) {
    SZrDeadSourceStorageSpan span;
    size_t i;
    if (pointer == ZR_NULL) return (TZrBool)(capacity == 0u);
    if (capacity == 0u || !dead_source_size_valid(capacity, elementSize) ||
        storage->count == DEAD_SOURCE_STORAGE_SPANS) return ZR_FALSE;
    span.start = (uintptr_t)pointer;
    span.bytes = capacity * elementSize;
    if (span.bytes > UINTPTR_MAX - span.start) return ZR_FALSE;
    for (i = 0u; i < storage->count; ++i)
        if (dead_source_spans_overlap(&storage->spans[i], &span)) return ZR_FALSE;
    storage->spans[storage->count++] = span;
    return ZR_TRUE;
}

static TZrBool dead_source_top_storage(
        const SZrExecIrFunction *function, SZrDeadSourceStorage *storage) {
    if (!dead_source_add_storage(storage, function, 1u, sizeof(*function))) return ZR_FALSE;
#define OWNED_ARRAY(field, capacity) \
    if (!dead_source_add_storage(storage, function->field, function->capacity, \
                                sizeof(*function->field))) return ZR_FALSE
    OWNED_ARRAY(values, valueCapacity);
    OWNED_ARRAY(instructions, instructionCapacity);
    OWNED_ARRAY(blocks, blockCapacity);
    OWNED_ARRAY(operandPool, operandCapacity);
    OWNED_ARRAY(resultPool, resultCapacity);
    OWNED_ARRAY(memoryTokenPool, memoryTokenCapacity);
    OWNED_ARRAY(phiPool, phiCapacity);
    OWNED_ARRAY(phiIncoming, phiIncomingCapacity);
    OWNED_ARRAY(predecessors, predecessorCapacity);
    OWNED_ARRAY(successors, successorCapacity);
    OWNED_ARRAY(gcRoots, gcRootCapacity);
    OWNED_ARRAY(deoptStates, deoptStateCapacity);
    OWNED_ARRAY(deoptValues, deoptValueCapacity);
    OWNED_ARRAY(deoptAggregates, deoptAggregateCapacity);
    OWNED_ARRAY(deoptAggregateFields, deoptAggregateFieldCapacity);
    OWNED_ARRAY(sourceMaps, sourceMapCapacity);
    OWNED_ARRAY(bindingRows, bindingRowCapacity);
#undef OWNED_ARRAY
#define OWNED_OBJECT(field) \
    if (function->field != ZR_NULL && \
        !dead_source_add_storage(storage, function->field, 1u, \
                                 sizeof(*function->field))) return ZR_FALSE
    OWNED_OBJECT(frameLayout);
    OWNED_OBJECT(gcMap);
    OWNED_OBJECT(stateMap);
#undef OWNED_OBJECT
    return ZR_TRUE;
}

static TZrBool dead_source_nested_storage(
        const SZrExecIrFunction *function, SZrDeadSourceStorage *storage) {
#define NESTED_ARRAY(owner, field, capacity) \
    if (!dead_source_add_storage(storage, function->owner->field, \
            function->owner->capacity, sizeof(*function->owner->field))) return ZR_FALSE
    if (function->frameLayout != ZR_NULL) {
        NESTED_ARRAY(frameLayout, slots, slotCapacity);
    }
    if (function->gcMap != ZR_NULL) {
        NESTED_ARRAY(gcMap, entries, entryCapacity);
        NESTED_ARRAY(gcMap, slotIndexPool, slotIndexCapacity);
        NESTED_ARRAY(gcMap, inlineRefOffsetPool, inlineRefOffsetCapacity);
    }
    if (function->stateMap != ZR_NULL) {
        NESTED_ARRAY(stateMap, entries, entryCapacity);
        NESTED_ARRAY(stateMap, valuePool, valueCapacity);
        NESTED_ARRAY(stateMap, rootPool, rootCapacity);
        NESTED_ARRAY(stateMap, ownerStatePool, ownerStateCapacity);
    }
#undef NESTED_ARRAY
    return ZR_TRUE;
}

static TZrBool dead_source_storage_separate(
        const SZrDeadSourceStorage *left, const SZrDeadSourceStorage *right) {
    size_t i, j;
    for (i = 0u; i < left->count; ++i)
        for (j = 0u; j < right->count; ++j)
            if (dead_source_spans_overlap(&left->spans[i], &right->spans[j])) return ZR_FALSE;
    return ZR_TRUE;
}

static TZrBool dead_source_storage_disjoint(
        const SZrExecIrFunction *input, const SZrExecIrFunction *output) {
    SZrDeadSourceStorage inputStorage = {0}, outputStorage = {0};
    /* Check object overlap before dereferencing any borrowed side-table
     * object. Cross-array interior aliases are rejected before publication. */
    if (!dead_source_top_storage(input, &inputStorage) ||
        !dead_source_top_storage(output, &outputStorage) ||
        !dead_source_storage_separate(&inputStorage, &outputStorage) ||
        !dead_source_nested_storage(input, &inputStorage) ||
        !dead_source_storage_separate(&inputStorage, &outputStorage) ||
        !dead_source_nested_storage(output, &outputStorage)) return ZR_FALSE;
    return dead_source_storage_separate(&inputStorage, &outputStorage);
}

static TZrBool dead_source_same_range(
        const SZrFileRange *left, const SZrFileRange *right) {
    return (TZrBool)(left->source == right->source &&
            left->start.offset == right->start.offset &&
            left->end.offset == right->end.offset &&
            left->start.line == right->start.line &&
            left->start.column == right->start.column &&
            left->end.line == right->end.line &&
            left->end.column == right->end.column);
}

static TZrBool dead_source_range_valid(const SZrFileRange *range) {
    return (TZrBool)(range->start.offset <= UINT32_MAX &&
            range->end.offset <= UINT32_MAX &&
            range->start.offset <= range->end.offset &&
            range->start.line > 0 && range->start.column > 0 &&
            range->end.line >= range->start.line && range->end.column > 0 &&
            (range->end.line != range->start.line ||
             range->end.column >= range->start.column));
}

static TZrBool dead_source_range_contains(
        const SZrFileRange *parent, const SZrFileRange *child) {
    return (TZrBool)(parent->source == child->source &&
            parent->start.offset <= child->start.offset &&
            parent->end.offset >= child->end.offset);
}

static TZrBool dead_source_semantic_storage_valid(
        const SZrSemanticIrFunction *semantic, const SZrSemanticContext *context) {
#define SEM_ARRAY(field, type) \
    if (!dead_source_array_valid(&semantic->field, sizeof(type))) return ZR_FALSE
    SEM_ARRAY(places.places, SZrParserPlace);
    SEM_ARRAY(cfg.blocks, SZrParserCfgBlock);
    SEM_ARRAY(locals, SZrSemanticIrLocal);
    SEM_ARRAY(values, SZrSemanticIrValue);
    SEM_ARRAY(instructions, SZrSemanticIrInstruction);
    SEM_ARRAY(valueOperands, TZrValueId);
    SEM_ARRAY(regions, SZrSemanticIrRegion);
    SEM_ARRAY(cleanupScopes, SZrSemanticIrCleanupScope);
    SEM_ARRAY(sourceMap, SZrSemanticIrSourceMapEntry);
    SEM_ARRAY(loanFacts, SZrSemanticIrLoanFact);
    SEM_ARRAY(escapeFacts, SZrSemanticEscapeFact);
    SEM_ARRAY(contiguousViewFacts, SZrSemanticContiguousViewFact);
    SEM_ARRAY(boundsFacts, SZrSemanticBoundsFact);
    SEM_ARRAY(scalarScratchProofs, SZrSemanticIrScalarScratchProof);
#undef SEM_ARRAY
    return (TZrBool)(semantic->state != ZR_NULL && semantic->state == context->state &&
            semantic->places.state == semantic->state &&
            semantic->cfg.state == semantic->state &&
            dead_source_array_valid(&context->canonicalTypes, sizeof(SZrCanonicalTypeNode)) &&
            dead_source_array_valid(&context->symbols, sizeof(SZrSemanticSymbolRecord)));
}

static TZrBool dead_source_semantic_instruction_inert(
        const SZrSemanticIrInstruction *instruction) {
    return (TZrBool)(instruction->scalarConversionTypeToken == 0u &&
            instruction->matchTypeId == ZR_SEMANTIC_ID_INVALID &&
            instruction->valueId == ZR_VALUE_ID_INVALID &&
            instruction->auxiliaryValueId == ZR_VALUE_ID_INVALID &&
            instruction->symbolId == ZR_SEMANTIC_ID_INVALID &&
            instruction->accessorSymbolId == ZR_SEMANTIC_ID_INVALID &&
            instruction->constructorId == ZR_SEMANTIC_ID_INVALID &&
            instruction->ownershipOperation == ZR_SEMANTIC_OWNERSHIP_NONE &&
            instruction->targetBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID &&
            instruction->loanId == ZR_SEMANTIC_LOAN_ID_INVALID &&
            instruction->regionId == ZR_SEMANTIC_REGION_ID_INVALID &&
            instruction->cleanupScopeId == ZR_SEMANTIC_CLEANUP_SCOPE_ID_INVALID &&
            instruction->comparisonPredicate == 0u &&
            instruction->comparisonOperandTypeId == ZR_SEMANTIC_ID_INVALID &&
            instruction->escape == ZR_SEMANTIC_ESCAPE_LOCAL);
}

/* This finite admission independently proves the actual literal SCRIPT, whose
 * temporary has no projections, aliases, locals, views, loans or identity peer.
 * The AST is used only for origin/shape, never to synthesize constant bits. */
static TZrBool dead_source_prove_semantic_literal(
        const SZrSemanticIrFunction *semantic, const SZrSemanticContext *context,
        const SZrExecIrFunction *input, SZrExecIrDiagnostic *diagnostic) {
    const SZrSemanticSymbolRecord *symbol;
    const SZrCanonicalTypeNode *callable, *type;
    const SZrAstNode *script, *terminalAst, *literal;
    const SZrSemanticIrInstruction *constant, *base, *terminal;
    const SZrSemanticIrValue *value;
    const SZrParserPlace *place;
    const SZrParserCfgBlock *block;
    const SZrSemanticIrRegion *region;
    const SZrFileRange emptyRange = {0};
    TZrUInt32 i;
    if (!dead_source_semantic_storage_valid(semantic, context) ||
        semantic->instructions.length != DEAD_SOURCE_LITERAL_INSTRUCTIONS ||
        semantic->values.length != DEAD_SOURCE_LITERAL_VALUES ||
        semantic->places.places.length != DEAD_SOURCE_LITERAL_PLACES ||
        semantic->valueOperands.length != 1u || semantic->cfg.blocks.length != 1u ||
        semantic->cfg.entryBlockId != 0u || semantic->cfg.exitBlockId != 0u ||
        semantic->sourceMap.length != semantic->instructions.length ||
        semantic->regions.length != 1u || semantic->locals.length != 0u ||
        semantic->cleanupScopes.length != 0u || semantic->loanFacts.length != 0u ||
        semantic->escapeFacts.length != 0u || semantic->contiguousViewFacts.length != 0u ||
        semantic->boundsFacts.length != 0u || semantic->scalarScratchProofs.length != 0u)
        goto unsupported;
    if (semantic->symbolId == ZR_SEMANTIC_ID_INVALID ||
        semantic->callableTypeId == ZR_SEMANTIC_ID_INVALID) goto unsupported;
    symbol = ZrParser_Semantic_FindSymbolById(context, semantic->symbolId);
    callable = ZrParser_CanonicalType_Find(context, semantic->callableTypeId);
    if (symbol == ZR_NULL || callable == ZR_NULL ||
        symbol->id != semantic->symbolId || symbol->typeId != semantic->callableTypeId ||
        symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        symbol->overloadSetId != ZR_SEMANTIC_ID_INVALID ||
        symbol->astNode == ZR_NULL || symbol->astNode->type != ZR_AST_SCRIPT ||
        callable->id != semantic->callableTypeId || callable->kind != ZR_CANONICAL_TYPE_FUNCTION ||
        callable->structuralHash == 0u || callable->structuralHash != input->signatureHash ||
        !dead_source_array_valid(&callable->data.function.parameterContracts,
                                sizeof(SZrCanonicalParameterContract)) ||
        callable->data.function.parameterContracts.length != 0u ||
        callable->data.function.receiverEffect != ZR_CANONICAL_RECEIVER_NONE ||
        callable->data.function.effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE ||
        input->contract.targetToken != input->functionToken ||
        input->contract.signatureHash != input->signatureHash)
        goto unsupported;
    type = ZrParser_CanonicalType_Find(context, callable->data.function.returnTypeId);
    if (type == ZR_NULL || type->id != callable->data.function.returnTypeId ||
        type->kind != ZR_CANONICAL_TYPE_PRIMITIVE ||
        type->data.primitive.valueType != ZR_VALUE_TYPE_INT64) goto unsupported;
    script = symbol->astNode;
    if (!dead_source_range_valid(&script->location) ||
        !dead_source_same_range(&symbol->location, &script->location) ||
        script->data.script.statements == ZR_NULL ||
        script->data.script.statements->count != 1u ||
        script->data.script.statements->nodes == ZR_NULL) goto unsupported;
    terminalAst = script->data.script.statements->nodes[0];
    if (terminalAst == ZR_NULL || terminalAst->type != ZR_AST_RETURN_STATEMENT ||
        terminalAst->data.returnStatement.isReferenceReturn ||
        terminalAst->data.returnStatement.expr == ZR_NULL) goto unsupported;
    literal = terminalAst->data.returnStatement.expr;
    if (literal->type != ZR_AST_INTEGER_LITERAL ||
        !dead_source_range_valid(&terminalAst->location) ||
        !dead_source_range_valid(&literal->location) ||
        !dead_source_range_contains(&script->location, &terminalAst->location) ||
        !dead_source_range_contains(&terminalAst->location, &literal->location)) goto unsupported;
    block = (const SZrParserCfgBlock *)ZrCore_Array_Get((SZrArray *)&semantic->cfg.blocks, 0u);
    region = (const SZrSemanticIrRegion *)ZrCore_Array_Get((SZrArray *)&semantic->regions, 0u);
    place = ZrParser_PlaceGraph_Get(&semantic->places, 1u);
    if (block->id != 0u || block->kind != ZR_PARSER_CFG_BLOCK_ENTRY ||
        block->statement != ZR_NULL || block->firstInstructionIndex != 0u ||
        block->instructionCount != semantic->instructions.length ||
        block->successorCount != 0u || block->predecessorCount != 0u ||
        !dead_source_array_valid(&block->outgoingEdges, sizeof(SZrParserCfgEdge)) ||
        block->outgoingEdges.length != 0u ||
        block->terminatorKind != ZR_PARSER_CFG_TERMINATOR_RETURN ||
        region->id != 1u || region->parentId != ZR_SEMANTIC_REGION_ID_INVALID ||
        region->escapeBound != ZR_SEMANTIC_ESCAPE_FUNCTION ||
        !dead_source_same_range(&region->sourceRange, &emptyRange) ||
        place == ZR_NULL || place->id != 1u ||
        place->parentId != ZR_PLACE_ID_INVALID ||
        place->base.kind != ZR_PARSER_PLACE_BASE_TEMPORARY || place->typeId != type->id ||
        !dead_source_array_valid(&place->projections, sizeof(SZrParserPlaceProjection)) ||
        place->projections.length != 0u ||
        !dead_source_same_range(&place->sourceRange, &literal->location)) goto unsupported;
    constant = ZrParser_SemanticIr_InstructionAt(semantic, 0u);
    base = ZrParser_SemanticIr_InstructionAt(semantic, 1u);
    terminal = ZrParser_SemanticIr_InstructionAt(semantic, 2u);
    value = ZrParser_SemanticIr_Value(semantic, 1u);
    if (constant->opcode != ZR_SEMANTIC_IR_CONSTANT ||
        constant->resultValueId != value->id || constant->operandCount != 0u ||
        constant->placeId != ZR_PLACE_ID_INVALID || !constant->hasConstantPoolIndex ||
        base->opcode != ZR_SEMANTIC_IR_PLACE_BASE || base->placeId != place->id ||
        base->resultValueId != ZR_VALUE_ID_INVALID || base->operandCount != 0u ||
        base->hasConstantPoolIndex || terminal->opcode != ZR_SEMANTIC_IR_RETURN ||
        terminal->placeId != ZR_PLACE_ID_INVALID ||
        terminal->resultValueId != ZR_VALUE_ID_INVALID || terminal->hasConstantPoolIndex ||
        terminal->operandStart != 0u || terminal->operandCount != 1u ||
        *(const TZrValueId *)ZrCore_Array_Get((SZrArray *)&semantic->valueOperands, 0u) != value->id ||
        value->id != 1u || value->definitionInstructionId != constant->id ||
        value->typeId != type->id ||
        value->facts.typeId != type->id ||
        value->facts.ownership != ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE ||
        value->facts.nullability != ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL ||
        !dead_source_same_range(&constant->sourceRange, &literal->location) ||
        !dead_source_same_range(&base->sourceRange, &literal->location) ||
        !dead_source_same_range(&value->sourceRange, &literal->location) ||
        !dead_source_same_range(&terminal->sourceRange, &terminalAst->location)) goto unsupported;
    for (i = 0u; i < semantic->instructions.length; ++i) {
        const SZrSemanticIrInstruction *instruction = ZrParser_SemanticIr_InstructionAt(semantic, i);
        const SZrSemanticIrSourceMapEntry *map = (const SZrSemanticIrSourceMapEntry *)
                ZrCore_Array_Get((SZrArray *)&semantic->sourceMap, i);
        if (instruction->id != i + 1u || instruction->typeId != type->id ||
            !dead_source_semantic_instruction_inert(instruction) ||
            map->instructionId != instruction->id ||
            !dead_source_same_range(&map->sourceRange, &instruction->sourceRange)) goto unsupported;
    }
    if (!ZrParser_SemanticIr_Validate(semantic)) goto unsupported;
    return ZR_TRUE;
unsupported:
    return dead_source_fail(input, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
}

static TZrBool dead_source_metadata_supported(
        const SZrExecIrFunction *input, SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 i, region;
    if (input->sealed)
        return dead_source_fail(input, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_SEALED, 0u);
    if (input->frameLayout != ZR_NULL || input->gcMap != ZR_NULL ||
        input->gcMapCount != 0u || input->gcRootCount != 0u ||
        input->deoptStateCount != 0u || input->deoptValueCount != 0u ||
        input->deoptAggregateCount != 0u || input->deoptAggregateFieldCount != 0u ||
        input->bindingRowCount != 0u || input->phiCount != 0u ||
        input->phiIncomingCount != 0u || input->memoryTokenCount != 0u ||
        (input->stateMap != ZR_NULL &&
         (input->stateMap->entryCount != 0u || input->stateMap->valueCount != 0u ||
          input->stateMap->rootCount != 0u || input->stateMap->ownerStateCount != 0u)))
        goto unsupported;
    for (i = 0u; i < input->blockCount; ++i) {
        const SZrExecIrBlock *block = &input->blocks[i];
        if (block->phis.count != 0u || block->effectPhiResult != 0u ||
            block->effectPhiIncomings.count != 0u) goto unsupported;
        for (region = 0u; region < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++region)
            if (block->memoryPhiResults[region] != 0u ||
                block->memoryPhiIncomings[region].count != 0u) goto unsupported;
    }
    return ZR_TRUE;
unsupported:
    return dead_source_fail(input, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
}

static TZrBool dead_source_instruction_inert(const SZrExecIrInstruction *instruction) {
    return (TZrBool)(instruction->flags == 0u && instruction->phiRange.count == 0u &&
            instruction->successorRange.count == 0u && instruction->memoryIn.count == 0u &&
            instruction->memoryOut.count == 0u && instruction->effectIn == 0u &&
            instruction->effectOut == 0u && instruction->deoptId == 0u &&
            instruction->bindingRow == 0u && instruction->matchTypeToken == 0u);
}

static TZrBool dead_source_map_matches(
        const SZrExecIrSourceMap *map, const SZrSemanticIrInstruction *source,
        TZrExecIrInstructionId instructionId) {
    const SZrFileRange *range = &source->sourceRange;
    return (TZrBool)(map->instructionId == instructionId && map->sourceId == source->id &&
            map->startOffset == range->start.offset && map->endOffset == range->end.offset &&
            map->startLine == (TZrUInt32)range->start.line &&
            map->startColumn == (TZrUInt32)range->start.column &&
            map->endLine == (TZrUInt32)range->end.line &&
            map->endColumn == (TZrUInt32)range->end.column);
}

static TZrBool dead_source_prove_exec_literal(
        const SZrSemanticIrFunction *semantic, const SZrExecIrFunction *input,
        SZrDeadSourcePlaceProof *proof, SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrInstructionId definitions[DEAD_SOURCE_LITERAL_INSTRUCTIONS] = {0};
    const SZrExecIrInstruction *constant, *base, *terminal;
    const SZrSemanticIrInstruction *sourceConstant = ZrParser_SemanticIr_InstructionAt(semantic, 0u);
    const SZrSemanticIrInstruction *sourceBase = ZrParser_SemanticIr_InstructionAt(semantic, 1u);
    TZrUInt32 i, j, provenanceUses = 0u;
    TZrBool tombstone;
    if (input->instructionCount != semantic->instructions.length || input->blockCount != 1u ||
        input->sourceMapCount != input->instructionCount || input->entryBlockId != 1u ||
        input->blocks[0].flags != ZR_EXEC_IR_BLOCK_FLAG_ENTRY ||
        input->blocks[0].instructionRange.start != 0u ||
        input->blocks[0].instructionRange.count != input->instructionCount ||
        input->blocks[0].terminatorInstructionId != input->instructionCount ||
        input->predecessorCount != 0u || input->successorCount != 0u) goto unsupported;
    for (i = 0u; i < input->instructionCount; ++i) {
        const SZrExecIrInstruction *instruction = &input->instructions[i];
        const SZrSemanticIrInstruction *source;
        const SZrExecIrSourceMap *map = ZR_NULL;
        if (instruction->sourceId == 0u || instruction->sourceId > semantic->instructions.length ||
            definitions[instruction->sourceId - 1u] != 0u) goto unsupported;
        definitions[instruction->sourceId - 1u] = i + 1u;
        source = ZrParser_SemanticIr_InstructionAt(semantic, instruction->sourceId - 1u);
        for (j = 0u; j < input->sourceMapCount; ++j) {
            if (input->sourceMaps[j].instructionId != i + 1u) continue;
            if (map != ZR_NULL) goto unsupported;
            map = &input->sourceMaps[j];
        }
        if (map == ZR_NULL || !dead_source_map_matches(map, source, i + 1u) ||
            instruction->typeToken != source->typeId ||
            !dead_source_instruction_inert(instruction)) goto unsupported;
    }
    constant = &input->instructions[definitions[0] - 1u];
    base = &input->instructions[definitions[1] - 1u];
    terminal = &input->instructions[definitions[2] - 1u];
    tombstone = (TZrBool)(base->opcode == ZR_EXEC_IR_OPCODE_NOP);
    if (constant->opcode != ZR_EXEC_IR_OPCODE_CONSTANT || constant->operands.count != 0u ||
        constant->results.count != 1u || constant->layoutId != sourceConstant->constantPoolIndex ||
        input->resultPool[constant->results.start] != sourceConstant->resultValueId ||
        terminal->opcode != ZR_EXEC_IR_OPCODE_RETURN || terminal->layoutId != 0u ||
        terminal->results.count != 0u || terminal->operands.count != 1u ||
        input->operandPool[terminal->operands.start] != sourceConstant->resultValueId ||
        base->layoutId != 0u ||
        input->valueCount != semantic->values.length +
                (tombstone ? 0u : 2u * (TZrUInt32)semantic->places.places.length)) goto unsupported;
    if (input->values[0].id != sourceConstant->resultValueId ||
        input->values[0].definition != definitions[0] ||
        input->values[0].typeToken != sourceConstant->typeId || input->values[0].flags != 0u ||
        input->values[0].ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
        input->values[0].nullability != ZR_EXEC_IR_NULLABILITY_NONNULL) goto unsupported;
    proof->placeInstructionId = definitions[1];
    proof->addressId = proof->provenanceId = ZR_EXEC_IR_VALUE_ID_INVALID;
    if (tombstone) {
        if (base->operands.count != 0u || base->results.count != 0u) goto unsupported;
        return ZR_TRUE;
    }
    if (base->opcode != ZR_EXEC_IR_OPCODE_PLACE_BASE ||
        base->operands.count != 1u || base->results.count != 1u) goto unsupported;
    proof->addressId = (TZrUInt32)semantic->values.length + sourceBase->placeId;
    proof->provenanceId = (TZrUInt32)semantic->values.length +
            (TZrUInt32)semantic->places.places.length + sourceBase->placeId;
    if (input->resultPool[base->results.start] != proof->addressId ||
        input->operandPool[base->operands.start] != proof->provenanceId) goto unsupported;
    {
        const SZrExecIrValue *address = &input->values[proof->addressId - 1u];
        const SZrExecIrValue *provenance = &input->values[proof->provenanceId - 1u];
        if (address->id != proof->addressId || address->definition != proof->placeInstructionId ||
            address->typeToken != sourceBase->typeId ||
            address->flags != ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS ||
            address->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            address->nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN ||
            provenance->id != proof->provenanceId || provenance->definition != 0u ||
            provenance->typeToken != sourceBase->typeId ||
            provenance->flags != ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY ||
            provenance->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN ||
            provenance->nullability != ZR_EXEC_IR_NULLABILITY_UNKNOWN) goto unsupported;
    }
    /* Scan actual live slices; unused pool garbage is discarded during rebuild. */
    for (i = 0u; i < input->instructionCount; ++i) {
        const SZrExecIrInstruction *instruction = &input->instructions[i];
        for (j = 0u; j < instruction->operands.count; ++j) {
            TZrExecIrValueId valueId = input->operandPool[instruction->operands.start + j];
            if (valueId == proof->addressId) goto unsupported;
            if (valueId != proof->provenanceId) continue;
            if (i + 1u != proof->placeInstructionId || provenanceUses == UINT32_MAX) goto unsupported;
            ++provenanceUses;
        }
    }
    if (provenanceUses != 1u) goto unsupported;
    return ZR_TRUE;
unsupported:
    return dead_source_fail(input, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0u);
}

static TZrBool dead_source_compact(
        SZrExecIrFunction *candidate, const SZrDeadSourcePlaceProof *proof,
        SZrExecIrDiagnostic *diagnostic) {
    TZrExecIrValueId *map = ZR_NULL, *operands = ZR_NULL, *results = ZR_NULL;
    SZrExecIrValue *values = ZR_NULL;
    TZrUInt32 i, j, valueCount = 0u, operandCount = 0u, resultCount = 0u;
    size_t mapCount;
    TZrBool success = ZR_FALSE;
    if (candidate->valueCount == UINT32_MAX)
        return dead_source_fail(candidate, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u);
    mapCount = (size_t)candidate->valueCount + 1u;
    if (!dead_source_size_valid(mapCount, sizeof(*map)))
        return dead_source_fail(candidate, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u);
    map = (TZrExecIrValueId *)calloc(mapCount, sizeof(*map));
    if (map == ZR_NULL) goto oom;
    for (i = 0u; i < candidate->valueCount; ++i) {
        TZrExecIrValueId id = i + 1u;
        if (id == proof->addressId || id == proof->provenanceId) continue;
        map[id] = ++valueCount;
    }
    if (!dead_source_size_valid(valueCount, sizeof(*values))) goto overflow;
    if (valueCount != 0u) {
        values = (SZrExecIrValue *)malloc((size_t)valueCount * sizeof(*values));
        if (values == ZR_NULL) goto oom;
    }
    for (i = 0u; i < candidate->instructionCount; ++i) {
        const SZrExecIrInstruction *instruction = &candidate->instructions[i];
        if (i + 1u == proof->placeInstructionId) continue;
        if (instruction->operands.count > UINT32_MAX - operandCount ||
            instruction->results.count > UINT32_MAX - resultCount) goto overflow;
        operandCount += instruction->operands.count;
        resultCount += instruction->results.count;
    }
    if (!dead_source_size_valid(operandCount, sizeof(*operands)) ||
        !dead_source_size_valid(resultCount, sizeof(*results))) goto overflow;
    if (operandCount != 0u) operands = (TZrExecIrValueId *)malloc((size_t)operandCount * sizeof(*operands));
    if (resultCount != 0u) results = (TZrExecIrValueId *)malloc((size_t)resultCount * sizeof(*results));
    if ((operandCount != 0u && operands == ZR_NULL) ||
        (resultCount != 0u && results == ZR_NULL)) goto oom;
    operandCount = resultCount = 0u;
    for (i = 0u; i < candidate->instructionCount; ++i) {
        SZrExecIrInstruction *instruction = &candidate->instructions[i];
        SZrExecIrRange oldOperands = instruction->operands, oldResults = instruction->results;
        instruction->operands.start = operandCount;
        instruction->results.start = resultCount;
        if (i + 1u == proof->placeInstructionId) {
            instruction->opcode = ZR_EXEC_IR_OPCODE_NOP;
            instruction->operands.count = instruction->results.count = 0u;
            continue;
        }
        for (j = 0u; j < oldOperands.count; ++j) {
            TZrExecIrValueId oldId = candidate->operandPool[oldOperands.start + j];
            if (oldId == 0u || oldId > candidate->valueCount || map[oldId] == 0u) goto invalid;
            operands[operandCount++] = map[oldId];
        }
        for (j = 0u; j < oldResults.count; ++j) {
            TZrExecIrValueId oldId = candidate->resultPool[oldResults.start + j];
            if (oldId == 0u || oldId > candidate->valueCount || map[oldId] == 0u) goto invalid;
            results[resultCount++] = map[oldId];
        }
    }
    for (i = 0u; i < candidate->valueCount; ++i) {
        TZrExecIrValueId newId = map[i + 1u];
        if (newId == 0u) continue;
        values[newId - 1u] = candidate->values[i];
        values[newId - 1u].id = newId;
    }
    free(candidate->values);
    free(candidate->operandPool);
    free(candidate->resultPool);
    candidate->values = values;
    candidate->valueCount = candidate->valueCapacity = valueCount;
    candidate->operandPool = operands;
    candidate->resultPool = results;
    candidate->operandCount = candidate->operandCapacity = operandCount;
    candidate->resultCount = candidate->resultCapacity = resultCount;
    operands = results = ZR_NULL;
    values = ZR_NULL;
    success = ZR_TRUE;
    goto cleanup;
oom:
    (void)dead_source_fail(candidate, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, 0u);
    goto cleanup;
overflow:
    (void)dead_source_fail(candidate, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, 0u);
    goto cleanup;
invalid:
    (void)dead_source_fail(candidate, diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, i + 1u);
cleanup:
    free(map);
    free(operands);
    free(results);
    free(values);
    return success;
}

TZrBool ZrParser_ExecIr_EliminateDeadSourcePlaces(
        const SZrSemanticIrFunction *semantic,
        const SZrSemanticContext *context,
        const SZrExecIrFunction *input,
        SZrExecIrFunction *output,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrFunction candidate;
    SZrDeadSourcePlaceProof proof;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (semantic == ZR_NULL || context == ZR_NULL || input == ZR_NULL ||
        output == ZR_NULL || input == output)
        return dead_source_fail(input, diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    if (!dead_source_storage_valid(input, diagnostic)) return ZR_FALSE;
    if (!dead_source_storage_disjoint(input, output))
        return dead_source_fail(input, diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0u);
    /* Preserve the core verifier's actual diagnostic for malformed input. */
    if (!ZrCore_ExecIr_VerifyFunction(input, ZR_EXEC_IR_VERIFY_ALL, diagnostic) ||
        !dead_source_metadata_supported(input, diagnostic) ||
        !dead_source_prove_semantic_literal(semantic, context, input, diagnostic) ||
        !dead_source_prove_exec_literal(semantic, input, &proof, diagnostic)) return ZR_FALSE;
    ZrCore_ExecIr_FunctionInit(&candidate);
    if (!ZrCore_ExecIr_CloneFunction(input, &candidate, diagnostic)) return ZR_FALSE;
    if (!dead_source_compact(&candidate, &proof, diagnostic) ||
        !ZrCore_ExecIr_VerifyFunction(&candidate, ZR_EXEC_IR_VERIFY_ALL, diagnostic)) {
        ZrCore_ExecIr_FreeFunction(&candidate);
        return ZR_FALSE;
    }
    ZrCore_ExecIr_FreeFunction(output);
    *output = candidate;
    return ZR_TRUE;
}
