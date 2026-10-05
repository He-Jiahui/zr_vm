#include "compiler_source_callable_identity.h"

#include <string.h>

static TZrBool source_identity_same_range(const SZrFileRange *left, const SZrFileRange *right) {
    return (TZrBool)(left->source == right->source &&
            left->start.offset == right->start.offset && left->end.offset == right->end.offset &&
            left->start.line == right->start.line && left->start.column == right->start.column &&
            left->end.line == right->end.line && left->end.column == right->end.column);
}

static const SZrFunctionTypeInfo *source_identity_find_declaration(
        const SZrCompilerState *cs, const SZrAstNode *node) {
    const SZrTypeEnvironment *environment;
    const SZrFunctionTypeInfo *found = ZR_NULL;
    for (environment = cs->typeEnv; environment != ZR_NULL;
         environment = environment->parent) {
        TZrSize index;
        for (index = 0U; index < environment->functionReturnTypes.length; ++index) {
            SZrFunctionTypeInfo *const *candidate =
                    (SZrFunctionTypeInfo *const *)ZrCore_Array_Get(
                            (SZrArray *)&environment->functionReturnTypes, index);
            if (candidate == ZR_NULL || *candidate == ZR_NULL ||
                (*candidate)->declarationNode != node) continue;
            if (found != ZR_NULL && found != *candidate) return ZR_NULL;
            found = *candidate;
        }
    }
    return found;
}

static TZrBool source_identity_is_int64(
        const SZrSemanticContext *context, TZrTypeId typeId) {
    const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(context, typeId);
    return (TZrBool)(type != ZR_NULL && type->id == typeId &&
            type->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
            type->data.primitive.valueType == ZR_VALUE_TYPE_INT64);
}

/* AST shape admits this finite slice; only canonical records and the actual
 * lowered value/RETURN below prove its type. No AST value is reconstructed. */
