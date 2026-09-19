#include "compiler_internal.h"
#include "type_inference_internal.h"

static const SZrAstNode *compiler_semantic_cfg_single_catch(
        const SZrAstNode *node) {
    const SZrTryCatchFinallyStatement *statement;

    if (node == ZR_NULL ||
        node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
        return ZR_NULL;
    }
    statement = &node->data.tryCatchFinallyStatement;
    if (statement->finallyBlock != ZR_NULL ||
        statement->catchClauses == ZR_NULL ||
        statement->catchClauses->count != 1U) {
        return ZR_NULL;
    }
    return statement->catchClauses->nodes[0];
}

static TZrBool compiler_semantic_cfg_is_empty_block(
        const SZrAstNode *node) {
    return (TZrBool)(node != ZR_NULL && node->type == ZR_AST_BLOCK &&
                    (node->data.block.body == ZR_NULL ||
                     node->data.block.body->count == 0U));
}

static SZrString *compiler_semantic_cfg_simple_identifier_name(
        const SZrAstNode *node) {
    const SZrAstNode *identifier = node;

    if (identifier != ZR_NULL &&
        identifier->type == ZR_AST_PRIMARY_EXPRESSION) {
        if (identifier->data.primaryExpression.members != ZR_NULL &&
            identifier->data.primaryExpression.members->count != 0U) {
            return ZR_NULL;
        }
        identifier = identifier->data.primaryExpression.property;
    }
    if (identifier == ZR_NULL ||
        identifier->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }
    return identifier->data.identifier.name;
}

static TZrBool compiler_semantic_cfg_is_catch_binding_read(
        const SZrAstNode *node,
        SZrString *bindingName) {
    const SZrAstNode *expression;
    SZrString *identifierName;

    if (node == ZR_NULL || node->type != ZR_AST_EXPRESSION_STATEMENT ||
        bindingName == ZR_NULL) {
        return ZR_FALSE;
    }
    expression = node->data.expressionStatement.expr;
    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }
    identifierName = compiler_semantic_cfg_simple_identifier_name(expression);
    return (TZrBool)(identifierName != ZR_NULL &&
                    ZrCore_String_Equal(identifierName, bindingName));
}

static TZrBool compiler_semantic_cfg_is_canonical_catch_return(
        const SZrAstNode *expression,
        SZrString *bindingName) {
    SZrString *identifierName;

    if (expression == ZR_NULL) {
        return ZR_TRUE;
    }
    switch (expression->type) {
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            return ZR_TRUE;
        default:
            break;
    }
    identifierName = compiler_semantic_cfg_simple_identifier_name(
            expression);
    return (TZrBool)(identifierName != ZR_NULL &&
                    bindingName != ZR_NULL &&
                    ZrCore_String_Equal(identifierName, bindingName));
}

static TZrBool compiler_semantic_cfg_is_direct_catch_return(
        const SZrAstNode *node,
        SZrString *bindingName) {
    const SZrAstNode *statement;

    if (node == ZR_NULL || node->type != ZR_AST_BLOCK ||
        node->data.block.body == ZR_NULL ||
        node->data.block.body->count != 1U) {
        return ZR_FALSE;
    }
    statement = node->data.block.body->nodes[0];
    return (TZrBool)(statement != ZR_NULL &&
                    statement->type == ZR_AST_RETURN_STATEMENT &&
                    compiler_semantic_cfg_is_canonical_catch_return(
                            statement->data.returnStatement.expr,
                            bindingName));
}

static TZrBool compiler_semantic_cfg_is_direct_catch_abrupt(
        const SZrAstNode *node,
        SZrString *bindingName) {
    const SZrAstNode *statement;
    SZrString *identifierName;

    if (node == ZR_NULL || node->type != ZR_AST_BLOCK ||
        node->data.block.body == ZR_NULL ||
        node->data.block.body->count != 1U) {
        return ZR_FALSE;
    }
    statement = node->data.block.body->nodes[0];
    if (statement == ZR_NULL) {
        return ZR_FALSE;
    }
    if (compiler_semantic_cfg_is_direct_catch_return(
                node, bindingName)) {
        return ZR_TRUE;
    }
    if (statement->type != ZR_AST_THROW_STATEMENT ||
        statement->data.throwStatement.expr == ZR_NULL) {
        return ZR_FALSE;
    }
    identifierName = compiler_semantic_cfg_simple_identifier_name(
            statement->data.throwStatement.expr);
    return (TZrBool)(identifierName != ZR_NULL &&
                    bindingName != ZR_NULL &&
                    ZrCore_String_Equal(identifierName, bindingName));
}

