#include "exec_ir/source/exec_ir_source_finalized_module_internal.h"
#include "exec_ir/exec_ir_gc_map_internal.h"

#include <string.h>
#include <stdlib.h>
#include "zr_vm_core/exec_ir_state_map.h"

TZrBool source_finalized_module_fail(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code,
                                     TZrMetadataToken token) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        diagnostic->functionToken = token;
    }
    return ZR_FALSE;
}

TZrBool source_finalized_module_error(SSourceFinalizedModuleWitness *w, EZrExecutionDiagnosticCode code, TZrUInt32 ev,
                                      TZrUInt32 av, TZrUInt64 eh, TZrUInt64 ah, unsigned role) {
    source_finalized_module_fail(w->diagnostic, code, w->token);
    if (w->diagnostic) {
        w->diagnostic->expectedVersion = ev;
        w->diagnostic->actualVersion = av;
        w->diagnostic->expectedHash = eh;
        w->diagnostic->actualHash = ah;
        if (w->mapped && role < SOURCE_FINALIZED_ROLES) {
            w->diagnostic->instructionId = w->instructionIds[role];
            w->diagnostic->sourceId = w->sourceIds[role];
        }
    }
    return ZR_FALSE;
}
#define FAIL(code) source_finalized_module_error(w, code, 0, 0, 0, 0, SOURCE_FINALIZED_NO_ROLE)
#define REQUIRE(condition, code)                                                                                       \
    do {                                                                                                               \
        if (!(condition))                                                                                              \
            return FAIL(code);                                                                                         \
    } while (0)
#define TYPE(actual, role)                                                                                             \
    do {                                                                                                               \
        if ((actual) != w->facts.returnTypeId)                                                                         \
            return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, w->facts.returnTypeId, \
                                                 (actual), 0, 0, role);                                                \
    } while (0)
