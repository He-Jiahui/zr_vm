#include "compiler_internal.h"

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

static const SZrAstNode *compiler_semantic_cfg_zero_argument_direct_call(
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
    if ((call->args != ZR_NULL && call->args->count != 0U) ||
        (call->genericArguments != ZR_NULL &&
         call->genericArguments->count != 0U) ||
        (call->argumentMarkers != ZR_NULL &&
         call->argumentMarkers->length != 0U) ||
        call->hasNamedArgs ||
        call->accessMode != ZR_POSTFIX_ACCESS_DIRECT) {
        return ZR_NULL;
    }
    return callNode;
}

TZrBool compiler_semantic_cfg_try_catch_is_supported(
        const SZrAstNode *node) {
    const SZrTryCatchFinallyStatement *statement;
    const SZrAstNode *catchClause;
    const SZrAstNode *parameter;
    const SZrAstNode *protectedStatement;

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
        compiler_semantic_cfg_zero_argument_direct_call(
                protectedStatement->data.expressionStatement.expr) ==
                ZR_NULL) {
        return ZR_FALSE;
    }
    if (!compiler_semantic_cfg_is_empty_block(
                catchClause->data.catchClause.block) ||
        catchClause->data.catchClause.pattern == ZR_NULL ||
        catchClause->data.catchClause.pattern->count != 1U) {
        return ZR_FALSE;
    }
    parameter = catchClause->data.catchClause.pattern->nodes[0];
    return (TZrBool)(parameter != ZR_NULL &&
                    parameter->type == ZR_AST_PARAMETER &&
                    parameter->data.parameter.name != ZR_NULL &&
                    parameter->data.parameter.name->name != ZR_NULL &&
                    parameter->data.parameter.typeInfo == ZR_NULL);
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
        !compiler_semantic_cfg_try_catch_is_supported(node) ||
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
        SZrFileRange sourceRange) {
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
    return compiler_semantic_ir_emit(cs, &spec);
}

TZrBool compiler_semantic_cfg_complete_try_catch(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrUInt32 handlerBlock,
        TZrUInt32 joinBlock,
        SZrArray *entrySlots) {
    const SZrAstNode *catchClause;
    TZrBool completed = ZR_FALSE;

    if (cs == ZR_NULL || node == ZR_NULL || entrySlots == ZR_NULL) {
        return ZR_FALSE;
    }
    catchClause = compiler_semantic_cfg_single_catch(node);
    if (!cs->preSemanticIrCfgActive || !cs->preSemanticIrCfgCatchUsed ||
        cs->preSemanticIrCfgCatchBlock != handlerBlock) {
        if (cs->preSemanticIrCfgActive &&
            !compiler_semantic_cfg_abandon(cs)) {
            goto cleanup;
        }
        cs->preSemanticIrCfgCatchBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
        cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
        completed = ZR_TRUE;
        goto cleanup;
    }
    if (catchClause == ZR_NULL ||
        !compiler_semantic_cfg_jump(cs, joinBlock, node->location)) {
        goto cleanup;
    }
    cs->preSemanticIrCfgCatchBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
    if (!compiler_semantic_cfg_restore_slots(cs, entrySlots)) {
        goto cleanup;
    }
    compiler_semantic_cfg_enter(cs, handlerBlock);
    if (!compiler_semantic_cfg_emit_exception_payload(
                cs, catchClause->location) ||
        !compiler_semantic_cfg_jump(cs, joinBlock, catchClause->location) ||
        !compiler_semantic_cfg_restore_slots(cs, entrySlots)) {
        goto cleanup;
    }
    compiler_semantic_cfg_enter(cs, joinBlock);
    completed = ZR_TRUE;

cleanup:
    if (entrySlots->isValid) {
        compiler_semantic_cfg_free_slots(cs, entrySlots);
    }
    return completed;
}
