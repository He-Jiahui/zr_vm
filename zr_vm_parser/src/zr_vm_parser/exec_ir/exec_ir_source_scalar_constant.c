#include "zr_vm_parser/exec_ir_source_scalar_constant.h"

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include <string.h>

/* This profile has one producer, one inert source place, and one return. */
enum { SOURCE_SCALAR_INSTRUCTIONS = 3 };

static TZrBool scalar_fail(SZrExecIrDiagnostic *d, EZrExecutionDiagnosticCode code,
        TZrUInt32 source, TZrUInt64 expected, TZrUInt64 actual) {
    if (d != ZR_NULL) {
        d->code = code;
        d->sourceId = source;
        d->expectedHash = expected;
        d->actualHash = actual;
    }
    return ZR_FALSE;
}

static TZrBool scalar_span(const void *head, TZrSize count, TZrSize width,
        SZrExecIrDiagnostic *d) {
    uintptr_t start = (uintptr_t)head;
    TZrSize span;
    if (count != 0U && head == ZR_NULL)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0U, 1U, 0U);
    if (count > SIZE_MAX / width)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                0U, SIZE_MAX / width, count);
    span = count * width;
    if ((uintmax_t)span > (uintmax_t)UINTPTR_MAX ||
        (uintptr_t)span > UINTPTR_MAX - start)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
                0U, (TZrUInt64)((UINTPTR_MAX - start) / width), count);
    return ZR_TRUE;
}

static TZrBool scalar_array(const SZrArray *a, TZrSize width, SZrExecIrDiagnostic *d) {
    if (!a->isValid)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0U, 1U, a->isValid);
    if (a->elementSize != width)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0U, width, a->elementSize);
    if (a->length > a->capacity)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0U, a->capacity, a->length);
    return scalar_span(a->head, a->capacity, width, d);
}

static TZrBool scalar_storage(const SZrCompilerState *c, SZrExecIrDiagnostic *d) {
    const SZrSemanticIrFunction *s = &c->preSemanticIr;
    if (c->state == ZR_NULL || c->state->global == ZR_NULL ||
        c->currentFunction == ZR_NULL || c->semanticContext == ZR_NULL)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, 1U, 0U);
    if (!scalar_array(&c->constants, sizeof(SZrTypeValue), d)) return ZR_FALSE;
#define SCALAR_ARRAY(field, type) \
    if (!scalar_array(&s->field, sizeof(type), d)) return ZR_FALSE
    SCALAR_ARRAY(instructions, SZrSemanticIrInstruction);
    SCALAR_ARRAY(values, SZrSemanticIrValue);
    SCALAR_ARRAY(valueOperands, TZrValueId);
    SCALAR_ARRAY(sourceMap, SZrSemanticIrSourceMapEntry);
    SCALAR_ARRAY(cfg.blocks, SZrParserCfgBlock);
    SCALAR_ARRAY(places.places, SZrParserPlace);
#undef SCALAR_ARRAY
    return scalar_array(&c->semanticContext->canonicalTypes, sizeof(SZrCanonicalTypeNode), d);
}

static TZrBool scalar_eligible(const SZrCompilerState *c, SZrExecIrDiagnostic *d) {
    const SZrFunction *f = c->currentFunction;
    if (c->hasError || c->currentAst == ZR_NULL || c->currentAst != c->scriptAst ||
        c->currentAst->type != ZR_AST_SCRIPT || c->currentFunctionNode != ZR_NULL ||
        c->topLevelFunction != ZR_NULL || c->submissionContext != ZR_NULL ||
        c->submissionEntryFunction != ZR_NULL || c->currentModuleKey == ZR_NULL ||
        !c->preSemanticIrInitialized || !c->preSemanticIrValidated ||
        !c->preSemanticIrCfgActive || !c->preSemanticIrCfgTerminated ||
        f->ownerFunction != ZR_NULL || f->childFunctionGraphIsBorrowed || f->moduleVersion != ZR_NULL ||
        f->hasVariableArguments || !f->hasCallableReturnType ||
        f->callableReturnType.baseType != ZR_VALUE_TYPE_INT64 ||
        f->callableReturnType.isNullable || f->callableReturnType.isArray ||
        f->callableReturnType.typeName != ZR_NULL ||
        f->callableReturnType.ownershipQualifier != 0U || !f->hasSourceCallableIdentity ||
        ZrCore_String_GetByteLength(c->currentModuleKey) == 0U)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, 1U, 0U);