static TZrBool compiler_semantic_cfg_identifier_name_is_unbound(
        SZrCompilerState *cs,
        SZrString *name) {
    SZrFunctionTypeInfo *functionInfo = ZR_NULL;

    if (cs == ZR_NULL || cs->typeEnv == ZR_NULL || name == ZR_NULL ||
        ZrParser_TypeEnvironment_FindVariableBinding(
                cs->typeEnv, name) != ZR_NULL ||
        ZrParser_TypeEnvironment_LookupFunction(
                cs->typeEnv, name, &functionInfo)) {
        return ZR_FALSE;
    }
    functionInfo = ZR_NULL;
    if (cs->compileTimeTypeEnv != ZR_NULL &&
        ZrParser_TypeEnvironment_LookupFunction(
                cs->compileTimeTypeEnv, name, &functionInfo)) {
        return ZR_FALSE;
    }
    return (TZrBool)(find_compiler_type_prototype_inference(cs, name) ==
                    ZR_NULL);
}

static TZrBool compiler_semantic_cfg_is_catch_local_flow(
        SZrCompilerState *cs,
        const SZrAstNode *node,
        SZrString *bindingName) {
    const SZrVariableDeclaration *declaration;
    SZrString *localName;
    SZrString *initializerName;

    if (cs == ZR_NULL || cs->typeEnv == ZR_NULL ||
        node == ZR_NULL || node->type != ZR_AST_BLOCK ||
        node->data.block.body == ZR_NULL ||
        node->data.block.body->count != 2U ||
        bindingName == ZR_NULL ||
        node->data.block.body->nodes[0] == ZR_NULL ||
        node->data.block.body->nodes[0]->type !=
                ZR_AST_VARIABLE_DECLARATION) {
        return ZR_FALSE;
    }
    declaration =
            &node->data.block.body->nodes[0]->data.variableDeclaration;
    if (declaration->pattern == ZR_NULL ||
        declaration->pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        declaration->typeInfo != ZR_NULL || declaration->value == ZR_NULL ||
        declaration->isConst ||
        declaration->accessModifier != ZR_ACCESS_PRIVATE) {
        return ZR_FALSE;
    }
    localName = declaration->pattern->data.identifier.name;
    initializerName = compiler_semantic_cfg_simple_identifier_name(
            declaration->value);
    return (TZrBool)(localName != ZR_NULL && initializerName != ZR_NULL &&
                    !ZrCore_String_Equal(localName, bindingName) &&
                    compiler_semantic_cfg_identifier_name_is_unbound(
                            cs, bindingName) &&
                    compiler_semantic_cfg_identifier_name_is_unbound(
                            cs, localName) &&
                    ZrCore_String_Equal(initializerName, bindingName) &&
                    compiler_semantic_cfg_is_catch_binding_read(
                            node->data.block.body->nodes[1], localName));
}

static TZrBool compiler_semantic_cfg_is_supported_catch_block(
        SZrCompilerState *cs,
        const SZrAstNode *node,
        SZrString *bindingName) {
    if (compiler_semantic_cfg_is_empty_block(node)) {
        return ZR_TRUE;
    }
    if (node != ZR_NULL && node->type == ZR_AST_BLOCK &&
        node->data.block.body != ZR_NULL &&
        node->data.block.body->count == 1U &&
        compiler_semantic_cfg_is_catch_binding_read(
                node->data.block.body->nodes[0], bindingName)) {
        return ZR_TRUE;
    }
    if (compiler_semantic_cfg_is_direct_catch_abrupt(
                node, bindingName)) {
        return (TZrBool)(
                !compiler_has_active_scope_ownership_cleanups(cs) &&
                !(cs->currentFunctionNode != ZR_NULL &&
                  compiler_semantic_cfg_is_direct_catch_return(
                          node, bindingName)));
    }
    return compiler_semantic_cfg_is_catch_local_flow(
            cs, node, bindingName);
}

