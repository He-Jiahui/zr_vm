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

void compile_foreach_statement(SZrCompilerState *cs, SZrAstNode *node) {
    SZrForeachLoop *foreachLoop;
    SZrLoopLabel loopLabel;
    TZrUInt32 iterableSlot;
    TZrUInt32 iteratorSlot;
    TZrUInt32 moveNextSlot;
    TZrUInt32 currentValueSlot;
    TZrSize loopStartLabelId;
    TZrSize loopEndLabelId;
    TZrBool hasResolvedBindingType = ZR_FALSE;
    TZrBool bindingTypeInitialized = ZR_FALSE;
    TZrBool useDynamicIteratorOps = ZR_FALSE;
    TZrSize oldConstLocalVarLength;
    SZrInferredType bindingType;

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
    iteratorSlot = allocate_stack_slot(cs);
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
    }

    emit_instruction(
            cs, create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0));
    add_pending_jump(cs, cs->instructionCount - 1U, loopStartLabelId);
    resolve_label(cs, loopEndLabelId);

foreach_cleanup:
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