#define SCALAR_ZERO(field) \
    if (f->field != 0U) return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, 0U, f->field)
    SCALAR_ZERO(parameterCount);
    SCALAR_ZERO(staticImportLength);
    SCALAR_ZERO(moduleEntryEffectLength);
    SCALAR_ZERO(childFunctionLength);
    SCALAR_ZERO(closureValueLength);
    SCALAR_ZERO(typedClosureBindingLength);
    SCALAR_ZERO(exportedVariableLength);
    SCALAR_ZERO(typedExportedSymbolLength);
    SCALAR_ZERO(exportedCallableSummaryLength);
    SCALAR_ZERO(topLevelCallableBindingLength);
    SCALAR_ZERO(testManifestDataLength);
#undef SCALAR_ZERO
    if (c->closureVars.length != 0U)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, 0U, c->closureVars.length);
    if (f->functionName != c->currentModuleKey)
        return scalar_fail(d, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0U, 1U, 0U);
    if (c->preSemanticIr.state != c->state || c->semanticContext->state != c->state ||
        c->preSemanticIr.places.state != c->state || c->preSemanticIr.cfg.state != c->state)
        return scalar_fail(d, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0U, 1U, 0U);
    return ZR_TRUE;
}

static TZrBool scalar_callable(const SZrCompilerState *c, TZrTypeId *returnType,
        SZrExecIrDiagnostic *d) {
    const SZrSemanticContext *context = c->semanticContext;
    const SZrFunctionSourceCallableIdentity *id = &c->currentFunction->sourceCallableIdentity;
    const SZrCanonicalTypeNode *nodes = (const SZrCanonicalTypeNode *)context->canonicalTypes.head;
    const SZrCanonicalTypeNode *callable, *ret;
    for (TZrSize i = 0U; i < context->canonicalTypes.length; ++i)
        if (nodes[i].id == ZR_SEMANTIC_ID_INVALID || (i != 0U && nodes[i].id <= nodes[i - 1U].id))
            return scalar_fail(d, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                    0U, i != 0U ? (TZrUInt64)nodes[i - 1U].id + 1U : 1U, nodes[i].id);
    if (id->schemaVersion != ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1) {
        if (d != ZR_NULL) {
            d->code = ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH;
            d->expectedVersion = ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1;
            d->actualVersion = id->schemaVersion;
        }
        return ZR_FALSE;
    }
    if (id->typeId == ZR_SEMANTIC_ID_INVALID || id->symbolId == ZR_SEMANTIC_ID_INVALID ||
        id->typeId != c->preSemanticIr.callableTypeId || id->symbolId != c->preSemanticIr.symbolId)
        return scalar_fail(d, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0U, 1U, 0U);
    callable = ZrParser_CanonicalType_Find(context, id->typeId);
    if (callable == ZR_NULL || callable->id != id->typeId ||
        callable->kind != ZR_CANONICAL_TYPE_FUNCTION || callable->structuralHash == 0U)
        return scalar_fail(d, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0U, 1U, 0U);
    if (!scalar_array(&callable->data.function.parameterContracts,
                      sizeof(SZrCanonicalParameterContract), d)) return ZR_FALSE;
    if (callable->data.function.parameterContracts.length != 0U ||
        callable->data.function.receiverEffect != ZR_CANONICAL_RECEIVER_NONE ||
        callable->data.function.effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, 1U, 0U);
    ret = ZrParser_CanonicalType_Find(context, callable->data.function.returnTypeId);
    if (ret == ZR_NULL || ret->id != callable->data.function.returnTypeId ||
        ret->kind != ZR_CANONICAL_TYPE_PRIMITIVE || ret->data.primitive.valueType != ZR_VALUE_TYPE_INT64)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, 1U, 0U);
    if (id->canonicalSignatureHash != callable->structuralHash)
        return scalar_fail(d, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
                0U, callable->structuralHash, id->canonicalSignatureHash);
    if (!id->hasExplicitNoArgsI64 || id->returnPrimitive != ZR_VALUE_TYPE_INT64 ||
        id->parameterCount != 0U || id->receiverFlags != ZR_CANONICAL_RECEIVER_NONE ||
        id->effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE)
        return scalar_fail(d, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH, 0U, 1U, 0U);
    *returnType = ret->id;
    return ZR_TRUE;
}

static TZrBool scalar_same_range(const SZrFileRange *a, const SZrFileRange *b) {
    return (TZrBool)(a->source == b->source && a->start.offset == b->start.offset &&
            a->end.offset == b->end.offset && a->start.line == b->start.line &&
            a->end.line == b->end.line && a->start.column == b->start.column &&
            a->end.column == b->end.column && a->start.offset <= a->end.offset);
}

