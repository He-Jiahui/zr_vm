#include "compiler_internal.h"
#include "type_inference_internal.h"

static const SZrAstNode *compiler_semantic_cfg_catch_parameter(
        const SZrAstNode *catchClause) {
    if (catchClause == ZR_NULL ||
        catchClause->type != ZR_AST_CATCH_CLAUSE ||
        catchClause->data.catchClause.pattern == ZR_NULL ||
        catchClause->data.catchClause.pattern->count != 1U) {
        return ZR_NULL;
    }
    return catchClause->data.catchClause.pattern->nodes[0];
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
        const SZrAstNode *node,
        TZrSize catchIndex) {
    const SZrTryCatchFinallyStatement *statement;
    const SZrAstNode *catchClause;
    const SZrAstNode *parameter;

    if (node == ZR_NULL ||
        node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
        return ZR_FALSE;
    }
    statement = &node->data.tryCatchFinallyStatement;
    if (statement->catchClauses == ZR_NULL ||
        catchIndex >= statement->catchClauses->count) {
        return ZR_FALSE;
    }
    catchClause = statement->catchClauses->nodes[catchIndex];
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
    return (TZrBool)(
            value != ZR_NULL &&
            (value->type == ZR_AST_IDENTIFIER_LITERAL ||
             value->type == ZR_AST_INTEGER_LITERAL ||
             value->type == ZR_AST_BOOLEAN_LITERAL ||
             value->type == ZR_AST_FLOAT_LITERAL));
}

static TZrBool compiler_semantic_cfg_call_has_supported_arguments(
        const SZrFunctionCall *call) {
    TZrSize index;

    if (call == ZR_NULL) {
        return ZR_FALSE;
    }
    if (call->args == ZR_NULL || call->args->count == 0U) {
        return (TZrBool)(call->argumentMarkers == ZR_NULL ||
                        call->argumentMarkers->length == 0U);
    }
    if (call->args->count > 2U ||
        (call->argumentMarkers != ZR_NULL &&
         call->argumentMarkers->length != call->args->count)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < call->args->count; index++) {
        const SZrCallArgumentSyntax *syntax = ZR_NULL;

        if (!compiler_semantic_cfg_is_simple_value_argument(
                    call->args->nodes[index])) {
            return ZR_FALSE;
        }
        if (call->argumentMarkers != ZR_NULL) {
            syntax = (const SZrCallArgumentSyntax *)ZrCore_Array_Get(
                    call->argumentMarkers, index);
            if (syntax == ZR_NULL ||
                syntax->marker != ZR_CALL_ARGUMENT_MARKER_NONE) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

const SZrAstNode *compiler_semantic_cfg_supported_direct_call(
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
    const SZrAstNode *protectedStatement;
    const SZrAstNode *protectedCall;
    TZrSize index;

    if (cs == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
        return ZR_FALSE;
    }
    statement = &node->data.tryCatchFinallyStatement;
    if (statement->finallyBlock != ZR_NULL ||
        statement->catchClauses == ZR_NULL ||
        statement->catchClauses->count == 0U ||
        statement->block == ZR_NULL || statement->block->type != ZR_AST_BLOCK ||
        statement->block->data.block.body == ZR_NULL ||
        statement->block->data.block.body->count != 1U) {
        return ZR_FALSE;
    }
    protectedStatement = statement->block->data.block.body->nodes[0];
    protectedCall = protectedStatement == ZR_NULL ||
                            protectedStatement->type !=
                                    ZR_AST_EXPRESSION_STATEMENT ||
                            protectedStatement->data.expressionStatement.expr ==
                                    ZR_NULL
                    ? ZR_NULL
                    : compiler_semantic_cfg_supported_direct_call(
                              protectedStatement->data.expressionStatement.expr);
    if (protectedStatement == ZR_NULL ||
        protectedStatement->type != ZR_AST_EXPRESSION_STATEMENT ||
        protectedStatement->data.expressionStatement.expr == ZR_NULL ||
        protectedCall == ZR_NULL ||
        (protectedCall->data.functionCall.args != ZR_NULL &&
         protectedCall->data.functionCall.args->count > 1U)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < statement->catchClauses->count; index++) {
        const SZrAstNode *catchClause =
                statement->catchClauses->nodes[index];
        const SZrAstNode *parameter =
                compiler_semantic_cfg_catch_parameter(catchClause);

        if (catchClause == ZR_NULL ||
            catchClause->type != ZR_AST_CATCH_CLAUSE ||
            parameter == ZR_NULL || parameter->type != ZR_AST_PARAMETER ||
            parameter->data.parameter.name == ZR_NULL ||
            parameter->data.parameter.name->name == ZR_NULL ||
            !compiler_semantic_cfg_catch_type_is_resolvable(
                    cs, parameter->data.parameter.typeInfo) ||
            !compiler_semantic_cfg_is_supported_catch_block(
                    cs,
                    catchClause->data.catchClause.block,
                    parameter->data.parameter.name->name) ||
            (parameter->data.parameter.typeInfo == ZR_NULL &&
             index + 1U != statement->catchClauses->count)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_try_call_arguments_are_exact(
        SZrCompilerState *cs,
        const SZrFunctionCall *call,
        const SZrResolvedCallSignature *resolvedSignature,
        TZrUInt32 firstArgumentSlot) {
    TZrSize index;
    SZrInferredType actualType;

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
    if (call->args->count > 2U || resolvedSignature == ZR_NULL ||
        resolvedSignature->parameterTypes.length != call->args->count ||
        resolvedSignature->parameterPassingModes.length != call->args->count ||
        firstArgumentSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    for (index = 0U; index < call->args->count; index++) {
        const SZrInferredType *expectedType =
                (const SZrInferredType *)ZrCore_Array_Get(
                        (SZrArray *)&resolvedSignature->parameterTypes,
                        index);
        const EZrParameterPassingMode *passingMode =
                (const EZrParameterPassingMode *)ZrCore_Array_Get(
                        (SZrArray *)&resolvedSignature->parameterPassingModes,
                        index);
        TZrValueId argumentValueId;
        const SZrSemanticIrValue *argumentValue;
        const SZrSemanticIrInstruction *argumentDefinition;

        if (expectedType == ZR_NULL || passingMode == ZR_NULL ||
            *passingMode != ZR_PARAMETER_PASSING_MODE_VALUE) {
            return ZR_FALSE;
        }
        ZrParser_InferredType_Init(
                cs->state, &actualType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_ExpressionType_Infer(
                    cs, call->args->nodes[index], &actualType) ||
            (!ZR_VALUE_IS_TYPE_INT(actualType.baseType) &&
             actualType.baseType != ZR_VALUE_TYPE_BOOL &&
             !ZR_VALUE_IS_TYPE_FLOAT(actualType.baseType)) ||
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
                cs, firstArgumentSlot + (TZrUInt32)index);
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
        if (argumentDefinition == ZR_NULL ||
            (argumentDefinition->opcode != ZR_SEMANTIC_IR_LOAD &&
             argumentDefinition->opcode != ZR_SEMANTIC_IR_CONSTANT) ||
            argumentDefinition->resultValueId != argumentValueId) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
