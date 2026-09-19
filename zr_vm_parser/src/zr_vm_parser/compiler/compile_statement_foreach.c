#include "zr_vm_parser/compiler.h"

#include "compiler_internal.h"
#include "compile_statement_internal.h"
#include "zr_vm_parser/iteration_contract.h"

static TZrBool foreach_infer_element_type(
        SZrCompilerState *cs,
        SZrAstNode *iterableExpr,
        SZrInferredType *outType) {
    SZrInferredType iterableType;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || iterableExpr == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(
            cs->state, &iterableType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, iterableExpr, &iterableType)) {
        ZrParser_InferredType_Free(cs->state, &iterableType);
        return ZR_FALSE;
    }

    success = ZrParser_EnumeratorBinding_ResolveElementType(
            cs, &iterableType, outType);
    ZrParser_InferredType_Free(cs->state, &iterableType);
    return success;
}

static TZrBool foreach_should_use_dynamic_iterator_ops(
        SZrCompilerState *cs,
        SZrAstNode *iterableExpr) {
    SZrInferredType iterableType;
    TZrBool useDynamic = ZR_FALSE;

    if (cs == ZR_NULL || iterableExpr == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(
            cs->state, &iterableType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(cs, iterableExpr, &iterableType)) {
        useDynamic = (TZrBool)(
                iterableType.baseType == ZR_VALUE_TYPE_OBJECT &&
                iterableType.typeName == ZR_NULL &&
                iterableType.elementTypes.length == 0U);
    }
    ZrParser_InferredType_Free(cs->state, &iterableType);
    return useDynamic;
}

static TZrBool foreach_abandon_semantic_cfg(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgActive &&
        !compiler_semantic_cfg_abandon(cs)) {
        return ZR_FALSE;
    }
    cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool foreach_refresh_semantic_slot_snapshot(
        SZrCompilerState *cs,
        SZrArray *snapshot) {
    const SZrArray *slots;

    if (cs == ZR_NULL || snapshot == ZR_NULL ||
        !snapshot->isValid || snapshot->head == ZR_NULL ||
        snapshot->elementSize == 0U ||
        snapshot->length > snapshot->capacity) {
        return ZR_FALSE;
    }
    slots = &cs->preSemanticIrSlots;
    if (!slots->isValid || slots->head == ZR_NULL ||
        slots->elementSize != snapshot->elementSize ||
        slots->length < snapshot->length ||
        slots->length > slots->capacity) {
        return ZR_FALSE;
    }
    if (snapshot->length != 0U) {
        ZrCore_Memory_RawCopy(
                snapshot->head,
                slots->head,
                snapshot->length * snapshot->elementSize);
    }
    return ZR_TRUE;
}

static TZrBool foreach_lower_semantic_iterator_invoke(
        SZrCompilerState *cs,
        EZrSemanticIrOpcode opcode,
        TZrUInt32 operandSlot,
        TZrUInt32 resultSlot,
        TZrTypeId resultTypeId,
        SZrAstNode *node,
        SZrFileRange sourceRange,
        TZrBool beginInvokeBlock,
        TZrUInt32 *invokeBlock) {
    SZrSemanticIrInstructionSpec spec;
    const SZrSemanticIrValue *operandValue;
    TZrValueId operandValueId;
    TZrValueId resultValueId;

    if (cs == ZR_NULL || node == ZR_NULL ||
        resultTypeId == ZR_SEMANTIC_ID_INVALID ||
        resultSlot == ZR_PARSER_SLOT_NONE ||
        (opcode != ZR_SEMANTIC_IR_ITER_INIT &&
         opcode != ZR_SEMANTIC_IR_ITER_MOVE_NEXT &&
         opcode != ZR_SEMANTIC_IR_ITER_CURRENT)) {
        return ZR_FALSE;
    }
    operandValueId = compiler_semantic_ir_slot_value(cs, operandSlot);
    operandValue = ZrParser_SemanticIr_Value(
            &cs->preSemanticIr, operandValueId);
    if (operandValue == ZR_NULL ||
        operandValue->definitionInstructionId ==
                ZR_SEMANTIC_INSTRUCTION_ID_INVALID) {
        return ZR_FALSE;
    }
    if (beginInvokeBlock &&
        !compiler_semantic_cfg_begin_invoke(cs, node, sourceRange)) {
        return ZR_FALSE;
    }
    if (!cs->preSemanticIrCfgActive ||
        cs->preSemanticIrCfgBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    if (invokeBlock != ZR_NULL) {
        *invokeBlock = cs->preSemanticIrCfgBlock;
    }

    resultValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, resultTypeId, sourceRange);
    if (resultValueId == ZR_VALUE_ID_INVALID ||
        !compiler_semantic_ir_bind_result_value(
                cs,
                resultSlot,
                resultTypeId,
                resultValueId,
                sourceRange)) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = opcode;
    spec.typeId = resultTypeId;
    spec.resultValueId = resultValueId;
    spec.operands = &operandValueId;
    spec.operandCount = 1U;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return (TZrBool)(
            compiler_semantic_ir_emit(cs, &spec) &&
            compiler_semantic_cfg_split_invoke(cs, node, sourceRange));
}

static TZrBool foreach_initialize_semantic_binding(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot;
    SZrSemanticIrInstructionSpec spec;

    if (cs == ZR_NULL || stackSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    slot = compiler_semantic_ir_find_slot(cs, stackSlot);
    if (slot == ZR_NULL || slot->typeId == ZR_SEMANTIC_ID_INVALID ||
        slot->placeId == ZR_PLACE_ID_INVALID ||
        slot->valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_INITIALIZE;
    spec.typeId = slot->typeId;
    spec.placeId = slot->placeId;
    spec.valueId = slot->valueId;
    spec.symbolId = slot->symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

void compile_foreach_statement(SZrCompilerState *cs, SZrAstNode *node) {
    SZrArray semanticSlotSnapshot;
    SZrForeachLoop *foreachLoop;
    SZrLoopLabel loopLabel;
    TZrUInt32 iterableSlot;
    TZrUInt32 iteratorSlot;
    TZrUInt32 moveNextSlot;
    TZrUInt32 currentValueSlot;
    TZrUInt32 moveNextBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    TZrUInt32 currentBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    TZrUInt32 joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    TZrSize loopStartLabelId;
    TZrSize loopEndLabelId;
    TZrBool hasResolvedBindingType = ZR_FALSE;
    TZrBool bindingTypeInitialized = ZR_FALSE;
    TZrBool useDynamicIteratorOps = ZR_FALSE;
    TZrBool hasSemanticCfg = ZR_FALSE;
    TZrBool semanticCfgCandidate = ZR_FALSE;
    TZrBool hasSemanticSlotSnapshot = ZR_FALSE;
    TZrBool previousSemanticCfgStartupSuppressed = ZR_FALSE;
    TZrBool restoreSemanticCfgStartupSuppression = ZR_FALSE;
    TZrSize oldConstLocalVarLength;
    TZrTypeId iteratorTypeId = ZR_SEMANTIC_ID_INVALID;
    TZrTypeId moveNextTypeId = ZR_SEMANTIC_ID_INVALID;
    TZrTypeId elementTypeId = ZR_SEMANTIC_ID_INVALID;
    SZrInferredType bindingType;

    ZrCore_Array_Construct(&semanticSlotSnapshot);
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_FOREACH_LOOP) {
        ZrParser_Compiler_Error(
                cs, "Expected foreach statement", node->location);
        return;
    }

    foreachLoop = &node->data.foreachLoop;
    oldConstLocalVarLength = cs->constLocalVars.length;

    enter_scope(cs);
    enter_type_scope(cs);

    loopLabel.breakLabelId = create_label(cs);
    loopLabel.continueLabelId = create_label(cs);
    loopLabel.targetScopeStackDepth = cs->scopeStack.length;
    loopLabel.semanticBreakBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    loopLabel.semanticContinueBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    ZrCore_Array_Push(cs->state, &cs->loopLabelStack, &loopLabel);

    if (foreachLoop->typeInfo != ZR_NULL) {
        ZrParser_InferredType_Init(
                cs->state, &bindingType, ZR_VALUE_TYPE_OBJECT);
        bindingTypeInitialized = ZR_TRUE;
        hasResolvedBindingType = ZrParser_AstTypeToInferredType_Convert(
                cs, foreachLoop->typeInfo, &bindingType);
    } else if (foreachLoop->pattern != ZR_NULL &&
               foreachLoop->pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
        ZrParser_InferredType_Init(
                cs->state, &bindingType, ZR_VALUE_TYPE_OBJECT);
        bindingTypeInitialized = ZR_TRUE;
        hasResolvedBindingType = foreach_infer_element_type(
                cs, foreachLoop->expr, &bindingType);
    }

    if (cs->hasError) {
        goto foreach_cleanup;
    }

    useDynamicIteratorOps = foreach_should_use_dynamic_iterator_ops(
            cs, foreachLoop->expr);
    semanticCfgCandidate = (TZrBool)(
            compiler_semantic_cfg_foreach_is_supported(cs, node) &&
            hasResolvedBindingType &&
            !useDynamicIteratorOps &&
            !cs->preSemanticIrCfgTerminated &&
            !cs->preSemanticIrCfgStartupSuppressed &&
            !cs->preSemanticIrCfgStartupBlocked);
    if (!semanticCfgCandidate) {
        if (!foreach_abandon_semantic_cfg(cs)) {
            ZrParser_Compiler_Error(
                    cs,
                    "Failed to abandon unsupported semantic foreach CFG",
                    node->location);
            goto foreach_cleanup;
        }
        previousSemanticCfgStartupSuppressed =
                cs->preSemanticIrCfgStartupSuppressed;
        cs->preSemanticIrCfgStartupSuppressed = ZR_TRUE;
        restoreSemanticCfgStartupSuppression = ZR_TRUE;
    } else {
        hasSemanticSlotSnapshot = compiler_semantic_cfg_capture_slots(
                cs, &semanticSlotSnapshot);
        if (!hasSemanticSlotSnapshot) {
            ZrParser_Compiler_Error(
                    cs,
                    "Failed to snapshot semantic foreach values",
                    node->location);
            goto foreach_cleanup;
        }
    }

    ZrParser_Expression_Compile(cs, foreachLoop->expr);
    if (cs->hasError || cs->stackSlotCount == 0U) {
        goto foreach_cleanup;
    }

    iterableSlot = cs->lastExpressionSlot;
    if (iterableSlot == ZR_PARSER_SLOT_NONE) {
        ZrParser_Compiler_Error(
                cs,
                "Foreach iterable expression did not produce a value",
                foreachLoop->expr->location);
        goto foreach_cleanup;
    }
    if (semanticCfgCandidate &&
        !foreach_refresh_semantic_slot_snapshot(
                cs, &semanticSlotSnapshot)) {
        ZrParser_Compiler_Error(
                cs,
                "Failed to refresh semantic foreach values",
                node->location);
        goto foreach_cleanup;
    }
    if (semanticCfgCandidate) {
        TZrValueId iterableValueId =
                compiler_semantic_ir_slot_value(cs, iterableSlot);
        const SZrSemanticIrValue *iterableValue =
                ZrParser_SemanticIr_Value(
                        &cs->preSemanticIr, iterableValueId);

        if (iterableValue == ZR_NULL ||
            iterableValue->definitionInstructionId ==
                    ZR_SEMANTIC_INSTRUCTION_ID_INVALID) {
            if (!foreach_abandon_semantic_cfg(cs)) {
                ZrParser_Compiler_Error(
                        cs,
                        "Failed to abandon untyped semantic foreach CFG",
                        node->location);
                goto foreach_cleanup;
            }
            semanticCfgCandidate = ZR_FALSE;
            previousSemanticCfgStartupSuppressed =
                    cs->preSemanticIrCfgStartupSuppressed;
            cs->preSemanticIrCfgStartupSuppressed = ZR_TRUE;
            restoreSemanticCfgStartupSuppression = ZR_TRUE;
        } else {
            iteratorTypeId = ZrParser_CanonicalType_InternPrimitive(
                    cs->semanticContext, ZR_VALUE_TYPE_OBJECT);
            moveNextTypeId = ZrParser_CanonicalType_InternPrimitive(
                    cs->semanticContext, ZR_VALUE_TYPE_BOOL);
            elementTypeId = ZrParser_Semantic_RegisterInferredType(
                    cs->semanticContext,
                    &bindingType,
                    ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                    ZR_NULL,
                    ZR_NULL);
            if (iteratorTypeId == ZR_SEMANTIC_ID_INVALID ||
                moveNextTypeId == ZR_SEMANTIC_ID_INVALID ||
                elementTypeId == ZR_SEMANTIC_ID_INVALID) {
                ZrParser_Compiler_Error(
                        cs,
                        "Failed to prepare semantic foreach iterator init",
                        node->location);
                goto foreach_cleanup;
            }
        }
    }
    iteratorSlot = allocate_stack_slot(cs);
    if (semanticCfgCandidate) {
        if (!foreach_lower_semantic_iterator_invoke(
                    cs,
                    ZR_SEMANTIC_IR_ITER_INIT,
                    iterableSlot,
                    iteratorSlot,
                    iteratorTypeId,
                    node,
                    foreachLoop->expr->location,
                    ZR_TRUE,
                    ZR_NULL)) {
            ZrParser_Compiler_Error(
                    cs,
                    "Failed to lower semantic foreach iterator init",
                    node->location);
            goto foreach_cleanup;
        }
        hasSemanticCfg = ZR_TRUE;
    }
    emit_instruction(
            cs,
            create_instruction_2(
                    useDynamicIteratorOps
                            ? ZR_INSTRUCTION_ENUM(DYN_ITER_INIT)
                            : ZR_INSTRUCTION_ENUM(ITER_INIT),
                    (TZrUInt16)iteratorSlot,
                    (TZrUInt16)iterableSlot,
                    0));

    moveNextSlot = allocate_stack_slot(cs);
    if (foreachLoop->pattern != ZR_NULL &&
        foreachLoop->pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
        foreachLoop->pattern->data.identifier.name != ZR_NULL) {
        currentValueSlot = allocate_local_var(
                cs, foreachLoop->pattern->data.identifier.name);
        if (cs->typeEnv != ZR_NULL) {
            if (hasResolvedBindingType) {
                ZrParser_TypeEnvironment_RegisterVariable(
                        cs->state,
                        cs->typeEnv,
                        foreachLoop->pattern->data.identifier.name,
                        &bindingType);
            } else {
                SZrInferredType defaultType;
                ZrParser_InferredType_Init(
                        cs->state, &defaultType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_TypeEnvironment_RegisterVariable(
                        cs->state,
                        cs->typeEnv,
                        foreachLoop->pattern->data.identifier.name,
                        &defaultType);
                ZrParser_InferredType_Free(cs->state, &defaultType);
            }
        }
    } else {
        currentValueSlot = allocate_stack_slot(cs);
    }

    loopStartLabelId = loopLabel.continueLabelId;
    loopEndLabelId = loopLabel.breakLabelId;
    resolve_label(cs, loopStartLabelId);

    if (hasSemanticCfg &&
        !foreach_lower_semantic_iterator_invoke(
                cs,
                ZR_SEMANTIC_IR_ITER_MOVE_NEXT,
                iteratorSlot,
                moveNextSlot,
                moveNextTypeId,
                node,
                node->location,
                ZR_TRUE,
                &moveNextBlock)) {
        ZrParser_Compiler_Error(
                cs,
                "Failed to lower semantic foreach move-next",
                node->location);
        goto foreach_cleanup;
    }

    emit_instruction(
            cs,
            create_instruction_2(
                    useDynamicIteratorOps
                            ? ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT)
                            : ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT),
                    (TZrUInt16)moveNextSlot,
                    (TZrUInt16)iteratorSlot,
                    0));

    {
        TZrInstruction jumpIfInst = create_instruction_1(
                ZR_INSTRUCTION_ENUM(JUMP_IF),
                (TZrUInt16)moveNextSlot,
                0);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, loopEndLabelId);
    }

    if (hasSemanticCfg &&
        !compiler_semantic_cfg_branch_foreach(
                cs,
                moveNextSlot,
                node,
                &currentBlock,
                &joinBlock)) {
        ZrParser_Compiler_Error(
                cs,
                "Failed to record semantic foreach condition",
                node->location);
        goto foreach_cleanup;
    }
    if (hasSemanticCfg) {
        SZrLoopLabel *activeLoopLabel =
                (SZrLoopLabel *)ZrCore_Array_Get(
                        &cs->loopLabelStack,
                        cs->loopLabelStack.length - 1U);

        if (activeLoopLabel == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs,
                    "Failed to bind semantic foreach loop targets",
                    node->location);
            goto foreach_cleanup;
        }
        activeLoopLabel->semanticBreakBlockId = joinBlock;
        activeLoopLabel->semanticContinueBlockId = moveNextBlock;
        if (!compiler_semantic_ir_register_local(
                    cs,
                    foreachLoop->pattern->data.identifier.name,
                    currentValueSlot,
                    foreachLoop->pattern->location,
                    ZR_FALSE) ||
            !foreach_lower_semantic_iterator_invoke(
                    cs,
                    ZR_SEMANTIC_IR_ITER_CURRENT,
                    iteratorSlot,
                    currentValueSlot,
                    elementTypeId,
                    node,
                    foreachLoop->pattern->location,
                    ZR_FALSE,
                    ZR_NULL) ||
            !foreach_initialize_semantic_binding(
                    cs,
                    currentValueSlot,
                    foreachLoop->pattern->location)) {
            ZrParser_Compiler_Error(
                    cs,
                    "Failed to lower semantic foreach current value",
                    node->location);
            goto foreach_cleanup;
        }
    }

    emit_instruction(
            cs,
            create_instruction_2(
                    ZR_INSTRUCTION_ENUM(ITER_CURRENT),
                    (TZrUInt16)currentValueSlot,
                    (TZrUInt16)iteratorSlot,
                    0));

    if (foreachLoop->pattern != ZR_NULL) {
        if (foreachLoop->pattern->type == ZR_AST_DESTRUCTURING_OBJECT) {
            compile_destructuring_object(
                    cs, foreachLoop->pattern, ZR_NULL);
        } else if (foreachLoop->pattern->type ==
                   ZR_AST_DESTRUCTURING_ARRAY) {
            compile_destructuring_array(
                    cs, foreachLoop->pattern, ZR_NULL);
        }
        if (!cs->hasError && foreachLoop->isConst) {
            compile_statement_register_const_pattern_bindings(
                    cs, foreachLoop->pattern);
        }
    }

    if (foreachLoop->block != ZR_NULL) {
        ZrParser_Statement_Compile(cs, foreachLoop->block);
        if (cs->hasError) {
            goto foreach_cleanup;
        }
    }
    if (hasSemanticCfg && !cs->preSemanticIrCfgActive) {
        hasSemanticCfg = ZR_FALSE;
    }
    if (hasSemanticCfg &&
        cs->preSemanticIrCfgBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
        !compiler_semantic_cfg_jump(cs, moveNextBlock, node->location)) {
        ZrParser_Compiler_Error(
                cs,
                "Failed to record semantic foreach backedge",
                node->location);
        goto foreach_cleanup;
    }

    emit_instruction(
            cs, create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0));
    add_pending_jump(cs, cs->instructionCount - 1U, loopStartLabelId);
    resolve_label(cs, loopEndLabelId);

    if (hasSemanticCfg) {
        if (!compiler_semantic_cfg_restore_slots(
                    cs, &semanticSlotSnapshot)) {
            ZrParser_Compiler_Error(
                    cs,
                    "Failed to restore semantic foreach values",
                    node->location);
            goto foreach_cleanup;
        }
        compiler_semantic_cfg_enter(cs, joinBlock);
    }

foreach_cleanup:
    if (restoreSemanticCfgStartupSuppression) {
        cs->preSemanticIrCfgStartupSuppressed =
                previousSemanticCfgStartupSuppressed;
    }
    if (hasSemanticSlotSnapshot) {
        compiler_semantic_cfg_free_slots(cs, &semanticSlotSnapshot);
    }
    cs->constLocalVars.length = oldConstLocalVarLength;
    if (cs->loopLabelStack.length > 0U) {
        ZrCore_Array_Pop(&cs->loopLabelStack);
    }
    if (bindingTypeInitialized) {
        ZrParser_InferredType_Free(cs->state, &bindingType);
    }
    exit_scope(cs);
    exit_type_scope(cs);
}