TZrBool compiler_semantic_cfg_try_catch_handler_terminates(
        const SZrAstNode *node) {
    const SZrAstNode *catchClause =
            compiler_semantic_cfg_single_catch(node);
    const SZrAstNode *parameter;

    if (catchClause == ZR_NULL ||
        catchClause->type != ZR_AST_CATCH_CLAUSE ||
        catchClause->data.catchClause.pattern == ZR_NULL ||
        catchClause->data.catchClause.pattern->count != 1U) {
        return ZR_FALSE;
    }
    parameter = catchClause->data.catchClause.pattern->nodes[0];
    return (TZrBool)(parameter != ZR_NULL &&
                    parameter->type == ZR_AST_PARAMETER &&
                    parameter->data.parameter.name != ZR_NULL &&
                    parameter->data.parameter.name->name != ZR_NULL &&
                    compiler_semantic_cfg_is_direct_catch_abrupt(
                            catchClause->data.catchClause.block,
                            parameter->data.parameter.name->name));
}

static TZrBool compiler_semantic_cfg_is_simple_value_argument(
        const SZrAstNode *node) {
    const SZrAstNode *value = node;

    if (value != ZR_NULL && value->type == ZR_AST_PRIMARY_EXPRESSION) {
        if (value->data.primaryExpression.members != ZR_NULL &&
            value->data.primaryExpression.members->count != 0U) {
            return ZR_FALSE;
        }
        value = value->data.primaryExpression.property;
    }
    return (TZrBool)(value != ZR_NULL &&
                    value->type == ZR_AST_IDENTIFIER_LITERAL);
}

static TZrBool compiler_semantic_cfg_call_has_supported_arguments(
        const SZrFunctionCall *call) {
    const SZrCallArgumentSyntax *syntax;

    if (call == ZR_NULL) {
        return ZR_FALSE;
    }
    if (call->args == ZR_NULL || call->args->count == 0U) {
        return (TZrBool)(call->argumentMarkers == ZR_NULL ||
                        call->argumentMarkers->length == 0U);
    }
    if (call->args->count != 1U ||
        !compiler_semantic_cfg_is_simple_value_argument(
                call->args->nodes[0])) {
        return ZR_FALSE;
    }
    if (call->argumentMarkers == ZR_NULL) {
        return ZR_TRUE;
    }
    if (call->argumentMarkers->length != 1U) {
        return ZR_FALSE;
    }
    syntax = (const SZrCallArgumentSyntax *)ZrCore_Array_Get(
            call->argumentMarkers, 0U);
    return (TZrBool)(syntax != ZR_NULL &&
                    syntax->marker == ZR_CALL_ARGUMENT_MARKER_NONE);
}

static const SZrAstNode *compiler_semantic_cfg_supported_direct_call(
        const SZrAstNode *node) {
    const SZrAstNode *callNode;
    const SZrFunctionCall *call;

    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION ||
        node->data.primaryExpression.property == ZR_NULL ||
        node->data.primaryExpression.property->type !=
                ZR_AST_IDENTIFIER_LITERAL ||
        node->data.primaryExpression.members == ZR_NULL ||
        node->data.primaryExpression.members->count != 1U) {
        return ZR_NULL;
    }
    callNode = node->data.primaryExpression.members->nodes[0];
    if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
        return ZR_NULL;
    }
    call = &callNode->data.functionCall;
    if (!compiler_semantic_cfg_call_has_supported_arguments(call) ||
        (call->genericArguments != ZR_NULL &&
         call->genericArguments->count != 0U) ||
        call->hasNamedArgs ||
        call->accessMode != ZR_POSTFIX_ACCESS_DIRECT) {
        return ZR_NULL;
    }
    return callNode;
}

