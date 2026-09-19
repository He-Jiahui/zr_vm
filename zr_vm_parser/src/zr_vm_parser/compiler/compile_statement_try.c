#include "compiler_internal.h"
#include "compile_statement_internal.h"

static SZrAstNodeArray *catch_clause_pattern(SZrAstNode *catchClauseNode) {
    if (catchClauseNode == ZR_NULL ||
        catchClauseNode->type != ZR_AST_CATCH_CLAUSE) {
        return ZR_NULL;
    }
    return catchClauseNode->data.catchClause.pattern;
}

static SZrAstNode *catch_clause_block(SZrAstNode *catchClauseNode) {
    if (catchClauseNode == ZR_NULL ||
        catchClauseNode->type != ZR_AST_CATCH_CLAUSE) {
        return ZR_NULL;
    }
    return catchClauseNode->data.catchClause.block;
}

static SZrString *catch_clause_annotation_type_name(
        SZrCompilerState *cs,
        SZrType *typeInfo) {
    if (cs == ZR_NULL || typeInfo == ZR_NULL || typeInfo->name == ZR_NULL) {
        return ZR_NULL;
    }
    while (typeInfo->subType != ZR_NULL) {
        typeInfo = typeInfo->subType;
    }
    if (typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return typeInfo->name->data.identifier.name;
    }
    return extract_type_name_string(cs, typeInfo);
}

static SZrString *catch_clause_type_name(
        SZrCompilerState *cs,
        SZrAstNode *catchClauseNode) {
    SZrAstNodeArray *pattern = catch_clause_pattern(catchClauseNode);
    SZrAstNode *parameter;

    if (cs == ZR_NULL || pattern == ZR_NULL || pattern->count != 1U) {
        return ZR_NULL;
    }
    parameter = pattern->nodes[0];
    if (parameter == ZR_NULL || parameter->type != ZR_AST_PARAMETER ||
        parameter->data.parameter.typeInfo == ZR_NULL ||
        parameter->data.parameter.typeInfo->name == ZR_NULL) {
        return ZR_NULL;
    }
    return catch_clause_annotation_type_name(
            cs, parameter->data.parameter.typeInfo);
}

static TZrUInt32 catch_clause_binding_slot(
        SZrCompilerState *cs,
        SZrAstNode *catchClauseNode) {
    SZrAstNodeArray *pattern = catch_clause_pattern(catchClauseNode);
    SZrAstNode *parameter;

    if (pattern == ZR_NULL || pattern->count == 0U) {
        return allocate_stack_slot(cs);
    }
    if (pattern->count != 1U) {
        ZrParser_Compiler_Error(
                cs, "Multiple catch parameters are not supported",
                catchClauseNode->location);
        return allocate_stack_slot(cs);
    }
    parameter = pattern->nodes[0];
    if (parameter == ZR_NULL || parameter->type != ZR_AST_PARAMETER ||
        parameter->data.parameter.name == ZR_NULL ||
        parameter->data.parameter.name->name == ZR_NULL) {
        return allocate_stack_slot(cs);
    }
    return allocate_local_var(cs, parameter->data.parameter.name->name);
}