static TZrBool scalar_shape(const SZrCompilerState *c,
        const SZrSemanticIrInstruction **constant, const SZrSemanticIrInstruction **terminal,
        SZrExecIrDiagnostic *d) {
    const SZrSemanticIrFunction *s = &c->preSemanticIr;
    const SZrSemanticIrInstruction *rows = (const SZrSemanticIrInstruction *)s->instructions.head;
    const SZrSemanticIrValue *values = (const SZrSemanticIrValue *)s->values.head;
    const SZrParserCfgBlock *block;
    const SZrParserPlace *place;
    if (s->instructions.length != SOURCE_SCALAR_INSTRUCTIONS || s->sourceMap.length != s->instructions.length ||
        s->cfg.blocks.length != 1U || s->cfg.entryBlockId != s->cfg.exitBlockId ||
        s->cfg.entryBlockId >= s->cfg.blocks.length)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, 1U, 0U);
    if (s->locals.length != 0U || s->cleanupScopes.length != 0U || s->loanFacts.length != 0U ||
        s->escapeFacts.length != 0U || s->contiguousViewFacts.length != 0U ||
        s->boundsFacts.length != 0U || s->scalarScratchProofs.length != 0U)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, 1U, 0U);
    block = (const SZrParserCfgBlock *)s->cfg.blocks.head;
    if (!scalar_array(&block->outgoingEdges, sizeof(SZrParserCfgEdge), d)) return ZR_FALSE;
    if (block->id != s->cfg.entryBlockId || block->kind != ZR_PARSER_CFG_BLOCK_ENTRY ||
        block->firstInstructionIndex != 0U || block->instructionCount != s->instructions.length ||
        block->outgoingEdges.length != 0U || block->successorCount != 0U || block->predecessorCount != 0U ||
        block->terminatorKind != ZR_PARSER_CFG_TERMINATOR_RETURN)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, 1U, 0U);
    for (TZrSize i = 0U; i < s->instructions.length; ++i) {
        const SZrSemanticIrInstruction *r = &rows[i];
        const SZrSemanticIrSourceMapEntry *map = &((const SZrSemanticIrSourceMapEntry *)s->sourceMap.head)[i];
        if (r->id != i + 1U || map->instructionId != r->id ||
            !scalar_same_range(&map->sourceRange, &r->sourceRange) ||
            r->operandStart > s->valueOperands.length || r->operandCount > s->valueOperands.length - r->operandStart ||
            r->resultValueId > s->values.length || r->valueId != ZR_VALUE_ID_INVALID ||
            r->auxiliaryValueId != ZR_VALUE_ID_INVALID || r->symbolId != ZR_SEMANTIC_ID_INVALID ||
            r->accessorSymbolId != ZR_SEMANTIC_ID_INVALID || r->constructorId != ZR_SEMANTIC_ID_INVALID ||
            r->scalarConversionTypeToken != 0U || r->matchTypeId != ZR_SEMANTIC_ID_INVALID ||
            r->ownershipOperation != ZR_SEMANTIC_OWNERSHIP_NONE ||
            r->targetBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID || r->loanId != ZR_SEMANTIC_LOAN_ID_INVALID ||
            r->regionId != ZR_SEMANTIC_REGION_ID_INVALID || r->cleanupScopeId != ZR_SEMANTIC_CLEANUP_SCOPE_ID_INVALID ||
            r->comparisonPredicate != 0U || r->comparisonOperandTypeId != ZR_SEMANTIC_ID_INVALID ||
            r->escape != ZR_SEMANTIC_ESCAPE_LOCAL)
            return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, r->id, 1U, 0U);
    }
    for (TZrSize i = 0U; i < s->values.length; ++i) {
        const SZrSemanticIrValue *v = &values[i];
        if (v->id != i + 1U || v->definitionInstructionId > s->instructions.length ||
            (v->definitionInstructionId != ZR_SEMANTIC_INSTRUCTION_ID_INVALID &&
             rows[v->definitionInstructionId - 1U].resultValueId != v->id))
            return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, 1U, 0U);
    }
    if (rows[0].opcode != ZR_SEMANTIC_IR_CONSTANT || rows[0].resultValueId == ZR_VALUE_ID_INVALID ||
        rows[0].placeId != ZR_PLACE_ID_INVALID || rows[0].operandCount != 0U ||
        rows[1].opcode != ZR_SEMANTIC_IR_PLACE_BASE || rows[1].operandCount != 0U ||
        rows[1].resultValueId != ZR_VALUE_ID_INVALID ||
        rows[1].hasConstantPoolIndex || rows[2].opcode != ZR_SEMANTIC_IR_RETURN ||
        rows[2].resultValueId != ZR_VALUE_ID_INVALID || rows[2].placeId != ZR_PLACE_ID_INVALID ||
        rows[2].hasConstantPoolIndex || rows[2].operandCount != 1U ||
        values[rows[0].resultValueId - 1U].definitionInstructionId != rows[0].id ||
        rows[1].placeId == ZR_PLACE_ID_INVALID || rows[1].placeId > s->places.places.length)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, 0U, 1U, 0U);
    place = &((const SZrParserPlace *)s->places.places.head)[rows[1].placeId - 1U];
    if (!scalar_array(&place->projections, sizeof(SZrParserPlaceProjection), d)) return ZR_FALSE;
    if (place->id != rows[1].placeId || place->parentId != ZR_PLACE_ID_INVALID ||
        place->base.kind != ZR_PARSER_PLACE_BASE_TEMPORARY || place->projections.length != 0U ||
        place->typeId != rows[1].typeId || !scalar_same_range(&place->sourceRange, &rows[1].sourceRange))
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, rows[1].id, 1U, 0U);
    if (!rows[0].hasConstantPoolIndex)
        return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, rows[0].id, 1U, 0U);
    {
        TZrValueId operand = ((const TZrValueId *)s->valueOperands.head)[rows[2].operandStart];
        if (operand == ZR_VALUE_ID_INVALID || operand > s->values.length)
            return scalar_fail(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                    rows[2].id, rows[0].resultValueId, operand);
    }
    *constant = &rows[0];
    *terminal = &rows[2];
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_MakeSourceScalarConstant(
        const struct SZrCompilerState *compiler, SZrExecIrConstant *output, SZrExecIrDiagnostic *diagnostic) {
    const SZrSemanticIrInstruction *constant, *terminal;
    const SZrSemanticIrValue *result;
    const SZrCanonicalTypeNode *node;
    const SZrTypeValue *sourceValue, *runtimeValue;
    TZrTypeId returnType;
    TZrValueId operand;
    SZrExecIrConstant candidate;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (compiler == ZR_NULL || output == ZR_NULL)
        return scalar_fail(diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, 0U, 0U, 0U);
    if (!scalar_storage(compiler, diagnostic) || !scalar_eligible(compiler, diagnostic) ||
        !scalar_callable(compiler, &returnType, diagnostic) ||
        !scalar_shape(compiler, &constant, &terminal, diagnostic)) return ZR_FALSE;
    if (compiler->constants.length != compiler->currentFunction->constantValueLength)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                0U, compiler->constants.length, compiler->currentFunction->constantValueLength);
    if (compiler->constants.length != 1U)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, 0U, 1U, compiler->constants.length);
    if (constant->constantPoolIndex >= compiler->constants.length)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
                constant->id, compiler->constants.length, constant->constantPoolIndex);
    if (!scalar_span(compiler->currentFunction->constantValueList,
                     compiler->currentFunction->constantValueLength, sizeof(SZrTypeValue), diagnostic)) return ZR_FALSE;
    node = ZrParser_CanonicalType_Find(compiler->semanticContext, constant->typeId);
    if (node == ZR_NULL || node->id != constant->typeId)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, constant->id, 1U, constant->typeId);
    if (node->kind != ZR_CANONICAL_TYPE_PRIMITIVE || node->data.primitive.valueType != ZR_VALUE_TYPE_INT64)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, constant->id, 1U, 0U);
    result = &((const SZrSemanticIrValue *)compiler->preSemanticIr.values.head)[constant->resultValueId - 1U];
    if (constant->typeId != returnType)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, constant->id, returnType, constant->typeId);
    if (result->typeId != returnType)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, constant->id, returnType, result->typeId);
    if (terminal->typeId != returnType)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, terminal->id, returnType, terminal->typeId);
    operand = ((const TZrValueId *)compiler->preSemanticIr.valueOperands.head)[terminal->operandStart];
    if (operand != constant->resultValueId)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, terminal->id, constant->resultValueId, operand);
    sourceValue = &((const SZrTypeValue *)compiler->constants.head)[constant->constantPoolIndex];
    runtimeValue = &compiler->currentFunction->constantValueList[constant->constantPoolIndex];
    if (sourceValue->type != ZR_VALUE_TYPE_INT64)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, constant->id, ZR_VALUE_TYPE_INT64, sourceValue->type);
    if (runtimeValue->type != ZR_VALUE_TYPE_INT64)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, constant->id, ZR_VALUE_TYPE_INT64, runtimeValue->type);
    if (sourceValue->value.nativeObject.nativeInt64 != runtimeValue->value.nativeObject.nativeInt64)
        return scalar_fail(diagnostic, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, constant->id,
                (TZrUInt64)sourceValue->value.nativeObject.nativeInt64,
                (TZrUInt64)runtimeValue->value.nativeObject.nativeInt64);
    memset(&candidate, 0, sizeof(candidate));
    candidate.typeToken = constant->typeId;
    candidate.bits = (TZrUInt64)sourceValue->value.nativeObject.nativeInt64;
    memcpy(output, &candidate, sizeof(candidate));
    return ZR_TRUE;
}