static TZrBool source_identity_candidate(
        SZrCompilerState *cs, const SZrAstNode *returnNode,
        SZrFunctionSourceCallableIdentity *identity, TZrTypeId *returnTypeId) {
    const SZrAstNode *node;
    const SZrFunctionDeclaration *declaration;
    const SZrFunctionTypeInfo *binding;
    const SZrSemanticSymbolRecord *symbol;
    const SZrCanonicalTypeNode *type;
    const SZrAstNodeArray *statements;
    const SZrFileRange *range;
    SZrFunction *function;
    if (cs == ZR_NULL || returnNode == ZR_NULL || cs->hasError ||
        cs->semanticContext == ZR_NULL || !cs->preSemanticIrInitialized ||
        cs->currentFunction == ZR_NULL || cs->currentFunctionNode == ZR_NULL)
        return ZR_FALSE;
    node = cs->currentFunctionNode;
    function = cs->currentFunction;
    if (node->type != ZR_AST_FUNCTION_DECLARATION ||
        returnNode->type != ZR_AST_RETURN_STATEMENT ||
        returnNode->data.returnStatement.isReferenceReturn ||
        returnNode->data.returnStatement.expr == ZR_NULL ||
        returnNode->data.returnStatement.expr->type != ZR_AST_INTEGER_LITERAL)
        return ZR_FALSE;
    declaration = &node->data.functionDeclaration;
    if (declaration->returnType == ZR_NULL || declaration->generic != ZR_NULL ||
        declaration->args != ZR_NULL || declaration->isAsync ||
        (declaration->params != ZR_NULL && declaration->params->count != 0U) ||
        (declaration->decorators != ZR_NULL && declaration->decorators->count != 0U) ||
        declaration->body == ZR_NULL || declaration->body->type != ZR_AST_BLOCK ||
        compiler_iterator_function_contains_yield(declaration->body) ||
        compiler_has_active_scope_ownership_cleanups(cs) ||
        cs->closureVars.length != 0U || cs->childFunctions.length != 0U ||
        function->closureValueLength != 0U || function->typedClosureBindingLength != 0U ||
        function->childFunctionLength != 0U || function->staticImportLength != 0U ||
        function->nativeImportContractLength != 0U || function->moduleEntryEffectLength != 0U)
        return ZR_FALSE;
    statements = declaration->body->data.block.body;
    if (statements == ZR_NULL || statements->count != 1U ||
        statements->nodes == ZR_NULL || statements->nodes[0] != returnNode)
        return ZR_FALSE;
    binding = source_identity_find_declaration(cs, node);
    if (binding == ZR_NULL || binding->isExternalCallable || binding->isCallableValueBinding ||
        binding->paramTypes.length != 0U || binding->genericParameters.length != 0U ||
        binding->symbolId == ZR_SEMANTIC_ID_INVALID ||
        binding->typeId == ZR_SEMANTIC_ID_INVALID || !binding->hasDeclarationRange)
        return ZR_FALSE;
    symbol = ZrParser_Semantic_FindSymbolById(cs->semanticContext, binding->symbolId);
    if (symbol == ZR_NULL || symbol->id != binding->symbolId || symbol->astNode != node ||
        symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION || symbol->typeId != binding->typeId)
        return ZR_FALSE;
    type = ZrParser_CanonicalType_Find(cs->semanticContext, binding->typeId);
    if (type == ZR_NULL || type->id != binding->typeId ||
        type->kind != ZR_CANONICAL_TYPE_FUNCTION || type->structuralHash == 0U ||
        !type->data.function.parameterContracts.isValid ||
        type->data.function.parameterContracts.length != 0U ||
        type->data.function.receiverEffect != ZR_CANONICAL_RECEIVER_NONE ||
        type->data.function.effectFlags != ZR_CANONICAL_CALLABLE_EFFECT_NONE ||
        !source_identity_is_int64(cs->semanticContext, type->data.function.returnTypeId))
        return ZR_FALSE;
    range = &binding->declarationRange;
    if (!source_identity_same_range(range, &declaration->nameLocation) ||
        range->source != returnNode->location.source ||
        range->source != returnNode->data.returnStatement.expr->location.source ||
        range->start.line <= 0 || range->start.column <= 0 ||
        range->end.line < range->start.line || range->end.column <= 0 ||
        (range->end.line == range->start.line && range->end.column < range->start.column))
        return ZR_FALSE;
    memset(identity, 0, sizeof(*identity));
    identity->schemaVersion = ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1;
    identity->symbolId = binding->symbolId;
    identity->typeId = binding->typeId;
    identity->canonicalSignatureHash = type->structuralHash;
    identity->returnPrimitive = ZR_VALUE_TYPE_INT64;
    identity->declarationRange.startLine = (TZrUInt32)range->start.line;
    identity->declarationRange.startColumn = (TZrUInt32)range->start.column;
    identity->declarationRange.endLine = (TZrUInt32)range->end.line;
    identity->declarationRange.endColumn = (TZrUInt32)range->end.column;
    identity->hasExplicitNoArgsI64 = ZR_TRUE;
    *returnTypeId = type->data.function.returnTypeId;
    return ZR_TRUE;
}