void compile_try_catch_finally_statement(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrTryCatchFinallyStatement *stmt;
    TZrUInt32 catchClauseStartIndex;
    TZrUInt32 handlerIndex;
    SZrCompilerExceptionHandlerInfo handlerInfo;
    TZrSize finallyLabelId;
    TZrSize afterFinallyLabelId;
    TZrBool hasFinally;
    TZrBool pushedTryContext = ZR_FALSE;
    TZrBool hasSemanticCatch = ZR_FALSE;
    TZrBool hasSemanticFinally = ZR_FALSE;
    TZrBool isolateSemanticCatchBodies = ZR_FALSE;
    TZrBool semanticCatchHandlerTerminates = ZR_FALSE;
    SZrCompilerSemanticCatchPlan semanticCatchPlan;
    SZrCompilerSemanticFinallyPlan semanticFinallyPlan;
    SZrArray semanticEntrySlots;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    if (node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
        ZrParser_Compiler_Error(
                cs, "Expected try-catch-finally statement", node->location);
        return;
    }

    stmt = &node->data.tryCatchFinallyStatement;
    memset(&semanticCatchPlan, 0, sizeof(semanticCatchPlan));
    memset(&semanticFinallyPlan, 0, sizeof(semanticFinallyPlan));
    memset(&semanticEntrySlots, 0, sizeof(semanticEntrySlots));
    if (compiler_semantic_cfg_try_catch_is_supported(cs, node)) {
        hasSemanticCatch = compiler_semantic_cfg_begin_try_catch(
                cs, node, &semanticCatchPlan, &semanticEntrySlots);
    } else if (compiler_semantic_cfg_try_finally_is_supported(cs, node)) {
        hasSemanticFinally = compiler_semantic_cfg_begin_try_finally(
                cs, node, &semanticFinallyPlan);
    }
    if (!hasSemanticCatch && !hasSemanticFinally) {
        if (cs->preSemanticIrCfgActive &&
            !compiler_semantic_cfg_abandon(cs)) {
            ZrParser_Compiler_Error(
                    cs,
                    "Failed to abandon unsupported exception CFG",
                    node->location);
            return;
        }
        cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
        isolateSemanticCatchBodies = ZR_TRUE;
    }

    catchClauseStartIndex = (TZrUInt32)cs->catchClauseInfos.length;
    hasFinally = (TZrBool)(stmt->finallyBlock != ZR_NULL);
    finallyLabelId = hasFinally ? create_label(cs) : ZR_PARSER_LABEL_ID_NONE;
    afterFinallyLabelId = create_label(cs);

    if (stmt->catchClauses != ZR_NULL) {
        TZrSize index;

        for (index = 0U; index < stmt->catchClauses->count; index++) {
            SZrAstNode *catchClauseNode =
                    stmt->catchClauses->nodes[index];
            SZrCompilerCatchClauseInfo catchInfo;

            if (catchClauseNode == ZR_NULL) {
                continue;
            }
            catchInfo.typeName =
                    catch_clause_type_name(cs, catchClauseNode);
            catchInfo.targetLabelId = create_label(cs);
            ZrCore_Array_Push(
                    cs->state, &cs->catchClauseInfos, &catchInfo);
        }
    }

    handlerInfo.protectedStartInstructionOffset =
            (TZrMemoryOffset)cs->instructionCount;
    handlerInfo.finallyLabelId = finallyLabelId;
    handlerInfo.afterFinallyLabelId = afterFinallyLabelId;
    handlerInfo.catchClauseStartIndex = catchClauseStartIndex;
    handlerInfo.catchClauseCount = (TZrUInt32)(
            cs->catchClauseInfos.length - catchClauseStartIndex);
    handlerInfo.hasFinally = hasFinally;
    handlerIndex = (TZrUInt32)cs->exceptionHandlerInfos.length;
    ZrCore_Array_Push(
            cs->state, &cs->exceptionHandlerInfos, &handlerInfo);

    emit_instruction(
            cs,
            create_instruction_0(
                    ZR_INSTRUCTION_ENUM(TRY), (TZrUInt16)handlerIndex));

    if (hasFinally) {
        SZrCompilerTryContext tryContext;

        tryContext.handlerIndex = handlerIndex;
        tryContext.finallyLabelId = finallyLabelId;
        tryContext.scopeStackDepth = cs->scopeStack.length;
        ZrCore_Array_Push(
                cs->state, &cs->tryContextStack, &tryContext);
        pushedTryContext = ZR_TRUE;
    }

    if (stmt->block != ZR_NULL) {
        ZrParser_Statement_Compile(cs, stmt->block);
    }
    if (hasSemanticFinally &&
        !compiler_semantic_cfg_enter_try_finally_cleanup(
                cs, node, &semanticFinallyPlan)) {
        ZrParser_Compiler_Error(
                cs, "Failed to enter semantic finally cleanup",
                node->location);
        return;
    }

    emit_instruction(
            cs,
            create_instruction_0(
                    ZR_INSTRUCTION_ENUM(END_TRY), (TZrUInt16)handlerIndex));
    emit_jump_to_label(
            cs, hasFinally ? finallyLabelId : afterFinallyLabelId);

    if (stmt->catchClauses != ZR_NULL) {
        TZrSize index;

        for (index = 0U; index < stmt->catchClauses->count; index++) {
            SZrAstNode *catchClauseNode =
                    stmt->catchClauses->nodes[index];
            SZrCompilerCatchClauseInfo *catchInfo =
                    (SZrCompilerCatchClauseInfo *)ZrCore_Array_Get(
                            &cs->catchClauseInfos,
                            catchClauseStartIndex + index);
            SZrCompilerSemanticIrIsolation semanticCatchBodyIsolation;
            TZrBool hasSemanticCatchBodyIsolation = ZR_FALSE;
            TZrBool previousSemanticCfgAbruptIsLocal = ZR_FALSE;
            TZrBool restoreSemanticCfgAbruptMode = ZR_FALSE;
            TZrUInt32 bindingSlot;

            if (catchClauseNode == ZR_NULL || catchInfo == ZR_NULL) {
                continue;
            }

            resolve_label(cs, catchInfo->targetLabelId);
            enter_scope(cs);
            bindingSlot = catch_clause_binding_slot(cs, catchClauseNode);
            emit_instruction(
                    cs,
                    create_instruction_0(
                            ZR_INSTRUCTION_ENUM(CATCH),
                            (TZrUInt16)bindingSlot));
            if (hasSemanticCatch) {
                TZrBool enteredSemanticHandler = ZR_FALSE;

                semanticCatchHandlerTerminates =
                        compiler_semantic_cfg_try_catch_handler_terminates(
                                node, index);
                if (!compiler_semantic_cfg_enter_try_catch_handler(
                            cs, node, &semanticCatchPlan, index,
                            bindingSlot, &semanticEntrySlots,
                            &enteredSemanticHandler)) {
                    ZrParser_Compiler_Error(
                            cs,
                            "Failed to enter semantic catch handler",
                            catchClauseNode->location);
                    return;
                }
                hasSemanticCatch = enteredSemanticHandler;
                isolateSemanticCatchBodies =
                        (TZrBool)!enteredSemanticHandler;
                if (enteredSemanticHandler &&
                    semanticCatchHandlerTerminates) {
                    previousSemanticCfgAbruptIsLocal =
                            cs->preSemanticIrCfgAbruptIsLocal;
                    cs->preSemanticIrCfgAbruptIsLocal = ZR_TRUE;
                    restoreSemanticCfgAbruptMode = ZR_TRUE;
                }
            }
            if (isolateSemanticCatchBodies) {
                if (!compiler_semantic_ir_isolation_begin(
                            cs, &semanticCatchBodyIsolation)) {
                    ZrParser_Compiler_Error(
                            cs,
                            "Failed to isolate fallback catch body Semantic IR",
                            catchClauseNode->location);
                    return;
                }
                hasSemanticCatchBodyIsolation = ZR_TRUE;
            }
            if (!cs->hasError &&
                catch_clause_block(catchClauseNode) != ZR_NULL) {
                ZrParser_Statement_Compile(
                        cs, catch_clause_block(catchClauseNode));
            }
            if (restoreSemanticCfgAbruptMode) {
                cs->preSemanticIrCfgAbruptIsLocal =
                        previousSemanticCfgAbruptIsLocal;
            }
            if (hasSemanticCatchBodyIsolation) {
                compiler_semantic_ir_isolation_end(
                        cs, &semanticCatchBodyIsolation);
            }
            if (hasSemanticCatch) {
                if (!compiler_semantic_cfg_complete_try_catch(
                            cs, node, &semanticCatchPlan, index,
                            semanticCatchHandlerTerminates,
                            &semanticEntrySlots)) {
                    ZrParser_Compiler_Error(
                            cs,
                            "Failed to complete semantic catch CFG",
                            catchClauseNode->location);
                    return;
                }
                hasSemanticCatch = (TZrBool)(
                        index + 1U < stmt->catchClauses->count);
            }
            exit_scope(cs);
            emit_instruction(
                    cs,
                    create_instruction_0(
                            ZR_INSTRUCTION_ENUM(END_TRY),
                            (TZrUInt16)handlerIndex));
            emit_jump_to_label(
                    cs, hasFinally ? finallyLabelId : afterFinallyLabelId);
        }
    }

    if (pushedTryContext && cs->tryContextStack.length > 0U) {
        cs->tryContextStack.length--;
    }

    if (hasFinally) {
        resolve_label(cs, finallyLabelId);
        ZrParser_Statement_Compile(cs, stmt->finallyBlock);
        if (hasSemanticFinally &&
            !compiler_semantic_cfg_complete_try_finally(
                    cs, node, &semanticFinallyPlan)) {
            ZrParser_Compiler_Error(
                    cs, "Failed to complete semantic finally cleanup",
                    node->location);
            return;
        }
        emit_instruction(
                cs,
                create_instruction_0(
                        ZR_INSTRUCTION_ENUM(END_FINALLY),
                        (TZrUInt16)handlerIndex));
    }

    resolve_label(cs, afterFinallyLabelId);
}
