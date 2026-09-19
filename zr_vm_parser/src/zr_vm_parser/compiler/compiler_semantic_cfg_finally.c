#include "compiler_internal.h"

static TZrBool compiler_semantic_cfg_finally_block_is_linear(
        const SZrAstNode *node) {
    TZrSize index;

    if (node == ZR_NULL || node->type != ZR_AST_BLOCK) {
        return ZR_FALSE;
    }
    if (node->data.block.body == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < node->data.block.body->count; index++) {
        const SZrAstNode *statement = node->data.block.body->nodes[index];

        if (statement == ZR_NULL) {
            continue;
        }
        if (statement->type == ZR_AST_BLOCK) {
            if (!compiler_semantic_cfg_finally_block_is_linear(statement)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (statement->type != ZR_AST_EXPRESSION_STATEMENT ||
            !compiler_semantic_cfg_expression_is_linear(
                    statement->data.expressionStatement.expr)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

typedef enum EZrCompilerSemanticFinallyFlow {
    ZR_COMPILER_SEMANTIC_FINALLY_FLOW_UNSUPPORTED = 0,
    ZR_COMPILER_SEMANTIC_FINALLY_FLOW_FALLTHROUGH = 1,
    ZR_COMPILER_SEMANTIC_FINALLY_FLOW_RETURN = 2,
    ZR_COMPILER_SEMANTIC_FINALLY_FLOW_THROW = 4
} EZrCompilerSemanticFinallyFlow;

typedef struct SZrCompilerSemanticFinallyFlowInfo {
    TZrUInt32 flow;
    TZrUInt32 abruptSiteCount;
    TZrUInt32 exceptionalSiteCount;
    const SZrAstNode *completionExpression;
    const SZrAstNode *exceptionExpression;
} SZrCompilerSemanticFinallyFlowInfo;

static TZrBool compiler_semantic_cfg_finally_is_supported_direct_call(
        const SZrAstNode *node) {
    const SZrAstNode *callNode =
            compiler_semantic_cfg_supported_direct_call(node);

    return (TZrBool)(callNode != ZR_NULL &&
                    callNode->type == ZR_AST_FUNCTION_CALL);
}

static TZrBool compiler_semantic_cfg_finally_call_is_resolved(
        SZrCompilerState *cs,
        const SZrAstNode *node) {
    const SZrAstNode *target;
    SZrFunctionTypeInfo *functionInfo = ZR_NULL;

    if (cs == ZR_NULL || cs->typeEnv == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    target = node->data.primaryExpression.property;
    if (target == ZR_NULL || target->type != ZR_AST_IDENTIFIER_LITERAL ||
        target->data.identifier.name == ZR_NULL ||
        ZrParser_TypeEnvironment_FindVariableBinding(
                cs->typeEnv, target->data.identifier.name) != ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrParser_TypeEnvironment_LookupFunction(
            cs->typeEnv, target->data.identifier.name, &functionInfo);
}

static TZrBool compiler_semantic_cfg_finally_protected_flow(
        const SZrAstNode *node,
        SZrCompilerSemanticFinallyFlowInfo *outInfo) {
    SZrCompilerSemanticFinallyFlowInfo info;

    if (outInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&info, 0, sizeof(info));
    if (node == ZR_NULL) {
        info.flow = ZR_COMPILER_SEMANTIC_FINALLY_FLOW_FALLTHROUGH;
    } else if (node->type == ZR_AST_BLOCK) {
        TZrSize index;

        info.flow = ZR_COMPILER_SEMANTIC_FINALLY_FLOW_FALLTHROUGH;
        if (node->data.block.body != ZR_NULL) {
            for (index = 0U; index < node->data.block.body->count; index++) {
                const SZrAstNode *statement =
                        node->data.block.body->nodes[index];
                SZrCompilerSemanticFinallyFlowInfo statementInfo;

                if (statement == ZR_NULL) {
                    continue;
                }
                if ((info.flow &
                     ZR_COMPILER_SEMANTIC_FINALLY_FLOW_FALLTHROUGH) == 0U ||
                    !compiler_semantic_cfg_finally_protected_flow(
                            statement, &statementInfo)) {
                    return ZR_FALSE;
                }
                info.flow =
                        (info.flow &
                         ~ZR_COMPILER_SEMANTIC_FINALLY_FLOW_FALLTHROUGH) |
                        statementInfo.flow;
                info.abruptSiteCount += statementInfo.abruptSiteCount;
                info.exceptionalSiteCount +=
                        statementInfo.exceptionalSiteCount;
                if (statementInfo.completionExpression != ZR_NULL) {
                    info.completionExpression =
                            statementInfo.completionExpression;
                }
                if (statementInfo.exceptionExpression != ZR_NULL) {
                    info.exceptionExpression =
                            statementInfo.exceptionExpression;
                }
            }
        }
    } else if (node->type == ZR_AST_EXPRESSION_STATEMENT) {
        if (compiler_semantic_cfg_finally_is_supported_direct_call(
                    node->data.expressionStatement.expr)) {
            info.exceptionalSiteCount = 1U;
            info.exceptionExpression =
                    node->data.expressionStatement.expr;
        } else if (!compiler_semantic_cfg_expression_is_linear(
                           node->data.expressionStatement.expr)) {
            return ZR_FALSE;
        }
        info.flow = ZR_COMPILER_SEMANTIC_FINALLY_FLOW_FALLTHROUGH;
    } else if (node->type == ZR_AST_IF_EXPRESSION) {
        SZrCompilerSemanticFinallyFlowInfo thenInfo;
        SZrCompilerSemanticFinallyFlowInfo elseInfo;

        if (!node->data.ifExpression.isStatement ||
            !compiler_semantic_cfg_expression_is_linear(
                    node->data.ifExpression.condition) ||
            !compiler_semantic_cfg_finally_protected_flow(
                    node->data.ifExpression.thenExpr, &thenInfo) ||
            !compiler_semantic_cfg_finally_protected_flow(
                    node->data.ifExpression.elseExpr, &elseInfo) ||
            thenInfo.exceptionalSiteCount != 0U ||
            elseInfo.exceptionalSiteCount != 0U) {
            return ZR_FALSE;
        }
        info.flow = thenInfo.flow | elseInfo.flow;
        info.abruptSiteCount =
                thenInfo.abruptSiteCount + elseInfo.abruptSiteCount;
        info.completionExpression =
                thenInfo.completionExpression != ZR_NULL
                        ? thenInfo.completionExpression
                        : elseInfo.completionExpression;
    } else if (node->type == ZR_AST_RETURN_STATEMENT) {
        if (!compiler_semantic_cfg_expression_is_linear(
                    node->data.returnStatement.expr)) {
            return ZR_FALSE;
        }
        info.flow = ZR_COMPILER_SEMANTIC_FINALLY_FLOW_RETURN;
        info.abruptSiteCount = 1U;
        info.completionExpression = node->data.returnStatement.expr;
    } else if (node->type == ZR_AST_THROW_STATEMENT) {
        if (node->data.throwStatement.expr == ZR_NULL ||
            !compiler_semantic_cfg_expression_is_linear(
                    node->data.throwStatement.expr)) {
            return ZR_FALSE;
        }
        info.flow = ZR_COMPILER_SEMANTIC_FINALLY_FLOW_THROW;
        info.abruptSiteCount = 1U;
        info.completionExpression = node->data.throwStatement.expr;
    } else {
        return ZR_FALSE;
    }
    if (info.abruptSiteCount > 1U ||
        info.exceptionalSiteCount > 1U ||
        (info.exceptionalSiteCount != 0U &&
         info.abruptSiteCount != 0U) ||
        ((info.flow & ZR_COMPILER_SEMANTIC_FINALLY_FLOW_RETURN) != 0U &&
         (info.flow & ZR_COMPILER_SEMANTIC_FINALLY_FLOW_THROW) != 0U)) {
        return ZR_FALSE;
    }
    *outInfo = info;
    return ZR_TRUE;
}

static EZrSemanticIrOpcode compiler_semantic_cfg_finally_completion_opcode(
        TZrUInt32 flow) {
    if ((flow & ZR_COMPILER_SEMANTIC_FINALLY_FLOW_RETURN) != 0U) {
        return ZR_SEMANTIC_IR_RETURN;
    }
    if ((flow & ZR_COMPILER_SEMANTIC_FINALLY_FLOW_THROW) != 0U) {
        return ZR_SEMANTIC_IR_THROW;
    }
    return ZR_SEMANTIC_IR_INVALID;
}

TZrBool compiler_semantic_cfg_try_finally_is_supported(
        SZrCompilerState *cs,
        const SZrAstNode *node) {
    const SZrTryCatchFinallyStatement *statement;
    SZrCompilerSemanticFinallyFlowInfo flowInfo;

    if (cs == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT ||
        cs->preSemanticIrCfgTerminated ||
        cs->preSemanticIrCfgStartupSuppressed ||
        cs->preSemanticIrCfgStartupBlocked ||
        (cs->preSemanticIrCfgActive &&
         cs->preSemanticIrCfgBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) ||
        cs->preSemanticIrCfgCatchBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        cs->preSemanticIrCfgFinallyPlan != ZR_NULL ||
        compiler_has_active_scope_ownership_cleanups(cs)) {
        return ZR_FALSE;
    }
    statement = &node->data.tryCatchFinallyStatement;
    return (TZrBool)(
            statement->finallyBlock != ZR_NULL &&
            (statement->catchClauses == ZR_NULL ||
             statement->catchClauses->count == 0U) &&
            compiler_semantic_cfg_finally_protected_flow(
                    statement->block, &flowInfo) &&
            (flowInfo.exceptionalSiteCount == 0U ||
             compiler_semantic_cfg_finally_call_is_resolved(
                     cs, flowInfo.exceptionExpression)) &&
            compiler_semantic_cfg_finally_block_is_linear(
                    statement->finallyBlock));
}

static void compiler_semantic_cfg_finally_fail(
        SZrCompilerState *cs,
        SZrCompilerSemanticFinallyPlan *plan) {
    if (cs != ZR_NULL) {
        if (plan != ZR_NULL &&
            cs->preSemanticIrCfgCatchBlock == plan->exceptionBlock) {
            cs->preSemanticIrCfgCatchBlock =
                    ZR_PARSER_CFG_INVALID_BLOCK_ID;
            cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
        }
        if (cs->preSemanticIrCfgActive) {
            (void)compiler_semantic_cfg_abandon(cs);
        }
        cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
        if (cs->preSemanticIrCfgFinallyPlan == plan) {
            cs->preSemanticIrCfgFinallyPlan = ZR_NULL;
        }
    }
    if (plan != ZR_NULL) {
        memset(plan, 0, sizeof(*plan));
        plan->cleanupBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        plan->joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        plan->completionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        plan->exceptionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        plan->completionValueId = ZR_VALUE_ID_INVALID;
        plan->completionSelectorSlot = ZR_PARSER_SLOT_NONE;
        plan->completionPayloadSlot = ZR_PARSER_SLOT_NONE;
    }
}

static TZrBool compiler_semantic_cfg_finally_store_bool(
        SZrCompilerState *cs,
        TZrUInt32 destinationSlot,
        TZrBool value,
        SZrFileRange range) {
    SZrCompilerSemanticIrSlot *destination;
    SZrSemanticIrInstructionSpec spec;
    SZrTypeValue constant;
    TZrUInt32 constantPoolIndex;
    TZrValueId valueId;

    destination = compiler_semantic_ir_find_slot(cs, destinationSlot);
    if (cs == ZR_NULL || destination == ZR_NULL ||
        destination->typeId == ZR_SEMANTIC_ID_INVALID ||
        destination->placeId == ZR_PLACE_ID_INVALID) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsBool(cs->state, &constant, value);
    constantPoolIndex = add_constant(cs, &constant);
    valueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, destination->typeId, range);
    if (cs->hasError || valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_CONSTANT;
    spec.typeId = destination->typeId;
    spec.resultValueId = valueId;
    spec.constantPoolIndex = constantPoolIndex;
    spec.hasConstantPoolIndex = ZR_TRUE;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = range;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_STORE;
    spec.typeId = destination->typeId;
    spec.placeId = destination->placeId;
    spec.valueId = valueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = range;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    destination->valueId = valueId;
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_finally_store_value(
        SZrCompilerState *cs,
        TZrUInt32 destinationSlot,
        TZrValueId valueId,
        SZrFileRange range) {
    SZrCompilerSemanticIrSlot *destination;
    const SZrSemanticIrValue *value;
    SZrSemanticIrInstructionSpec spec;

    if (cs == ZR_NULL || valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    destination = compiler_semantic_ir_find_slot(cs, destinationSlot);
    value = ZrParser_SemanticIr_Value(&cs->preSemanticIr, valueId);
    if (destination == ZR_NULL || value == ZR_NULL ||
        destination->typeId == ZR_SEMANTIC_ID_INVALID ||
        destination->typeId != value->typeId ||
        destination->placeId == ZR_PLACE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_STORE;
    spec.typeId = destination->typeId;
    spec.placeId = destination->placeId;
    spec.valueId = valueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = range;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    destination->valueId = valueId;
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_finally_prepare_pending_type(
        SZrCompilerState *cs,
        SZrCompilerSemanticFinallyPlan *plan,
        const SZrInferredType *payloadType,
        SZrFileRange range) {
    SZrInferredType selectorType;
    TZrBool prepared;

    if (cs == ZR_NULL || plan == ZR_NULL || payloadType == ZR_NULL) {
        return ZR_FALSE;
    }
    compiler_advance_stack_to_fresh_slot(cs);
    plan->completionSelectorSlot = allocate_stack_slot(cs);
    plan->completionPayloadSlot = allocate_stack_slot(cs);
    ZrParser_InferredType_Init(
            cs->state, &selectorType, ZR_VALUE_TYPE_BOOL);
    prepared = (TZrBool)(
            compiler_semantic_ir_prepare_optional_merge(
                    cs, plan->completionSelectorSlot,
                    &selectorType, range) &&
            compiler_semantic_ir_prepare_optional_merge(
                    cs, plan->completionPayloadSlot,
                    payloadType, range) &&
            compiler_semantic_cfg_finally_store_bool(
                    cs, plan->completionSelectorSlot,
                    ZR_FALSE, range));
    ZrParser_InferredType_Free(cs->state, &selectorType);
    return prepared;
}

static TZrBool compiler_semantic_cfg_finally_prepare_pending_state(
        SZrCompilerState *cs,
        SZrCompilerSemanticFinallyPlan *plan,
        const SZrAstNode *completionExpression,
        TZrBool exceptionPayload,
        SZrFileRange range) {
    SZrInferredType payloadType;
    TZrBool prepared;

    if (cs == ZR_NULL || plan == ZR_NULL ||
        (!exceptionPayload && completionExpression == ZR_NULL)) {
        return ZR_FALSE;
    }
    ZrParser_InferredType_Init(
            cs->state, &payloadType, ZR_VALUE_TYPE_OBJECT);
    prepared = (TZrBool)(
            (exceptionPayload ||
             ZrParser_ExpressionType_Infer(
                     cs, (SZrAstNode *)completionExpression,
                     &payloadType)) &&
            compiler_semantic_cfg_finally_prepare_pending_type(
                    cs, plan, &payloadType, range));
    ZrParser_InferredType_Free(cs->state, &payloadType);
    return prepared;
}

TZrBool compiler_semantic_cfg_begin_try_finally(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompilerSemanticFinallyPlan *plan) {
    SZrParserCfg *cfg;
    SZrCompilerSemanticFinallyFlowInfo flowInfo;

    if (cs == ZR_NULL || node == ZR_NULL || plan == ZR_NULL ||
        !compiler_semantic_cfg_try_finally_is_supported(cs, node)) {
        return ZR_FALSE;
    }
    memset(plan, 0, sizeof(*plan));
    plan->cleanupBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->completionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->exceptionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->completionValueId = ZR_VALUE_ID_INVALID;
    plan->completionSelectorSlot = ZR_PARSER_SLOT_NONE;
    plan->completionPayloadSlot = ZR_PARSER_SLOT_NONE;
    if (!compiler_semantic_cfg_finally_protected_flow(
                node->data.tryCatchFinallyStatement.block,
                &flowInfo)) {
        return ZR_FALSE;
    }
    plan->hasExceptionalEntry = (TZrBool)(
            flowInfo.exceptionalSiteCount != 0U);
    plan->completionOpcode = plan->hasExceptionalEntry
            ? ZR_SEMANTIC_IR_THROW
            : compiler_semantic_cfg_finally_completion_opcode(flowInfo.flow);
    plan->hasFallthrough = (TZrBool)(
            (flowInfo.flow &
             ZR_COMPILER_SEMANTIC_FINALLY_FLOW_FALLTHROUGH) != 0U);
    if (!compiler_semantic_cfg_ensure_active(cs)) {
        return ZR_FALSE;
    }
    if (plan->completionOpcode != ZR_SEMANTIC_IR_INVALID &&
        plan->hasFallthrough &&
        !compiler_semantic_cfg_finally_prepare_pending_state(
                cs, plan, flowInfo.completionExpression,
                plan->hasExceptionalEntry,
                node->location)) {
        compiler_semantic_cfg_finally_fail(cs, plan);
        return ZR_FALSE;
    }
    cfg = &cs->preSemanticIr.cfg;
    plan->cleanupBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_CLEANUP,
            node->data.tryCatchFinallyStatement.finallyBlock);
    if (plan->hasExceptionalEntry) {
        plan->exceptionBlock = ZrParser_Cfg_AppendBlock(
                cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, node);
    }
    if (plan->completionOpcode != ZR_SEMANTIC_IR_INVALID) {
        plan->completionBlock = ZrParser_Cfg_AppendBlock(
                cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, node);
    }
    if (plan->completionOpcode == ZR_SEMANTIC_IR_INVALID ||
        plan->hasFallthrough) {
        plan->joinBlock = ZrParser_Cfg_AppendBlock(
                cs->state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, node);
    }
    if (plan->cleanupBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        (plan->hasExceptionalEntry &&
         plan->exceptionBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) ||
        (plan->completionOpcode != ZR_SEMANTIC_IR_INVALID &&
         plan->completionBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) ||
        ((plan->completionOpcode == ZR_SEMANTIC_IR_INVALID ||
          plan->hasFallthrough) &&
         plan->joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID)) {
        compiler_semantic_cfg_finally_fail(cs, plan);
        return ZR_FALSE;
    }
    plan->initialized = ZR_TRUE;
    cs->preSemanticIrCfgFinallyPlan = plan;
    if (plan->hasExceptionalEntry) {
        cs->preSemanticIrCfgCatchBlock = plan->exceptionBlock;
        cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_completion_through_finally_is_active(
        const SZrCompilerState *cs,
        EZrSemanticIrOpcode completionOpcode) {
    const SZrCompilerSemanticFinallyPlan *plan =
            cs != ZR_NULL ? cs->preSemanticIrCfgFinallyPlan : ZR_NULL;

    return (TZrBool)((completionOpcode == ZR_SEMANTIC_IR_RETURN ||
                      completionOpcode == ZR_SEMANTIC_IR_THROW) &&
                     plan != ZR_NULL && plan->initialized &&
                     plan->completionOpcode == completionOpcode &&
                     !plan->completionPending &&
                     !plan->cleanupEntered);
}

static TZrBool compiler_semantic_cfg_redirect_completion_through_finally(
        SZrCompilerState *cs,
        TZrUInt32 valueSlot,
        SZrFileRange range,
        EZrSemanticIrOpcode completionOpcode) {
    SZrCompilerSemanticFinallyPlan *plan;
    TZrValueId valueId;

    if (!compiler_semantic_cfg_completion_through_finally_is_active(
                cs, completionOpcode)) {
        return ZR_FALSE;
    }
    plan = cs->preSemanticIrCfgFinallyPlan;
    valueId = compiler_semantic_ir_slot_value(cs, valueSlot);
    if (valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    if (plan->hasFallthrough &&
        (!compiler_semantic_ir_store_optional_present(
                 cs, plan->completionPayloadSlot,
                 valueSlot, range) ||
         !compiler_semantic_cfg_finally_store_bool(
                 cs, plan->completionSelectorSlot,
                 ZR_TRUE, range))) {
        return ZR_FALSE;
    }
    if (!compiler_semantic_cfg_jump_edge(
                cs, plan->cleanupBlock, ZR_PARSER_CFG_EDGE_CLEANUP,
                cs->currentAst, range)) {
        return ZR_FALSE;
    }
    if (!plan->hasFallthrough) {
        plan->completionValueId = valueId;
    }
    plan->completionRange = range;
    plan->completionPending = ZR_TRUE;
    cs->preSemanticIrCfgBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgStart =
            (TZrUInt32)cs->preSemanticIr.instructions.length;
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_return_through_finally_is_active(
        const SZrCompilerState *cs) {
    return compiler_semantic_cfg_completion_through_finally_is_active(
            cs, ZR_SEMANTIC_IR_RETURN);
}

TZrBool compiler_semantic_cfg_redirect_return_through_finally(
        SZrCompilerState *cs,
        TZrUInt32 valueSlot,
        SZrFileRange range) {
    return compiler_semantic_cfg_redirect_completion_through_finally(
            cs, valueSlot, range, ZR_SEMANTIC_IR_RETURN);
}

TZrBool compiler_semantic_cfg_throw_through_finally_is_active(
        const SZrCompilerState *cs) {
    return compiler_semantic_cfg_completion_through_finally_is_active(
            cs, ZR_SEMANTIC_IR_THROW);
}

TZrBool compiler_semantic_cfg_redirect_throw_through_finally(
        SZrCompilerState *cs,
        TZrUInt32 valueSlot,
        SZrFileRange range) {
    return compiler_semantic_cfg_redirect_completion_through_finally(
            cs, valueSlot, range, ZR_SEMANTIC_IR_THROW);
}

static TZrBool compiler_semantic_cfg_finally_capture_exception(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompilerSemanticFinallyPlan *plan) {
    TZrUInt32 normalBlock = cs->preSemanticIrCfgBlock;
    TZrTypeId payloadTypeId = ZR_SEMANTIC_ID_INVALID;
    TZrValueId payloadValueId = ZR_VALUE_ID_INVALID;

    cs->preSemanticIrCfgCatchBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
    compiler_semantic_cfg_enter(cs, plan->exceptionBlock);
    if (!compiler_semantic_cfg_emit_exception_payload(
                cs, node->location, &payloadTypeId, &payloadValueId) ||
        !compiler_semantic_cfg_finally_store_value(
                cs, plan->completionPayloadSlot,
                payloadValueId, node->location) ||
        !compiler_semantic_cfg_finally_store_bool(
                cs, plan->completionSelectorSlot,
                ZR_TRUE, node->location) ||
        !compiler_semantic_cfg_jump_edge(
                cs, plan->cleanupBlock, ZR_PARSER_CFG_EDGE_CLEANUP,
                node, node->location)) {
        return ZR_FALSE;
    }
    ZR_UNUSED_PARAMETER(payloadTypeId);
    plan->completionRange = node->location;
    plan->completionPending = ZR_TRUE;
    compiler_semantic_cfg_enter(cs, normalBlock);
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_enter_try_finally_cleanup(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompilerSemanticFinallyPlan *plan) {
    if (cs == ZR_NULL || node == ZR_NULL || plan == ZR_NULL ||
        !plan->initialized || plan->cleanupEntered) {
        return ZR_FALSE;
    }
    if (plan->hasExceptionalEntry && !cs->preSemanticIrCfgActive) {
        compiler_semantic_cfg_finally_fail(cs, plan);
        return ZR_TRUE;
    }
    if (!cs->preSemanticIrCfgActive) {
        compiler_semantic_cfg_finally_fail(cs, plan);
        return ZR_FALSE;
    }
    if (plan->hasExceptionalEntry) {
        if (!cs->preSemanticIrCfgCatchUsed ||
            cs->preSemanticIrCfgCatchBlock != plan->exceptionBlock ||
            cs->preSemanticIrCfgBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
            compiler_semantic_cfg_finally_fail(cs, plan);
            return ZR_TRUE;
        }
        if (!compiler_semantic_cfg_finally_capture_exception(
                    cs, node, plan)) {
            compiler_semantic_cfg_finally_fail(cs, plan);
            return ZR_FALSE;
        }
    }
    if (plan->completionOpcode != ZR_SEMANTIC_IR_INVALID
                 ? (!plan->completionPending ||
                    (plan->hasFallthrough
                             ? (cs->preSemanticIrCfgBlock ==
                                        ZR_PARSER_CFG_INVALID_BLOCK_ID ||
                                !compiler_semantic_cfg_jump_edge(
                                        cs, plan->cleanupBlock,
                                        ZR_PARSER_CFG_EDGE_CLEANUP,
                                        node, node->location))
                             : cs->preSemanticIrCfgBlock !=
                                       ZR_PARSER_CFG_INVALID_BLOCK_ID))
                 : (cs->preSemanticIrCfgBlock ==
                            ZR_PARSER_CFG_INVALID_BLOCK_ID ||
                     !compiler_semantic_cfg_jump_edge(
                             cs, plan->cleanupBlock,
                             ZR_PARSER_CFG_EDGE_CLEANUP,
                             node, node->location))) {
        compiler_semantic_cfg_finally_fail(cs, plan);
        return ZR_FALSE;
    }
    compiler_semantic_cfg_enter(cs, plan->cleanupBlock);
    plan->cleanupEntered = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_complete_try_finally(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrCompilerSemanticFinallyPlan *plan) {
    if (cs == ZR_NULL || node == ZR_NULL || plan == ZR_NULL ||
        !plan->initialized || !plan->cleanupEntered ||
        !cs->preSemanticIrCfgActive ||
        cs->preSemanticIrCfgBlock != plan->cleanupBlock) {
        compiler_semantic_cfg_finally_fail(cs, plan);
        return ZR_FALSE;
    }
    if (plan->completionOpcode != ZR_SEMANTIC_IR_INVALID) {
        TZrBool terminated;

        if (!plan->completionPending) {
            compiler_semantic_cfg_finally_fail(cs, plan);
            return ZR_FALSE;
        }
        if (plan->hasFallthrough) {
            TZrValueId selectorValueId;
            TZrValueId payloadValueId;
            TZrBool previousAbruptIsLocal;

            if (!compiler_semantic_ir_load_optional_merge(
                        cs, plan->completionSelectorSlot,
                        node->location)) {
                compiler_semantic_cfg_finally_fail(cs, plan);
                return ZR_FALSE;
            }
            selectorValueId = compiler_semantic_ir_slot_value(
                    cs, plan->completionSelectorSlot);
            if (selectorValueId == ZR_VALUE_ID_INVALID ||
                !compiler_semantic_cfg_cleanup_dispatch(
                        cs, selectorValueId,
                        plan->completionBlock, plan->joinBlock,
                        node, node->location)) {
                compiler_semantic_cfg_finally_fail(cs, plan);
                return ZR_FALSE;
            }
            compiler_semantic_cfg_enter(cs, plan->completionBlock);
            if (!compiler_semantic_ir_load_optional_merge(
                        cs, plan->completionPayloadSlot,
                        plan->completionRange)) {
                compiler_semantic_cfg_finally_fail(cs, plan);
                return ZR_FALSE;
            }
            payloadValueId = compiler_semantic_ir_slot_value(
                    cs, plan->completionPayloadSlot);
            if (payloadValueId == ZR_VALUE_ID_INVALID) {
                compiler_semantic_cfg_finally_fail(cs, plan);
                return ZR_FALSE;
            }
            cs->preSemanticIrCfgFinallyPlan = ZR_NULL;
            previousAbruptIsLocal = cs->preSemanticIrCfgAbruptIsLocal;
            cs->preSemanticIrCfgAbruptIsLocal = ZR_TRUE;
            if (plan->completionOpcode == ZR_SEMANTIC_IR_RETURN) {
                terminated = compiler_semantic_cfg_terminate_return_value(
                        cs, payloadValueId,
                        plan->completionRange);
            } else if (plan->completionOpcode == ZR_SEMANTIC_IR_THROW) {
                terminated = compiler_semantic_cfg_terminate_throw_value(
                        cs, payloadValueId,
                        plan->completionRange);
            } else {
                terminated = ZR_FALSE;
            }
            cs->preSemanticIrCfgAbruptIsLocal = previousAbruptIsLocal;
            if (!terminated) {
                compiler_semantic_cfg_finally_fail(cs, plan);
                return ZR_FALSE;
            }
            compiler_semantic_cfg_enter(cs, plan->joinBlock);
        } else {
            if (plan->completionValueId == ZR_VALUE_ID_INVALID ||
                !compiler_semantic_cfg_jump_edge(
                        cs, plan->completionBlock,
                        ZR_PARSER_CFG_EDGE_CLEANUP,
                        node, node->location)) {
                compiler_semantic_cfg_finally_fail(cs, plan);
                return ZR_FALSE;
            }
            compiler_semantic_cfg_enter(cs, plan->completionBlock);
            cs->preSemanticIrCfgFinallyPlan = ZR_NULL;
            if (plan->completionOpcode == ZR_SEMANTIC_IR_RETURN) {
                terminated = compiler_semantic_cfg_terminate_return_value(
                        cs, plan->completionValueId,
                        plan->completionRange);
            } else if (plan->completionOpcode == ZR_SEMANTIC_IR_THROW) {
                terminated = compiler_semantic_cfg_terminate_throw_value(
                        cs, plan->completionValueId,
                        plan->completionRange);
            } else {
                terminated = ZR_FALSE;
            }
            if (!terminated) {
                compiler_semantic_cfg_finally_fail(cs, plan);
                return ZR_FALSE;
            }
        }
    } else {
        if (!compiler_semantic_cfg_jump_edge(
                    cs, plan->joinBlock, ZR_PARSER_CFG_EDGE_CLEANUP,
                    node, node->location)) {
            compiler_semantic_cfg_finally_fail(cs, plan);
            return ZR_FALSE;
        }
        compiler_semantic_cfg_enter(cs, plan->joinBlock);
        cs->preSemanticIrCfgFinallyPlan = ZR_NULL;
    }
    memset(plan, 0, sizeof(*plan));
    plan->cleanupBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->completionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->exceptionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->completionValueId = ZR_VALUE_ID_INVALID;
    plan->completionSelectorSlot = ZR_PARSER_SLOT_NONE;
    plan->completionPayloadSlot = ZR_PARSER_SLOT_NONE;
    return ZR_TRUE;
}