static TZrBool source_identity_prove_return(
        const SZrCompilerState *cs, TZrTypeId returnTypeId, const SZrAstNode *returnNode) {
    const SZrSemanticIrFunction *ir = &cs->preSemanticIr;
    const SZrParserCfgBlock *block;
    const SZrSemanticIrInstruction *definition;
    const SZrSemanticIrInstruction *placeBase;
    const SZrSemanticIrInstruction *terminal;
    const SZrSemanticIrValue *value;
    const TZrValueId *operand;
    /* The only reachable block is the source-produced entry. There is no
     * analysis-only exit edge, inferred return or bytecode-derived fact. */
    if (!ir->cfg.blocks.isValid || ir->cfg.blocks.length != 1U ||
        ir->cfg.entryBlockId != 0U || ir->cfg.exitBlockId != 0U ||
        ir->instructions.length != 3U || ir->values.length != 1U ||
        ir->valueOperands.length != 1U) return ZR_FALSE;
    block = (const SZrParserCfgBlock *)ZrCore_Array_Get((SZrArray *)&ir->cfg.blocks, 0U);
    if (block == ZR_NULL || block->id != 0U || block->kind != ZR_PARSER_CFG_BLOCK_ENTRY ||
        block->firstInstructionIndex != 0U || block->instructionCount != 3U ||
        block->successorCount != 0U || !block->outgoingEdges.isValid ||
        block->outgoingEdges.length != 0U ||
        block->terminatorKind != ZR_PARSER_CFG_TERMINATOR_RETURN) return ZR_FALSE;
    definition = ZrParser_SemanticIr_InstructionAt(ir, 0U);
    placeBase = ZrParser_SemanticIr_InstructionAt(ir, 1U);
    terminal = ZrParser_SemanticIr_InstructionAt(ir, 2U);
    operand = (const TZrValueId *)ZrCore_Array_Get((SZrArray *)&ir->valueOperands, 0U);
    value = operand != ZR_NULL ? ZrParser_SemanticIr_Value(ir, *operand) : ZR_NULL;
    return (TZrBool)(definition != ZR_NULL && placeBase != ZR_NULL &&
            terminal != ZR_NULL && value != ZR_NULL &&
            definition->id == 1U && definition->opcode == ZR_SEMANTIC_IR_CONSTANT &&
            definition->operandCount == 0U && definition->hasConstantPoolIndex &&
            definition->placeId == ZR_PLACE_ID_INVALID && definition->valueId == ZR_VALUE_ID_INVALID &&
            definition->typeId == returnTypeId && definition->resultValueId == value->id &&
            value->definitionInstructionId == definition->id && value->typeId == returnTypeId &&
            placeBase->id == 2U && placeBase->opcode == ZR_SEMANTIC_IR_PLACE_BASE &&
            placeBase->typeId == returnTypeId && placeBase->operandCount == 0U &&
            placeBase->resultValueId == ZR_VALUE_ID_INVALID &&
            terminal->id == 3U && terminal->opcode == ZR_SEMANTIC_IR_RETURN &&
            terminal->typeId == returnTypeId && terminal->operandStart == 0U &&
            terminal->operandCount == 1U && terminal->resultValueId == ZR_VALUE_ID_INVALID &&
            terminal->placeId == ZR_PLACE_ID_INVALID && terminal->valueId == ZR_VALUE_ID_INVALID &&
            source_identity_same_range(&terminal->sourceRange, &returnNode->location) &&
            source_identity_is_int64(cs->semanticContext, value->typeId));
}