TZrBool compiler_semantic_cfg_try_catch_is_supported(
        SZrCompilerState *cs,
        const SZrAstNode *node) {
    const SZrTryCatchFinallyStatement *statement;
    const SZrAstNode *catchClause;
    const SZrAstNode *parameter;
    const SZrAstNode *protectedStatement;

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    catchClause = compiler_semantic_cfg_single_catch(node);
    if (catchClause == ZR_NULL || catchClause->type != ZR_AST_CATCH_CLAUSE) {
        return ZR_FALSE;
    }
    statement = &node->data.tryCatchFinallyStatement;
    if (statement->block == ZR_NULL || statement->block->type != ZR_AST_BLOCK ||
        statement->block->data.block.body == ZR_NULL ||
        statement->block->data.block.body->count != 1U) {
        return ZR_FALSE;
    }
    protectedStatement = statement->block->data.block.body->nodes[0];
    if (protectedStatement == ZR_NULL ||
        protectedStatement->type != ZR_AST_EXPRESSION_STATEMENT ||
        protectedStatement->data.expressionStatement.expr == ZR_NULL ||
        compiler_semantic_cfg_supported_direct_call(
                protectedStatement->data.expressionStatement.expr) ==
                ZR_NULL) {
        return ZR_FALSE;
    }
    if (catchClause->data.catchClause.pattern == ZR_NULL ||
        catchClause->data.catchClause.pattern->count != 1U) {
        return ZR_FALSE;
    }
    parameter = catchClause->data.catchClause.pattern->nodes[0];
    return (TZrBool)(parameter != ZR_NULL &&
                    parameter->type == ZR_AST_PARAMETER &&
                    parameter->data.parameter.name != ZR_NULL &&
                    parameter->data.parameter.name->name != ZR_NULL &&
                    parameter->data.parameter.typeInfo == ZR_NULL &&
                    compiler_semantic_cfg_is_supported_catch_block(
                            cs,
                            catchClause->data.catchClause.block,
                            parameter->data.parameter.name->name));
}

TZrBool compiler_semantic_cfg_try_call_arguments_are_exact(
        SZrCompilerState *cs,
        const SZrFunctionCall *call,
        const SZrResolvedCallSignature *resolvedSignature,
        TZrUInt32 firstArgumentSlot) {
    const SZrInferredType *expectedType;
    const EZrParameterPassingMode *passingMode;
    const SZrSemanticIrValue *argumentValue;
    const SZrSemanticIrInstruction *argumentDefinition;
    SZrInferredType actualType;
    TZrValueId argumentValueId;
    TZrBool supported = ZR_FALSE;

    if (cs == ZR_NULL || call == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgCatchBlock ==
        ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_TRUE;
    }
    if (call->args == ZR_NULL || call->args->count == 0U) {
        return ZR_TRUE;
    }
    if (call->args->count != 1U || resolvedSignature == ZR_NULL ||
        resolvedSignature->parameterTypes.length != 1U ||
        resolvedSignature->parameterPassingModes.length != 1U ||
        firstArgumentSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    expectedType = (const SZrInferredType *)ZrCore_Array_Get(
            (SZrArray *)&resolvedSignature->parameterTypes, 0U);
    passingMode = (const EZrParameterPassingMode *)ZrCore_Array_Get(
            (SZrArray *)&resolvedSignature->parameterPassingModes, 0U);
    if (expectedType == ZR_NULL || passingMode == ZR_NULL ||
        *passingMode != ZR_PARAMETER_PASSING_MODE_VALUE) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &actualType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(
                cs, call->args->nodes[0], &actualType)) {
        ZrParser_InferredType_Free(cs->state, &actualType);
        return ZR_FALSE;
    }
    if (actualType.baseType != ZR_VALUE_TYPE_INT64 ||
        actualType.isNullable ||
        actualType.ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        actualType.gcBridgeKind != ZR_GC_BRIDGE_NONE ||
        actualType.referenceAccess != ZR_REFERENCE_ACCESS_NONE ||
        !ZrParser_InferredType_Equal(&actualType, expectedType)) {
        ZrParser_InferredType_Free(cs->state, &actualType);
        return ZR_FALSE;
    }
    ZrParser_InferredType_Free(cs->state, &actualType);

    argumentValueId = compiler_semantic_ir_slot_value(
            cs, firstArgumentSlot);
    argumentValue = ZrParser_SemanticIr_Value(
            &cs->preSemanticIr, argumentValueId);
    if (argumentValue == ZR_NULL ||
        argumentValue->definitionInstructionId ==
                ZR_SEMANTIC_INSTRUCTION_ID_INVALID) {
        return ZR_FALSE;
    }
    argumentDefinition = ZrParser_SemanticIr_InstructionAt(
            &cs->preSemanticIr,
            argumentValue->definitionInstructionId - 1U);
    if (argumentDefinition != ZR_NULL &&
        argumentDefinition->opcode == ZR_SEMANTIC_IR_LOAD &&
        argumentDefinition->resultValueId == argumentValueId) {
        supported = ZR_TRUE;
    }
    return supported;
}