static TZrBool span(SSourceFinalizedModuleWitness *w, const void *p, uintmax_t count, uintmax_t capacity, size_t width,
                    size_t alignment) {
    REQUIRE(count <= capacity, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    REQUIRE(capacity <= SIZE_MAX / width, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    REQUIRE((p != ZR_NULL) == (capacity != 0), ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    if (!capacity)
        return ZR_TRUE;
    size_t bytes = (size_t) capacity * width;
    uintptr_t start = (uintptr_t) p;
    REQUIRE(start % alignment == 0, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    REQUIRE(bytes <= UINTPTR_MAX && start <= UINTPTR_MAX - bytes, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    uintptr_t end = start + bytes;
    for (size_t i = 0; i < w->spanCount; ++i)
        REQUIRE(end <= w->spans[i].start || start >= w->spans[i].end, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    if (w->spanCount == w->spanLimit) {
        REQUIRE(w->spanLimit <= SIZE_MAX / 2 / sizeof(*w->spans), ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
        size_t spanCapacity = w->spanLimit * 2;
        SSourceFinalizedSpan *grown = (SSourceFinalizedSpan *)malloc(spanCapacity * sizeof(*grown));
        REQUIRE(grown != ZR_NULL, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY);
        memcpy(grown, w->spans, w->spanCount * sizeof(*grown));
        if (w->spans != w->inlineSpans) free(w->spans);
        w->spans = grown;
        w->spanLimit = spanCapacity;
    }
    w->spans[w->spanCount++] = (SSourceFinalizedSpan) {start, end};
    return ZR_TRUE;
}
static TZrBool array(SSourceFinalizedModuleWitness *w, const SZrArray *a, size_t width, size_t alignment) {
    REQUIRE(a->isValid && a->elementSize == width && a->length <= a->capacity, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    return span(w, a->head, a->length, a->capacity, width, alignment);
}
#define HEADER(p, type)                                                                                                \
    do {                                                                                                               \
        if (!span(w, p, 1, 1, sizeof(type), _Alignof(type)))                                                           \
            return ZR_FALSE;                                                                                           \
    } while (0)
#define POOL(o, field, count, capacity, type)                                                                          \
    do {                                                                                                               \
        if (!span(w, (o)->field, (o)->count, (o)->capacity, sizeof(type), _Alignof(type)))                             \
            return ZR_FALSE;                                                                                           \
    } while (0)
#define ARRAY(p, type)                                                                                                 \
    do {                                                                                                               \
        if (!array(w, p, sizeof(type), _Alignof(type)))                                                                \
            return ZR_FALSE;                                                                                           \
    } while (0)
static TZrBool module_storage(SSourceFinalizedModuleWitness *w, const SZrExecIrModule *m) {
    POOL(m, functions, functionCount, functionCapacity, SZrExecIrFunction);
    POOL(m, constants, constantCount, constantCapacity, SZrExecIrConstant);
    POOL(m, layouts, layoutCount, layoutCapacity, SZrExecIrLayout);
    POOL(m, sourceMaps, sourceMapCount, sourceMapCapacity, SZrExecIrSourceMap);
    return ZR_TRUE;
}
static TZrBool function_storage(SSourceFinalizedModuleWitness *w, const SZrExecIrFunction *f) {
    POOL(f, values, valueCount, valueCapacity, SZrExecIrValue);
    POOL(f, instructions, instructionCount, instructionCapacity, SZrExecIrInstruction);
    POOL(f, blocks, blockCount, blockCapacity, SZrExecIrBlock);
    POOL(f, operandPool, operandCount, operandCapacity, TZrExecIrValueId);
    POOL(f, resultPool, resultCount, resultCapacity, TZrExecIrValueId);
    POOL(f, memoryTokenPool, memoryTokenCount, memoryTokenCapacity, TZrExecIrMemoryTokenId);
    POOL(f, phiPool, phiCount, phiCapacity, SZrExecIrPhi);
    POOL(f, phiIncoming, phiIncomingCount, phiIncomingCapacity, SZrExecIrPhiIncoming);
    POOL(f, predecessors, predecessorCount, predecessorCapacity, TZrExecIrBlockId);
    POOL(f, successors, successorCount, successorCapacity, TZrExecIrBlockId);
    POOL(f, gcRoots, gcRootCount, gcRootCapacity, TZrExecIrValueId);
    POOL(f, deoptStates, deoptStateCount, deoptStateCapacity, SZrExecIrDeoptState);
    POOL(f, deoptValues, deoptValueCount, deoptValueCapacity, TZrExecIrValueId);
    POOL(f, deoptAggregates, deoptAggregateCount, deoptAggregateCapacity, SZrExecIrDeoptAggregate);
    POOL(f, deoptAggregateFields, deoptAggregateFieldCount, deoptAggregateFieldCapacity, SZrExecIrDeoptAggregateField);
    POOL(f, sourceMaps, sourceMapCount, sourceMapCapacity, SZrExecIrSourceMap);
    POOL(f, bindingRows, bindingRowCount, bindingRowCapacity, SZrExecIrBindingRow);
    if (f->frameLayout)
        HEADER(f->frameLayout, SZrExecIrFrameLayout);
    if (f->gcMap)
        HEADER(f->gcMap, SZrExecIrGcMap);
    if (f->stateMap)
        HEADER(f->stateMap, SZrExecIrStateMap);
    REQUIRE(zr_exec_ir_gc_map_outer_shape_is_valid(f), ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    if (f->frameLayout)
        POOL(f->frameLayout, slots, slotCount, slotCapacity, SZrExecIrFrameSlot);
    if (f->gcMap) {
        POOL(f->gcMap, entries, entryCount, entryCapacity, SZrExecIrGcMapEntry);
        POOL(f->gcMap, slotIndexPool, slotIndexCount, slotIndexCapacity, TZrUInt32);
        POOL(f->gcMap, inlineRefOffsetPool, inlineRefOffsetCount, inlineRefOffsetCapacity, TZrUInt32);
    }
    if (f->stateMap) {
        POOL(f->stateMap, entries, entryCount, entryCapacity, SZrExecIrStateMapEntry);
        POOL(f->stateMap, valuePool, valueCount, valueCapacity, TZrExecIrValueId);
        POOL(f->stateMap, rootPool, rootCount, rootCapacity, TZrExecIrValueId);
        POOL(f->stateMap, ownerStatePool, ownerStateCount, ownerStateCapacity, TZrUInt32);
    }
    return ZR_TRUE;
}
TZrBool source_finalized_module_preflight(SSourceFinalizedModuleWitness *w) {
    w->spans = w->inlineSpans;
    w->spanLimit = SOURCE_FINALIZED_INLINE_SPANS;
    source_integer_proof_init(&w->integerProof);
    HEADER(w->semantic, SZrSemanticIrFunction);
    HEADER(w->context, SZrSemanticContext);
    HEADER(w->identity, SZrFunctionSourceCallableIdentity);
    HEADER(w->compilerConstants, SZrArray);
    HEADER(w->original, SZrExecIrModule);
    HEADER(w->destination, SZrExecIrOwnedModule *);
    if (w->diagnostic)
        HEADER(w->diagnostic, SZrExecIrDiagnostic);
    if (!module_storage(w, w->original))
        return ZR_FALSE;
    REQUIRE(w->original->functionCount == 1, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    w->selected = &w->original->functions[0];
    w->token = w->selected->functionToken;
    REQUIRE(w->finalized != w->selected, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    HEADER(w->finalized, SZrExecIrFunction);
    if (!function_storage(w, w->selected) || !function_storage(w, w->finalized))
        return ZR_FALSE;
    ARRAY(&w->context->canonicalTypes, SZrCanonicalTypeNode);
    ARRAY(&w->context->symbols, SZrSemanticSymbolRecord);
    const SZrSemanticIrFunction *s = w->semantic;
    ARRAY(&s->places.places, SZrParserPlace);
    ARRAY(&s->cfg.blocks, SZrParserCfgBlock);
    ARRAY(&s->locals, SZrSemanticIrLocal);
    ARRAY(&s->values, SZrSemanticIrValue);
    ARRAY(&s->instructions, SZrSemanticIrInstruction);
    ARRAY(&s->valueOperands, TZrValueId);
    ARRAY(&s->regions, SZrSemanticIrRegion);
    ARRAY(&s->cleanupScopes, SZrSemanticIrCleanupScope);
    ARRAY(&s->sourceMap, SZrSemanticIrSourceMapEntry);
    ARRAY(&s->loanFacts, SZrSemanticIrLoanFact);
    ARRAY(&s->escapeFacts, SZrSemanticEscapeFact);
    ARRAY(&s->contiguousViewFacts, SZrSemanticContiguousViewFact);
    ARRAY(&s->boundsFacts, SZrSemanticBoundsFact);
    ARRAY(&s->scalarScratchProofs, SZrSemanticIrScalarScratchProof);
    REQUIRE(s->places.places.length != 0 && s->cfg.blocks.length == 1, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    const SZrParserPlace *places = (const SZrParserPlace *) s->places.places.head;
    const SZrParserCfgBlock *blocks = (const SZrParserCfgBlock *) s->cfg.blocks.head;
    for (size_t i = 0; i < s->places.places.length; ++i)
        ARRAY(&places[i].projections, SZrParserPlaceProjection);
    for (size_t i = 0; i < s->cfg.blocks.length; ++i)
        ARRAY(&blocks[i].outgoingEdges, SZrParserCfgEdge);
    ARRAY(w->compilerConstants, SZrTypeValue);
    return span(w, w->functionConstants, w->functionConstantCount, w->functionConstantCount,
                sizeof(*w->functionConstants), _Alignof(SZrTypeValue));
}
void source_finalized_module_release(SSourceFinalizedModuleWitness *w) {
    source_integer_proof_free(&w->integerProof);
    if (w->spans != ZR_NULL && w->spans != w->inlineSpans) free(w->spans);
    w->spans = ZR_NULL;
    w->spanCount = w->spanLimit = 0;
}
TZrBool source_finalized_module_candidate_storage(SSourceFinalizedModuleWitness *w, const SZrExecIrModule *m) {
    HEADER(m, SZrExecIrModule);
    if (!module_storage(w, m))
        return ZR_FALSE;
    REQUIRE(m->functionCount == 1, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    if (!function_storage(w, &m->functions[0]))
        return ZR_FALSE;
    return ZR_TRUE;
}
TZrBool source_finalized_module_metadata(SSourceFinalizedModuleWitness *w, const SZrExecIrFunction *f) {
    REQUIRE(!f->gcMap && !f->gcMapCount && !f->gcMapCapacity && !f->gcRootCount && !f->memoryTokenCount &&
                    !f->phiCount && !f->phiIncomingCount && !f->deoptStateCount && !f->deoptValueCount &&
                    !f->deoptAggregateCount && !f->deoptAggregateFieldCount && !f->bindingRowCount &&
                    f->bindingRowsSchemaVersion == ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    if (f->stateMap) {
        const SZrExecIrStateMap *s = f->stateMap;
        REQUIRE(!s->entryCount && !s->valueCount && !s->rootCount && !s->ownerStateCount &&
                        s->functionToken == f->functionToken && s->signatureHash == f->signatureHash &&
                        s->generation == f->contract.generation,
                ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    }
    return ZR_TRUE;
}

static TZrBool range_equal(SZrFileRange a, SZrFileRange b) {
    return a.source == b.source && a.start.offset == b.start.offset && a.end.offset == b.end.offset &&
           a.start.line == b.start.line && a.start.column == b.start.column && a.end.line == b.end.line &&
           a.end.column == b.end.column;
}
static TZrBool range_valid(SZrFileRange r) {
    return r.start.offset <= UINT32_MAX && r.end.offset <= UINT32_MAX && r.start.offset <= r.end.offset &&
           r.start.line > 0 && r.start.column > 0 && r.end.line > 0 && r.end.column > 0 && r.end.line >= r.start.line &&
           (r.end.line != r.start.line || r.end.column >= r.start.column);
}
static TZrBool contained(SZrFileRange child, SZrFileRange parent) {
    return child.source == parent.source && child.start.offset >= parent.start.offset &&
           child.end.offset <= parent.end.offset;
}
static TZrBool semantic_rows(SSourceFinalizedModuleWitness *w, const SZrAstNode *returned, const SZrAstNode *integer) {
    const SZrSemanticIrFunction *s = w->semantic;
    const SZrSemanticIrInstruction *in = (const SZrSemanticIrInstruction *) s->instructions.head;
    const SZrSemanticIrSourceMapEntry *maps = (const SZrSemanticIrSourceMapEntry *) s->sourceMap.head;
    const SZrSemanticIrValue *value = (const SZrSemanticIrValue *) s->values.head;
    const TZrValueId *operands = (const TZrValueId *) s->valueOperands.head;
    for (unsigned i = 0; i < SOURCE_FINALIZED_ROLES; ++i) {
        const SZrSemanticIrInstruction *p = &in[i];
        REQUIRE(p->id == i + 1, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
        TYPE(p->typeId, SOURCE_FINALIZED_NO_ROLE);
        REQUIRE(maps[i].instructionId == p->id && range_equal(maps[i].sourceRange, p->sourceRange),
                ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
        REQUIRE(!p->scalarConversionTypeToken && !p->matchTypeId && !p->valueId && !p->auxiliaryValueId &&
                        !p->symbolId && !p->accessorSymbolId && !p->constructorId && !p->ownershipOperation &&
                        p->targetBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID && !p->loanId && !p->regionId &&
                        !p->cleanupScopeId && !p->comparisonPredicate && !p->comparisonOperandTypeId &&
                        p->escape == ZR_SEMANTIC_ESCAPE_LOCAL,
                ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
        if (p->operandStart > s->valueOperands.length)
            return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                                 (TZrUInt32) s->valueOperands.length, p->operandStart, 0, 0,
                                                 SOURCE_FINALIZED_NO_ROLE);
        if (p->operandCount > s->valueOperands.length - p->operandStart)
            return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                                                 (TZrUInt32) (s->valueOperands.length - p->operandStart),
                                                 p->operandCount, 0, 0, SOURCE_FINALIZED_NO_ROLE);
        if (p->resultValueId > s->values.length)
            return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, (TZrUInt32) s->values.length,
                                                 p->resultValueId, 0, 0, SOURCE_FINALIZED_NO_ROLE);
    }
    REQUIRE(in[0].opcode == ZR_SEMANTIC_IR_CONSTANT, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    if (!in[0].hasConstantPoolIndex)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 1, 0, 0, 0,
                                             SOURCE_FINALIZED_NO_ROLE);
    REQUIRE(in[0].resultValueId == value->id && !in[0].operandCount && !in[0].placeId,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    REQUIRE(in[1].opcode == ZR_SEMANTIC_IR_PLACE_BASE && in[1].placeId == 1 && !in[1].resultValueId &&
                    !in[1].operandCount && !in[1].hasConstantPoolIndex,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    REQUIRE(in[2].opcode == ZR_SEMANTIC_IR_RETURN && !in[2].placeId && !in[2].resultValueId &&
                    !in[2].hasConstantPoolIndex && !in[2].operandStart && in[2].operandCount == 1 &&
                    operands[0] == in[0].resultValueId,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    REQUIRE(value->id == 1 && value->definitionInstructionId == in[0].id, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    TYPE(value->typeId, SOURCE_FINALIZED_NO_ROLE);
    TYPE(value->facts.typeId, SOURCE_FINALIZED_NO_ROLE);
    REQUIRE(value->facts.ownership == ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE &&
                    value->facts.nullability == ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    REQUIRE(range_equal(in[0].sourceRange, integer->location) && range_equal(in[1].sourceRange, integer->location) &&
                    range_equal(value->sourceRange, integer->location) &&
                    range_equal(in[2].sourceRange, returned->location),
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    return ZR_TRUE;
}
TZrBool source_finalized_module_admit_source(SSourceFinalizedModuleWitness *w) {
    const SZrSemanticIrFunction *s = w->semantic;
    if (!source_callable_query(w->context, s->callableTypeId, w->token, &w->facts, w->diagnostic))
        return ZR_FALSE;
    const SZrCanonicalTypeNode *callable = ZrParser_CanonicalType_Find(w->context, w->facts.callableTypeId);
    REQUIRE(callable && callable->kind == ZR_CANONICAL_TYPE_FUNCTION, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    ARRAY(&callable->data.function.parameterContracts, SZrCanonicalParameterContract);
    const SZrFunctionSourceCallableIdentity *id = w->identity;
#define ID_VERSION(field, expected, code)                                                                              \
    do {                                                                                                               \
        if (id->field != (expected))                                                                                   \
            return source_finalized_module_error(w, code, expected, id->field, 0, 0, SOURCE_FINALIZED_NO_ROLE);        \
    } while (0)
    ID_VERSION(schemaVersion, ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH);
    if (!s->callableTypeId || id->typeId != s->callableTypeId)
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                                             s->callableTypeId ? s->callableTypeId : 1, id->typeId, 0, 0,
                                             SOURCE_FINALIZED_NO_ROLE);
    if (!s->symbolId || id->symbolId != s->symbolId)
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                                             s->symbolId ? s->symbolId : 1, id->symbolId, 0, 0,
                                             SOURCE_FINALIZED_NO_ROLE);
    if (id->canonicalSignatureHash != w->facts.signatureHash)
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0, 0,
                                             w->facts.signatureHash, id->canonicalSignatureHash,
                                             SOURCE_FINALIZED_NO_ROLE);
    ID_VERSION(hasExplicitNoArgsI64, ZR_TRUE, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    ID_VERSION(returnPrimitive, (TZrUInt32) w->facts.returnPrimitive, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    ID_VERSION(parameterCount, 0, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    ID_VERSION(receiverFlags, 0, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
    ID_VERSION(effectFlags, 0, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH);
#undef ID_VERSION
    REQUIRE(s->state && s->state == w->context->state && s->places.state == s->state && s->cfg.state == s->state,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    const SZrSemanticSymbolRecord *symbols = (const SZrSemanticSymbolRecord *) w->context->symbols.head,
                                  *symbol = ZR_NULL;
    size_t matches = 0;
    for (size_t i = 0; i < w->context->symbols.length; ++i)
        if (symbols[i].id == s->symbolId) {
            symbol = &symbols[i];
            ++matches;
        }
    REQUIRE(matches == 1 && symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    if (symbol->typeId != s->callableTypeId)
        return source_finalized_module_error(w, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, s->callableTypeId,
                                             symbol->typeId, 0, 0, SOURCE_FINALIZED_NO_ROLE);
    REQUIRE(!symbol->overloadSetId && symbol->astNode == w->script, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    HEADER(w->script, SZrAstNode);
    REQUIRE(w->script->type == ZR_AST_SCRIPT, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    const SZrAstNodeArray *statements = w->script->data.script.statements;
    REQUIRE(statements != ZR_NULL, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    HEADER(statements, SZrAstNodeArray);
    if (!span(w, statements->nodes, statements->count, statements->capacity, sizeof(*statements->nodes),
              _Alignof(SZrAstNode *)))
        return ZR_FALSE;
    REQUIRE(statements->count == 1, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    const SZrAstNode *returned = statements->nodes[0];
    REQUIRE(returned != ZR_NULL, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    HEADER(returned, SZrAstNode);
    REQUIRE(returned->type == ZR_AST_RETURN_STATEMENT && !returned->data.returnStatement.isReferenceReturn,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    const SZrAstNode *integer = returned->data.returnStatement.expr;
    REQUIRE(integer != ZR_NULL, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    HEADER(integer, SZrAstNode);
    if (integer->type != ZR_AST_INTEGER_LITERAL) {
        SSourceIntegerProof *proof = &w->integerProof;
        if (!source_integer_prove(s, w->context, w->token, proof, w->diagnostic) ||
            !source_integer_check_ast(proof, w->script, w->identity, w->diagnostic) ||
            !source_integer_check_pools(proof, w->compilerConstants, w->functionConstants,
                                        w->functionConstantCount, w->diagnostic)) return ZR_FALSE;
        /* The certified complete tree still participates in cross-input geometry. */
        if (!span(w, proof->scratch, proof->scratchBytes, proof->scratchBytes, 1, 1)) return ZR_FALSE;
        for (TZrUInt32 i = 0; i < proof->valueCount; ++i) {
            const SZrAstNode *node = proof->astByValue[i];
            REQUIRE(node != ZR_NULL, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
            if (node != integer) HEADER(node, SZrAstNode);
        }
        w->integerExpression = ZR_TRUE;
        return ZR_TRUE;
    }
    REQUIRE(range_valid(w->script->location) && range_valid(returned->location) && range_valid(integer->location) &&
                    contained(returned->location, w->script->location) &&
                    contained(integer->location, returned->location) &&
                    range_equal(symbol->location, w->script->location),
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    REQUIRE((TZrUInt64) id->declarationRange.startLine == (TZrUInt64) w->script->location.start.line &&
                    (TZrUInt64) id->declarationRange.startColumn == (TZrUInt64) w->script->location.start.column &&
                    (TZrUInt64) id->declarationRange.endLine == (TZrUInt64) w->script->location.end.line &&
                    (TZrUInt64) id->declarationRange.endColumn == (TZrUInt64) w->script->location.end.column,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    if (s->instructions.length > UINT32_MAX)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0, 0, UINT32_MAX,
                                             s->instructions.length, SOURCE_FINALIZED_NO_ROLE);
    if (s->sourceMap.length > UINT32_MAX)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0, 0, UINT32_MAX,
                                             s->sourceMap.length, SOURCE_FINALIZED_NO_ROLE);
    if (s->sourceMap.length != s->instructions.length)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, (TZrUInt32) s->instructions.length,
                                             (TZrUInt32) s->sourceMap.length, 0, 0, SOURCE_FINALIZED_NO_ROLE);
    REQUIRE(s->instructions.length == 3 && s->values.length == 1 && s->places.places.length == 1 &&
                    s->valueOperands.length == 1 && s->cfg.blocks.length == 1 && !s->cfg.entryBlockId &&
                    !s->cfg.exitBlockId && s->regions.length == 1 && !s->locals.length && !s->cleanupScopes.length &&
                    !s->loanFacts.length && !s->escapeFacts.length && !s->contiguousViewFacts.length &&
                    !s->boundsFacts.length && !s->scalarScratchProofs.length,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    const SZrParserCfgBlock *block = (const SZrParserCfgBlock *) s->cfg.blocks.head;
    REQUIRE(!block->id && block->kind == ZR_PARSER_CFG_BLOCK_ENTRY && !block->statement &&
                    !block->firstInstructionIndex && block->instructionCount == 3 && !block->successorCount &&
                    !block->predecessorCount && !block->outgoingEdges.length &&
                    block->terminatorKind == ZR_PARSER_CFG_TERMINATOR_RETURN,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    const SZrSemanticIrRegion *region = (const SZrSemanticIrRegion *) s->regions.head;
    SZrFileRange empty = {0};
    REQUIRE(region->id == 1 && !region->parentId && region->escapeBound == ZR_SEMANTIC_ESCAPE_FUNCTION &&
                    range_equal(region->sourceRange, empty),
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    const SZrParserPlace *place = (const SZrParserPlace *) s->places.places.head;
    REQUIRE(place->id == 1 && !place->parentId && place->base.kind == ZR_PARSER_PLACE_BASE_TEMPORARY,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    TYPE(place->typeId, SOURCE_FINALIZED_NO_ROLE);
    REQUIRE(!place->projections.length && range_equal(place->sourceRange, integer->location),
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    return semantic_rows(w, returned, integer);
}

static const SZrExecIrSourceMap *find_map(const SZrExecIrFunction *f, TZrUInt32 instruction, unsigned *matches) {
    const SZrExecIrSourceMap *map = ZR_NULL;
    *matches = 0;
    for (TZrUInt32 i = 0; i < f->sourceMapCount; ++i)
        if (f->sourceMaps[i].instructionId == instruction) {
            map = &f->sourceMaps[i];
            ++*matches;
        }
    return map;
}
static TZrBool map_range(const SZrExecIrSourceMap *map, SZrFileRange range) {
    return range_valid(range) && map->startOffset == range.start.offset && map->endOffset == range.end.offset &&
           (TZrUInt64) map->startLine == (TZrUInt64) range.start.line &&
           (TZrUInt64) map->endLine == (TZrUInt64) range.end.line &&
           (TZrUInt64) map->startColumn == (TZrUInt64) range.start.column &&
           (TZrUInt64) map->endColumn == (TZrUInt64) range.end.column;
}
static TZrBool counts(SSourceFinalizedModuleWitness *w, const SZrExecIrFunction *f, TZrUInt32 values) {
    if (f->sourceMapCount != f->instructionCount)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f->instructionCount,
                                             f->sourceMapCount, 0, 0, SOURCE_FINALIZED_NO_ROLE);
    REQUIRE(f->instructionCount == w->semantic->instructions.length && f->blockCount == 1 && f->entryBlockId == 1 &&
                    !f->predecessorCount && !f->successorCount && f->valueCount == values,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    return ZR_TRUE;
}
TZrBool source_finalized_module_mapping(SSourceFinalizedModuleWitness *w) {
    if (w->integerExpression)
        return source_integer_check_function(&w->integerProof, w->selected, SOURCE_INTEGER_ORIGINAL, w->diagnostic);

    uintmax_t values = w->semantic->values.length, places = w->semantic->places.places.length;
    REQUIRE(places <= (UINT32_MAX - values) / 2, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    if (!counts(w, w->selected, (TZrUInt32) (values + 2 * places)) || !counts(w, w->finalized, (TZrUInt32) values))
        return ZR_FALSE;
    const SZrSemanticIrInstruction *semantic = (const SZrSemanticIrInstruction *) w->semantic->instructions.head;
    TZrBool seen[SOURCE_FINALIZED_ROLES] = {0};
    for (unsigned i = 0; i < SOURCE_FINALIZED_ROLES; ++i) {
        TZrUInt32 source = w->selected->instructions[i].sourceId;
        if (!source || source > SOURCE_FINALIZED_ROLES)
            return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, SOURCE_FINALIZED_ROLES, source,
                                                 0, 0, SOURCE_FINALIZED_NO_ROLE);
        unsigned role = source - 1, matches;
        REQUIRE(!seen[role] && semantic[role].id == source, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
        const SZrExecIrSourceMap *map = find_map(w->selected, i + 1, &matches);
        REQUIRE(matches == 1 && map->sourceId == source && map_range(map, semantic[role].sourceRange),
                ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
        seen[role] = ZR_TRUE;
        w->instructionIds[role] = i + 1;
        w->sourceIds[role] = source;
    }
    w->mapped = ZR_TRUE;
    return ZR_TRUE;
}
static TZrBool bound(SSourceFinalizedModuleWitness *w, SZrExecIrRange range, TZrUInt32 count, unsigned role) {
    if (range.offset > count)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, count, range.offset, 0, 0, role);
    if (range.count > count - range.offset)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, count - range.offset, range.count,
                                             0, 0, role);
    return ZR_TRUE;
}
static TZrBool instruction_metadata(SSourceFinalizedModuleWitness *w, const SZrExecIrInstruction *p, unsigned role) {
    REQUIRE(!p->flags && !p->phiRange.count && !p->successorRange.count && !p->memoryIn.count && !p->memoryOut.count &&
                    !p->effectIn && !p->effectOut && !p->deoptId && !p->bindingRow && !p->matchTypeToken,
            ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    TYPE(p->typeToken, role);
    return ZR_TRUE;
}
static TZrBool block_relation(SSourceFinalizedModuleWitness *w, const SZrExecIrBlock *b) {
    const SZrExecIrBlock *a = &w->selected->blocks[0];
    REQUIRE(a->id == 1 && a->flags == ZR_EXEC_IR_BLOCK_FLAG_ENTRY && !a->instructions.offset &&
                    a->instructions.count == 3 && a->terminatorInstructionId == 3 && !a->predecessors.count &&
                    !a->successors.count && !a->phis.count && !a->effectPhiResult && !a->effectPhiIncomings.count,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
#define SAME(field) REQUIRE(a->field == b->field, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE)
    SAME(id);
    SAME(flags);
    SAME(instructions.offset);
    SAME(instructions.count);
    SAME(predecessors.offset);
    SAME(predecessors.count);
    SAME(successors.offset);
    SAME(successors.count);
    SAME(phis.offset);
    SAME(phis.count);
    SAME(effectPhiResult);
    SAME(effectPhiIncomings.offset);
    SAME(effectPhiIncomings.count);
    for (unsigned i = 0; i < ZR_EXEC_IR_MEMORY_CLASS_COUNT; ++i) {
        REQUIRE(!a->memoryPhiResults[i] && !a->memoryPhiIncomings[i].count, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
        SAME(memoryPhiResults[i]);
        SAME(memoryPhiIncomings[i].offset);
        SAME(memoryPhiIncomings[i].count);
    }
    SAME(immediateDominator);
    SAME(terminatorInstructionId);
#undef SAME
    return ZR_TRUE;
}
static TZrBool value_profile(SSourceFinalizedModuleWitness *w, const SZrExecIrValue *v, TZrUInt32 id,
                             TZrUInt32 definition, EZrExecIrNullability nullability, TZrUInt32 flags, unsigned role) {
    if (v->id != id || v->definition != definition)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0, 0, 0, 0, role);
    TYPE(v->typeToken, role);
    if (v->ownership != ZR_EXEC_IR_OWNERSHIP_UNKNOWN || v->nullability != nullability || v->flags != flags)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0, 0, 0, 0, role);
    return ZR_TRUE;
}
TZrBool source_finalized_module_relation(SSourceFinalizedModuleWitness *w, const SZrExecIrFunction *f) {
    if (w->integerExpression) {
        SSourceIntegerProof *proof = &w->integerProof;
        if (!source_finalized_module_metadata(w, f) ||
            !source_integer_check_function(proof, f, SOURCE_INTEGER_COMPACTED, w->diagnostic)) return ZR_FALSE;
        REQUIRE((w->selected->stateMap != ZR_NULL) == (f->stateMap != ZR_NULL), ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
        for (TZrUInt32 i = 0; i < proof->instructionCount; ++i)
            REQUIRE(proof->instructionIds[SOURCE_INTEGER_ORIGINAL][i] ==
                    proof->instructionIds[SOURCE_INTEGER_COMPACTED][i], ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
        return ZR_TRUE;
    }
    if (!counts(w, f, (TZrUInt32) w->semantic->values.length) || !source_finalized_module_metadata(w, f))
        return ZR_FALSE;
    REQUIRE((w->selected->stateMap != ZR_NULL) == (f->stateMap != ZR_NULL), ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    if (!block_relation(w, &f->blocks[0]))
        return ZR_FALSE;
    unsigned provenanceUses = 0;
    const SZrSemanticIrInstruction *semantic = (const SZrSemanticIrInstruction *) w->semantic->instructions.head;
    const SZrSemanticIrValue *retained = (const SZrSemanticIrValue *) w->semantic->values.head;
    const SZrParserPlace *place = (const SZrParserPlace *) w->semantic->places.places.head;
    uintmax_t valueCount = w->semantic->values.length, placeCount = w->semantic->places.places.length;
    REQUIRE(place->id <= UINT32_MAX - valueCount, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    uintmax_t address = valueCount + place->id;
    REQUIRE(placeCount <= UINT32_MAX - address, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
    TZrUInt32 addressId = (TZrUInt32) address, provenanceId = (TZrUInt32) (address + placeCount);
    REQUIRE(addressId && addressId <= w->selected->valueCount && provenanceId &&
                    provenanceId <= w->selected->valueCount,
            ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE);
    for (unsigned role = 0; role < SOURCE_FINALIZED_ROLES; ++role) {
        TZrUInt32 index = w->instructionIds[role] - 1;
        const SZrExecIrInstruction *a = &w->selected->instructions[index], *b = &f->instructions[index];
        if (!instruction_metadata(w, a, role) || !instruction_metadata(w, b, role))
            return ZR_FALSE;
        if (!bound(w, a->operands, w->selected->operandCount, role) ||
            !bound(w, a->results, w->selected->resultCount, role) || !bound(w, b->operands, f->operandCount, role) ||
            !bound(w, b->results, f->resultCount, role))
            return ZR_FALSE;
#define SAME(field)                                                                                                    \
    do {                                                                                                               \
        if (a->field != b->field)                                                                                      \
            return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0, 0, 0, 0, role);            \
    } while (0)
        SAME(phiRange.offset);
        SAME(phiRange.count);
        SAME(successorRange.offset);
        SAME(successorRange.count);
        SAME(memoryIn.offset);
        SAME(memoryIn.count);
        SAME(memoryOut.offset);
        SAME(memoryOut.count);
        SAME(effectIn);
        SAME(effectOut);
        SAME(typeToken);
        SAME(matchTypeToken);
        SAME(layoutId);
        SAME(sourceId);
        SAME(deoptId);
        SAME(bindingRow);
#undef SAME
        TZrBool shape;
        if (role == 0) {
            shape = a->opcode == ZR_EXEC_IR_OPCODE_CONSTANT && b->opcode == a->opcode && !a->operands.count &&
                    !b->operands.count && a->results.count == 1 && b->results.count == 1 &&
                    a->layoutId == semantic[0].constantPoolIndex &&
                    w->selected->resultPool[a->results.offset] == semantic[0].resultValueId &&
                    f->resultPool[b->results.offset] == semantic[0].resultValueId;
        } else if (role == 1) {
            shape = a->opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE && b->opcode == ZR_EXEC_IR_OPCODE_NOP && !a->layoutId &&
                    a->operands.count == 1 && a->results.count == 1 && !b->operands.count && !b->results.count &&
                    w->selected->operandPool[a->operands.offset] == provenanceId &&
                    w->selected->resultPool[a->results.offset] == addressId;
        } else {
            shape = a->opcode == ZR_EXEC_IR_OPCODE_RETURN && b->opcode == a->opcode && !a->layoutId &&
                    !a->results.count && !b->results.count && a->operands.count == 1 && b->operands.count == 1 &&
                    w->selected->operandPool[a->operands.offset] == semantic[0].resultValueId &&
                    f->operandPool[b->operands.offset] == semantic[0].resultValueId;
        }
        if (!shape)
            return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0, 0, 0, 0, role);
        unsigned matches;
        const SZrExecIrSourceMap *map = find_map(f, index + 1, &matches);
        if (matches != 1 || map->sourceId != w->sourceIds[role] || !map_range(map, semantic[role].sourceRange))
            return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0, 0, 0, 0, role);
        for (TZrUInt32 j = 0; j < a->operands.count; ++j) {
            TZrExecIrValueId value = w->selected->operandPool[a->operands.offset + j];
            if (value == addressId)
                return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0, 0, 0, 0, role);
            if (value == provenanceId)
                ++provenanceUses;
        }
    }
    REQUIRE(provenanceUses == 1, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE);
    if (!value_profile(w, &w->selected->values[0], retained->id, w->instructionIds[0], ZR_EXEC_IR_NULLABILITY_NONNULL,
                       0, 0))
        return ZR_FALSE;
    const SZrExecIrValue *a = &w->selected->values[0], *b = &f->values[0];
    TYPE(b->typeToken, 0);
    if (a->id != b->id || a->definition != b->definition || a->typeToken != b->typeToken ||
        a->ownership != b->ownership || a->nullability != b->nullability || a->flags != b->flags)
        return source_finalized_module_error(w, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, 0, 0, 0, 0, 0);
    return value_profile(w, &w->selected->values[addressId - 1], addressId, w->instructionIds[1],
                         ZR_EXEC_IR_NULLABILITY_UNKNOWN, ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS, 1) &&
           value_profile(w, &w->selected->values[provenanceId - 1], provenanceId, 0, ZR_EXEC_IR_NULLABILITY_UNKNOWN,
                         ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY, 1);
}
