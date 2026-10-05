#include "compiler_script_callable_identity.h"

#include <string.h>

static TZrBool script_identity_same_range(
        const SZrFileRange *left, const SZrFileRange *right) {
    return (TZrBool)(left->source == right->source &&
            left->start.offset == right->start.offset && left->end.offset == right->end.offset &&
            left->start.line == right->start.line && left->start.column == right->start.column &&
            left->end.line == right->end.line && left->end.column == right->end.column);
}

static TZrBool script_identity_valid_range(const SZrFileRange *range) {
    return (TZrBool)(range->start.line > 0 && range->start.column > 0 &&
            range->end.line >= range->start.line && range->end.column > 0 &&
            range->end.offset >= range->start.offset &&
            (range->end.line != range->start.line ||
             range->end.column >= range->start.column));
}

static TZrBool script_identity_contains_range(
        const SZrFileRange *origin, const SZrFileRange *child) {
    return (TZrBool)(origin->source == child->source &&
            origin->start.offset <= child->start.offset &&
            origin->end.offset >= child->end.offset);
}

static TZrBool script_identity_is_int64(
        const SZrSemanticContext *context, TZrTypeId typeId) {
    const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(context, typeId);
    return (TZrBool)(type != ZR_NULL && type->id == typeId &&
            type->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
            type->data.primitive.valueType == ZR_VALUE_TYPE_INT64);
}

static const SZrAstNode *script_identity_return_node(
        const SZrCompilerState *cs, const SZrFunction *function) {
    const SZrAstNode *script;
    const SZrAstNodeArray *statements;
    const SZrAstNode *returnNode;
    const SZrFunctionTypedTypeRef *returnType;
    if (cs == ZR_NULL || function == ZR_NULL || cs->hasError ||
        cs->state == ZR_NULL || cs->state->global == ZR_NULL ||
        cs->currentAst == ZR_NULL || cs->currentAst != cs->scriptAst ||
        cs->currentAst->type != ZR_AST_SCRIPT || cs->currentFunction != function ||
        cs->currentFunctionNode != ZR_NULL || cs->topLevelFunction != ZR_NULL ||
        cs->submissionContext != ZR_NULL || cs->submissionEntryFunction != ZR_NULL ||
        cs->currentModuleKey == ZR_NULL || ZrCore_String_GetByteLength(cs->currentModuleKey) == 0U ||
        function->functionName != cs->currentModuleKey ||
        cs->semanticContext == ZR_NULL || !cs->preSemanticIrInitialized ||
        !cs->preSemanticIrValidated || !cs->preSemanticIrCfgActive ||
        !cs->preSemanticIrCfgTerminated || cs->preSemanticIrCfgStartupBlocked ||
        cs->preSemanticIrCfgStartupSuppressed || function->hasSourceCallableIdentity ||
        function->moduleVersion != ZR_NULL || function->parameterCount != 0U ||
        function->hasVariableArguments || function->staticImportLength != 0U ||
        function->nativeImportContractLength != 0U || function->moduleEntryEffectLength != 0U ||
        function->childFunctionLength != 0U || cs->childFunctions.length != 0U ||
        function->closureValueLength != 0U || function->typedClosureBindingLength != 0U ||
        cs->closureVars.length != 0U || function->exportedVariableLength != 0U ||
        function->typedExportedSymbolLength != 0U || function->exportedCallableSummaryLength != 0U ||
        function->topLevelCallableBindingLength != 0U || !function->hasCallableReturnType)
        return ZR_NULL;
    returnType = &function->callableReturnType;
    if (returnType->baseType != ZR_VALUE_TYPE_INT64 || returnType->isNullable ||
        returnType->isArray || returnType->typeName != ZR_NULL ||
        returnType->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE)
        return ZR_NULL;
    script = cs->currentAst;
    statements = script->data.script.statements;
    if (statements == ZR_NULL || statements->count != 1U || statements->nodes == ZR_NULL)
        return ZR_NULL;
    returnNode = statements->nodes[0];
    if (returnNode == ZR_NULL || returnNode->type != ZR_AST_RETURN_STATEMENT ||
        returnNode->data.returnStatement.isReferenceReturn ||
        returnNode->data.returnStatement.expr == ZR_NULL ||
        returnNode->data.returnStatement.expr->type != ZR_AST_INTEGER_LITERAL ||
        !script_identity_valid_range(&script->location) ||
        !script_identity_valid_range(&returnNode->location) ||
        !script_identity_valid_range(&returnNode->data.returnStatement.expr->location) ||
        !script_identity_contains_range(&script->location, &returnNode->location) ||
        !script_identity_contains_range(&script->location,
                &returnNode->data.returnStatement.expr->location))
        return ZR_NULL;
    /* The current RETURN AST origin is a point range at the return keyword;
     * retain it exactly instead of fabricating an extent over its expression. */
    return returnNode;
}