TZrBool compiler_semantic_cfg_begin_try_catch(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrUInt32 *handlerBlock,
        TZrUInt32 *joinBlock,
        SZrArray *entrySlots) {
    SZrParserCfg *cfg;

    if (cs == ZR_NULL || node == ZR_NULL || handlerBlock == ZR_NULL ||
        joinBlock == ZR_NULL || entrySlots == ZR_NULL ||
        !compiler_semantic_cfg_try_catch_is_supported(cs, node) ||
        cs->preSemanticIrCfgCatchBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        !compiler_semantic_cfg_ensure_active(cs)) {
        return ZR_FALSE;
    }
    memset(entrySlots, 0, sizeof(*entrySlots));
    cfg = &cs->preSemanticIr.cfg;
    *handlerBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
            node->data.tryCatchFinallyStatement.catchClauses->nodes[0]);
    *joinBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, node);
    if (*handlerBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        *joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        !compiler_semantic_cfg_capture_slots(cs, entrySlots)) {
        if (entrySlots->isValid) {
            compiler_semantic_cfg_free_slots(cs, entrySlots);
        }
        (void)compiler_semantic_cfg_abandon(cs);
        return ZR_FALSE;
    }
    cs->preSemanticIrCfgCatchBlock = *handlerBlock;
    cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_emit_exception_payload(
        SZrCompilerState *cs,
        SZrFileRange sourceRange,
        TZrTypeId *outTypeId,
        TZrValueId *outValueId) {
    SZrInferredType payloadType;
    SZrSemanticIrInstructionSpec spec;
    TZrTypeId typeId;
    TZrValueId valueId;

    ZrParser_InferredType_Init(cs->state, &payloadType, ZR_VALUE_TYPE_OBJECT);
    typeId = ZrParser_Semantic_RegisterInferredType(
            cs->semanticContext, &payloadType,
            ZR_SEMANTIC_TYPE_KIND_UNKNOWN, ZR_NULL, ZR_NULL);
    ZrParser_InferredType_Free(cs->state, &payloadType);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    valueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, typeId, sourceRange);
    if (valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD;
    spec.typeId = typeId;
    spec.resultValueId = valueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    if (outTypeId != ZR_NULL) {
        *outTypeId = typeId;
    }
    if (outValueId != ZR_NULL) {
        *outValueId = valueId;
    }
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_bind_catch_payload(
        SZrCompilerState *cs,
        const SZrAstNode *catchClause,
        TZrUInt32 bindingSlot) {
    const SZrAstNode *parameter;
    SZrString *bindingName;
    SZrCompilerSemanticIrSlot slot;
    SZrParserPlaceBase base;
    SZrSemanticIrInstructionSpec spec;
    TZrTypeId typeId = ZR_SEMANTIC_ID_INVALID;
    TZrValueId valueId = ZR_VALUE_ID_INVALID;

    if (cs == ZR_NULL || catchClause == ZR_NULL ||
        catchClause->type != ZR_AST_CATCH_CLAUSE ||
        catchClause->data.catchClause.pattern == ZR_NULL ||
        catchClause->data.catchClause.pattern->count != 1U ||
        bindingSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    parameter = catchClause->data.catchClause.pattern->nodes[0];
    if (parameter == ZR_NULL || parameter->type != ZR_AST_PARAMETER ||
        parameter->data.parameter.name == ZR_NULL ||
        parameter->data.parameter.name->name == ZR_NULL ||
        !compiler_semantic_cfg_emit_exception_payload(
                cs, catchClause->location, &typeId, &valueId)) {
        return ZR_FALSE;
    }
    bindingName = parameter->data.parameter.name->name;

    memset(&slot, 0, sizeof(slot));
    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    base.identity = bindingSlot;
    slot.stackSlot = bindingSlot;
    slot.typeId = typeId;
    slot.valueId = valueId;
    slot.symbolId = ZrParser_Semantic_RegisterSymbol(
            cs->semanticContext,
            bindingName,
            ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
            typeId,
            ZR_SEMANTIC_ID_INVALID,
            (SZrAstNode *)parameter,
            parameter->location);
    if (slot.symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    slot.placeId = ZrParser_SemanticIr_AddLocal(
            &cs->preSemanticIr,
            slot.symbolId,
            &base,
            typeId,
            parameter->location,
            ZR_FALSE);
    if (slot.placeId == ZR_PLACE_ID_INVALID) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(cs->state, &cs->preSemanticIrSlots, &slot);

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    spec.typeId = typeId;
    spec.placeId = slot.placeId;
    spec.symbolId = slot.symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = parameter->location;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_INITIALIZE;
    spec.typeId = typeId;
    spec.placeId = slot.placeId;
    spec.valueId = valueId;
    spec.symbolId = slot.symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = parameter->location;
    return compiler_semantic_ir_emit(cs, &spec);
}

TZrBool compiler_semantic_cfg_enter_try_catch_handler(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrUInt32 handlerBlock,
        TZrUInt32 joinBlock,
        TZrUInt32 bindingSlot,
        SZrArray *entrySlots,
        TZrBool *outEntered) {
    const SZrAstNode *catchClause;

    if (outEntered != ZR_NULL) {
        *outEntered = ZR_FALSE;
    }
    if (cs == ZR_NULL || node == ZR_NULL || entrySlots == ZR_NULL ||
        outEntered == ZR_NULL) {
        return ZR_FALSE;
    }
    catchClause = compiler_semantic_cfg_single_catch(node);
    if (!cs->preSemanticIrCfgActive || !cs->preSemanticIrCfgCatchUsed ||
        cs->preSemanticIrCfgCatchBlock != handlerBlock) {
        if (cs->preSemanticIrCfgActive &&
            !compiler_semantic_cfg_abandon(cs)) {
            return ZR_FALSE;
        }
        cs->preSemanticIrCfgCatchBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
        cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
        if (entrySlots->isValid) {
            compiler_semantic_cfg_free_slots(cs, entrySlots);
        }
        return ZR_TRUE;
    }
    if (catchClause == ZR_NULL ||
        !compiler_semantic_cfg_jump(cs, joinBlock, node->location)) {
        goto fail;
    }
    cs->preSemanticIrCfgCatchBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
    if (!compiler_semantic_cfg_restore_slots(cs, entrySlots)) {
        goto fail;
    }
    compiler_semantic_cfg_enter(cs, handlerBlock);
    if (!compiler_semantic_cfg_bind_catch_payload(
                cs, catchClause, bindingSlot)) {
        goto fail;
    }
    *outEntered = ZR_TRUE;
    return ZR_TRUE;

fail:
    if (cs->preSemanticIrCfgActive) {
        (void)compiler_semantic_cfg_abandon(cs);
    }
    cs->preSemanticIrCfgCatchBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
    cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
    if (entrySlots->isValid) {
        compiler_semantic_cfg_free_slots(cs, entrySlots);
    }
    return ZR_FALSE;
}

TZrBool compiler_semantic_cfg_complete_try_catch(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrUInt32 handlerBlock,
        TZrUInt32 joinBlock,
        TZrBool handlerTerminates,
        SZrArray *entrySlots) {
    TZrBool completed = ZR_FALSE;

    if (cs == ZR_NULL || node == ZR_NULL || entrySlots == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!cs->preSemanticIrCfgActive ||
        cs->preSemanticIrCfgCatchBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        (handlerTerminates
                 ? cs->preSemanticIrCfgBlock !=
                           ZR_PARSER_CFG_INVALID_BLOCK_ID
                 : cs->preSemanticIrCfgBlock != handlerBlock)) {
        goto cleanup;
    }
    if (!handlerTerminates &&
        !compiler_semantic_cfg_jump(cs, joinBlock, node->location)) {
        goto cleanup;
    }
    if (!compiler_semantic_cfg_restore_slots(cs, entrySlots)) {
        goto cleanup;
    }
    compiler_semantic_cfg_enter(cs, joinBlock);
    completed = ZR_TRUE;

cleanup:
    if (!completed) {
        if (cs->preSemanticIrCfgActive) {
            (void)compiler_semantic_cfg_abandon(cs);
        }
        cs->preSemanticIrCfgCatchBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
        cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
    }
    if (entrySlots->isValid) {
        compiler_semantic_cfg_free_slots(cs, entrySlots);
    }
    return completed;
}