void compiler_source_callable_identity_try_publish_return(
        SZrCompilerState *cs, const SZrAstNode *returnNode, TZrUInt32 resultSlot) {
    SZrFunctionSourceCallableIdentity identity;
    TZrTypeId returnTypeId;
    const SZrSemanticIrInstruction *definition;
    const SZrSemanticIrInstruction *placeBase;
    const SZrSemanticIrValue *value;
    const SZrCompilerSemanticIrSlot *slot;
    const SZrParserPlace *place;
    const SZrTypeValue *pooledConstant;
    const SZrSemanticIrRegion *region;
    const SZrFileRange *expressionRange;
    const SZrFileRange emptyRange = {0};
    SZrSemanticIrFunction *ir;
    if (!source_identity_candidate(cs, returnNode, &identity, &returnTypeId)) return;
    ir = &cs->preSemanticIr;
    /* Only the literal producer's one inert TEMPORARY place and default
     * function region are admitted. No additional place use or effect survives
     * this gate. The existing caller releases this isolated graph. */
    if (ir->instructions.length != 2U || ir->values.length != 1U ||
        ir->valueOperands.length != 0U || ir->locals.length != 0U ||
        ir->places.places.length != 1U || cs->preSemanticIrSlots.length != 1U ||
        ir->scalarScratchProofs.length != 0U ||
        ir->regions.length != 1U || ir->cleanupScopes.length != 0U ||
        ir->loanFacts.length != 0U || ir->escapeFacts.length != 0U ||
        ir->contiguousViewFacts.length != 0U || ir->boundsFacts.length != 0U ||
        ir->cfg.blocks.length != 0U || cs->preSemanticIrCfgActive ||
        cs->preSemanticIrCfgTerminated || cs->preSemanticIrCfgStartupBlocked ||
        cs->preSemanticIrCfgStartupSuppressed) return;
    region = (const SZrSemanticIrRegion *)ZrCore_Array_Get(&ir->regions, 0U);
    if (region == ZR_NULL || region->id != 1U ||
        region->parentId != ZR_SEMANTIC_REGION_ID_INVALID ||
        region->escapeBound != ZR_SEMANTIC_ESCAPE_FUNCTION ||
        !source_identity_same_range(&region->sourceRange, &emptyRange)) return;
    definition = ZrParser_SemanticIr_InstructionAt(ir, 0U);
    placeBase = ZrParser_SemanticIr_InstructionAt(ir, 1U);
    slot = compiler_semantic_ir_find_slot(cs, resultSlot);
    place = slot != ZR_NULL ? ZrParser_PlaceGraph_Get(&ir->places, slot->placeId) : ZR_NULL;
    value = ZrParser_SemanticIr_Value(ir, compiler_semantic_ir_slot_value(cs, resultSlot));
    if (definition == ZR_NULL || placeBase == ZR_NULL || slot == ZR_NULL ||
        place == ZR_NULL || value == ZR_NULL ||
        definition->opcode != ZR_SEMANTIC_IR_CONSTANT || !definition->hasConstantPoolIndex ||
        definition->placeId != ZR_PLACE_ID_INVALID || definition->valueId != ZR_VALUE_ID_INVALID ||
        definition->loanId != ZR_SEMANTIC_LOAN_ID_INVALID ||
        definition->cleanupScopeId != ZR_SEMANTIC_CLEANUP_SCOPE_ID_INVALID ||
        definition->operandCount != 0U || definition->typeId != returnTypeId ||
        definition->resultValueId != value->id || value->typeId != returnTypeId ||
        value->definitionInstructionId != definition->id ||
        slot->loanId != ZR_SEMANTIC_LOAN_ID_INVALID || slot->typeId != returnTypeId ||
        slot->symbolId != ZR_SEMANTIC_ID_INVALID || slot->regionId != ZR_SEMANTIC_REGION_ID_INVALID ||
        slot->valueId != value->id || place->typeId != returnTypeId ||
        place->base.kind != ZR_PARSER_PLACE_BASE_TEMPORARY ||
        place->base.identity != resultSlot || place->parentId != ZR_PLACE_ID_INVALID ||
        !place->projections.isValid || place->projections.length != 0U ||
        placeBase->opcode != ZR_SEMANTIC_IR_PLACE_BASE || placeBase->typeId != returnTypeId ||
        placeBase->placeId != place->id || placeBase->operandCount != 0U ||
        placeBase->valueId != ZR_VALUE_ID_INVALID ||
        placeBase->loanId != ZR_SEMANTIC_LOAN_ID_INVALID ||
        placeBase->cleanupScopeId != ZR_SEMANTIC_CLEANUP_SCOPE_ID_INVALID ||
        placeBase->resultValueId != ZR_VALUE_ID_INVALID ||
        definition->constantPoolIndex >= cs->constants.length) return;
    pooledConstant = (const SZrTypeValue *)ZrCore_Array_Get(&cs->constants, definition->constantPoolIndex);
    expressionRange = &returnNode->data.returnStatement.expr->location;
    if (pooledConstant == ZR_NULL || pooledConstant->type != ZR_VALUE_TYPE_INT64 ||
        !source_identity_same_range(&definition->sourceRange, expressionRange) ||
        !source_identity_same_range(&placeBase->sourceRange, expressionRange) ||
        !source_identity_same_range(&place->sourceRange, expressionRange) ||
        !source_identity_same_range(&value->sourceRange, expressionRange)) return;
    if (!compiler_semantic_cfg_terminate_return(cs, resultSlot, returnNode->location) ||
        !ZrParser_SemanticIr_ResolveValueFacts(ir, cs->semanticContext) ||
        !ZrParser_SemanticIr_Validate(ir) ||
        !source_identity_prove_return(cs, returnTypeId, returnNode)) return;
    cs->currentFunction->sourceCallableIdentity = identity;
    cs->currentFunction->hasSourceCallableIdentity = ZR_TRUE;
}

void compiler_source_callable_identity_finish(SZrCompilerState *cs) {
    SZrFunction *function;
    const SZrFunctionTypedTypeRef *returnType;
    if (cs == ZR_NULL || cs->currentFunction == ZR_NULL) return;
    function = cs->currentFunction;
    if (!function->hasSourceCallableIdentity) return;
    returnType = &function->callableReturnType;
    if (cs->hasError || !function->hasCallableReturnType ||
        returnType->baseType != ZR_VALUE_TYPE_INT64 || returnType->isNullable ||
        returnType->isArray || returnType->typeName != ZR_NULL ||
        returnType->ownershipQualifier != 0U || cs->closureVars.length != 0U ||
        function->typedClosureBindingLength != 0U) {
        memset(&function->sourceCallableIdentity, 0, sizeof(function->sourceCallableIdentity));
        function->hasSourceCallableIdentity = ZR_FALSE;
    }
}