/* CONSTANT, its inert temporary PLACE_BASE and RETURN must be the actual
 * validated source producers. The AST supplies only shape and origin; neither
 * its literal value nor ExecBC is used to reconstruct a missing producer. */
static TZrBool script_identity_prove_literal_return(
        const SZrCompilerState *cs, const SZrAstNode *returnNode, TZrTypeId returnTypeId) {
    const SZrSemanticIrFunction *ir = &cs->preSemanticIr;
    const SZrParserCfgBlock *block;
    const SZrSemanticIrInstruction *constant, *placeBase, *terminal;
    const SZrSemanticIrValue *value;
    const TZrValueId *operand;
    const SZrCompilerSemanticIrSlot *slot;
    const SZrParserPlace *place;
    const SZrSemanticIrRegion *region;
    const SZrTypeValue *pooledConstant;
    const SZrFileRange *expressionRange = &returnNode->data.returnStatement.expr->location;
    const SZrFileRange emptyRange = {0};
    if (!script_identity_is_int64(cs->semanticContext, returnTypeId) ||
        ir->symbolId != ZR_SEMANTIC_ID_INVALID || ir->callableTypeId != ZR_SEMANTIC_ID_INVALID ||
        !ir->cfg.blocks.isValid || ir->cfg.blocks.length != 1U ||
        ir->cfg.entryBlockId != 0U || ir->cfg.exitBlockId != 0U ||
        ir->instructions.length != 3U || ir->values.length != 1U ||
        ir->valueOperands.length != 1U || ir->locals.length != 0U ||
        ir->places.places.length != 1U || cs->preSemanticIrSlots.length != 1U ||
        ir->scalarScratchProofs.length != 0U || ir->regions.length != 1U ||
        ir->cleanupScopes.length != 0U || ir->loanFacts.length != 0U ||
        ir->escapeFacts.length != 0U || ir->contiguousViewFacts.length != 0U ||
        ir->boundsFacts.length != 0U || cs->preSemanticIrReceiverLoanIds.length != 0U ||
        !ZrParser_SemanticIr_Validate(ir)) return ZR_FALSE;
    block = (const SZrParserCfgBlock *)ZrCore_Array_Get((SZrArray *)&ir->cfg.blocks, 0U);
    region = (const SZrSemanticIrRegion *)ZrCore_Array_Get((SZrArray *)&ir->regions, 0U);
    if (block == ZR_NULL || block->id != 0U || block->kind != ZR_PARSER_CFG_BLOCK_ENTRY ||
        block->statement != ZR_NULL || block->firstInstructionIndex != 0U ||
        block->instructionCount != 3U || block->successorCount != 0U ||
        block->predecessorCount != 0U || !block->outgoingEdges.isValid ||
        block->outgoingEdges.length != 0U || block->terminatorKind != ZR_PARSER_CFG_TERMINATOR_RETURN ||
        region == ZR_NULL || region->id != 1U || region->parentId != ZR_SEMANTIC_REGION_ID_INVALID ||
        region->escapeBound != ZR_SEMANTIC_ESCAPE_FUNCTION ||
        !script_identity_same_range(&region->sourceRange, &emptyRange)) return ZR_FALSE;
    constant = ZrParser_SemanticIr_InstructionAt(ir, 0U);
    placeBase = ZrParser_SemanticIr_InstructionAt(ir, 1U);
    terminal = ZrParser_SemanticIr_InstructionAt(ir, 2U);
    operand = (const TZrValueId *)ZrCore_Array_Get((SZrArray *)&ir->valueOperands, 0U);
    value = operand != ZR_NULL ? ZrParser_SemanticIr_Value(ir, *operand) : ZR_NULL;
    slot = (const SZrCompilerSemanticIrSlot *)ZrCore_Array_Get(
            (SZrArray *)&cs->preSemanticIrSlots, 0U);
    place = slot != ZR_NULL ? ZrParser_PlaceGraph_Get(&ir->places, slot->placeId) : ZR_NULL;
    if (constant == ZR_NULL || placeBase == ZR_NULL || terminal == ZR_NULL ||
        value == ZR_NULL || slot == ZR_NULL || place == ZR_NULL ||
        constant->id != 1U || constant->opcode != ZR_SEMANTIC_IR_CONSTANT ||
        constant->typeId != returnTypeId || constant->operandCount != 0U ||
        constant->placeId != ZR_PLACE_ID_INVALID || constant->valueId != ZR_VALUE_ID_INVALID ||
        !constant->hasConstantPoolIndex || constant->constantPoolIndex >= cs->constants.length ||
        constant->resultValueId != value->id || value->typeId != returnTypeId ||
        value->definitionInstructionId != constant->id ||
        slot->typeId != returnTypeId || slot->valueId != value->id ||
        slot->symbolId != ZR_SEMANTIC_ID_INVALID || slot->loanId != ZR_SEMANTIC_LOAN_ID_INVALID ||
        slot->regionId != ZR_SEMANTIC_REGION_ID_INVALID || slot->stackSlot == ZR_PARSER_SLOT_NONE ||
        place->typeId != returnTypeId || place->base.kind != ZR_PARSER_PLACE_BASE_TEMPORARY ||
        place->base.identity != slot->stackSlot || place->parentId != ZR_PLACE_ID_INVALID ||
        !place->projections.isValid || place->projections.length != 0U ||
        placeBase->id != 2U || placeBase->opcode != ZR_SEMANTIC_IR_PLACE_BASE ||
        placeBase->typeId != returnTypeId || placeBase->placeId != place->id ||
        placeBase->valueId != ZR_VALUE_ID_INVALID || placeBase->resultValueId != ZR_VALUE_ID_INVALID ||
        placeBase->operandCount != 0U || placeBase->hasConstantPoolIndex ||
        terminal->id != 3U || terminal->opcode != ZR_SEMANTIC_IR_RETURN ||
        terminal->typeId != returnTypeId || terminal->operandStart != 0U || terminal->operandCount != 1U ||
        terminal->placeId != ZR_PLACE_ID_INVALID || terminal->valueId != ZR_VALUE_ID_INVALID ||
        terminal->resultValueId != ZR_VALUE_ID_INVALID || terminal->hasConstantPoolIndex ||
        !script_identity_same_range(&constant->sourceRange, expressionRange) ||
        !script_identity_same_range(&placeBase->sourceRange, expressionRange) ||
        !script_identity_same_range(&place->sourceRange, expressionRange) ||
        !script_identity_same_range(&value->sourceRange, expressionRange) ||
        !script_identity_same_range(&terminal->sourceRange, &returnNode->location)) return ZR_FALSE;
    pooledConstant = (const SZrTypeValue *)ZrCore_Array_Get(
            (SZrArray *)&cs->constants, constant->constantPoolIndex);
    if (pooledConstant == ZR_NULL || pooledConstant->type != ZR_VALUE_TYPE_INT64) return ZR_FALSE;
    for (TZrSize index = 0U; index < ir->instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction = ZrParser_SemanticIr_InstructionAt(ir, index);
        const SZrSemanticIrSourceMapEntry *sourceMap = (const SZrSemanticIrSourceMapEntry *)ZrCore_Array_Get(
                (SZrArray *)&ir->sourceMap, index);
        if (instruction->scalarConversionTypeToken != 0U || instruction->matchTypeId != ZR_SEMANTIC_ID_INVALID ||
            instruction->auxiliaryValueId != ZR_VALUE_ID_INVALID || instruction->symbolId != ZR_SEMANTIC_ID_INVALID ||
            instruction->accessorSymbolId != ZR_SEMANTIC_ID_INVALID || instruction->constructorId != ZR_SEMANTIC_ID_INVALID ||
            instruction->ownershipOperation != ZR_SEMANTIC_OWNERSHIP_NONE ||
            instruction->targetBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID ||
            instruction->loanId != ZR_SEMANTIC_LOAN_ID_INVALID || instruction->regionId != ZR_SEMANTIC_REGION_ID_INVALID ||
            instruction->cleanupScopeId != ZR_SEMANTIC_CLEANUP_SCOPE_ID_INVALID ||
            instruction->comparisonPredicate != 0U || instruction->comparisonOperandTypeId != ZR_SEMANTIC_ID_INVALID ||
            sourceMap == ZR_NULL || sourceMap->instructionId != instruction->id ||
            !script_identity_same_range(&sourceMap->sourceRange, &instruction->sourceRange)) return ZR_FALSE;
    }
    return ZR_TRUE;
}

void compiler_script_callable_identity_try_publish(
        SZrCompilerState *cs, SZrFunction *function, TZrTypeId returnTypeId) {
    const SZrAstNode *returnNode = script_identity_return_node(cs, function);
    const SZrCanonicalTypeNode *type;
    const SZrSemanticSymbolRecord *symbol;
    const SZrFileRange *range;
    TZrTypeId callableTypeId;
    TZrSymbolId symbolId;
    TZrBool functionRootAdded = ZR_FALSE, nameRootAdded = ZR_FALSE;
    SZrFunctionSourceCallableIdentity identity = {0};
    if (returnNode == ZR_NULL ||
        !script_identity_prove_literal_return(cs, returnNode, returnTypeId)) return;
    /* Native semantic arrays borrow names. The prepare caller has attached
     * the actual key to functionName with a barrier; preserve both across this
     * allocation window, without removing roots owned by another caller. */
    if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(cs->state->global, cs->state,
            &function->super, &functionRootAdded)) return;
    if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(cs->state->global, cs->state,
            &cs->currentModuleKey->super, &nameRootAdded)) goto cleanup;
    callableTypeId = ZrParser_CanonicalType_InternFunction(
            cs->semanticContext, ZR_NULL, 0U, returnTypeId,
            ZR_CANONICAL_RECEIVER_NONE, ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    type = ZrParser_CanonicalType_Find(cs->semanticContext, callableTypeId);
    if (callableTypeId == ZR_SEMANTIC_ID_INVALID || type == ZR_NULL || type->id != callableTypeId ||
        type->kind != ZR_CANONICAL_TYPE_FUNCTION || type->structuralHash == 0U ||
        !type->data.function.parameterContracts.isValid || type->data.function.parameterContracts.length != 0U ||
        type->data.function.returnTypeId != returnTypeId ||
        type->data.function.receiverEffect != ZR_CANONICAL_RECEIVER_NONE ||
        type->data.function.effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE ||
        !script_identity_is_int64(cs->semanticContext, returnTypeId)) goto cleanup;
    range = &cs->currentAst->location;
    /* The actual module key is only this internal owner's required
     * symbol name. It never supplies the canonical function structural hash. */
    symbolId = ZrParser_Semantic_RegisterSymbol(cs->semanticContext, cs->currentModuleKey,
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, callableTypeId, ZR_SEMANTIC_ID_INVALID,
            cs->currentAst, *range);
    symbol = ZrParser_Semantic_FindSymbolById(cs->semanticContext, symbolId);
    if (symbolId == ZR_SEMANTIC_ID_INVALID || symbol == ZR_NULL || symbol->id != symbolId ||
        symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION || symbol->typeId != callableTypeId ||
        symbol->name != cs->currentModuleKey || symbol->astNode != cs->currentAst ||
        symbol->overloadSetId != ZR_SEMANTIC_ID_INVALID ||
        !script_identity_same_range(&symbol->location, range)) goto cleanup;
    /* Reacquire the canonical node after symbol registration's allocation. */
    type = ZrParser_CanonicalType_Find(cs->semanticContext, symbol->typeId);
    if (type == ZR_NULL || type->id != callableTypeId || type->kind != ZR_CANONICAL_TYPE_FUNCTION ||
        type->structuralHash == 0U || !type->data.function.parameterContracts.isValid ||
        type->data.function.parameterContracts.length != 0U ||
        type->data.function.returnTypeId != returnTypeId ||
        type->data.function.receiverEffect != ZR_CANONICAL_RECEIVER_NONE ||
        type->data.function.effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE ||
        !script_identity_is_int64(cs->semanticContext, type->data.function.returnTypeId)) goto cleanup;
    identity.schemaVersion = ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1;
    identity.symbolId = symbolId;
    identity.typeId = callableTypeId;
    identity.canonicalSignatureHash = type->structuralHash;
    identity.returnPrimitive = ZR_VALUE_TYPE_INT64;
    identity.parameterCount = (TZrUInt32)type->data.function.parameterContracts.length;
    identity.receiverFlags = (TZrUInt32)type->data.function.receiverEffect;
    identity.effectFlags = type->data.function.effectFlags;
    identity.declarationRange.startLine = (TZrUInt32)range->start.line;
    identity.declarationRange.startColumn = (TZrUInt32)range->start.column;
    identity.declarationRange.endLine = (TZrUInt32)range->end.line;
    identity.declarationRange.endColumn = (TZrUInt32)range->end.column;
    identity.hasExplicitNoArgsI64 = ZR_TRUE;
    /* No fallible gate remains: bind the actual semantic owner and copy only
     * pointer-free provenance to the returned function in this finalize window. */
    cs->preSemanticIr.symbolId = symbolId;
    cs->preSemanticIr.callableTypeId = callableTypeId;
    function->sourceCallableIdentity = identity;
    function->hasSourceCallableIdentity = ZR_TRUE;
cleanup:
    if (nameRootAdded)
        ZrCore_GarbageCollector_UnignoreObject(cs->state->global, &cs->currentModuleKey->super);
    if (functionRootAdded)
        ZrCore_GarbageCollector_UnignoreObject(cs->state->global, &function->super);
}
