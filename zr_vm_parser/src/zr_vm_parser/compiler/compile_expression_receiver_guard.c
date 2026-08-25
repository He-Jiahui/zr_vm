#include "compile_expression_internal.h"

#include "zr_vm_parser/semantic_facts.h"

typedef struct SZrReceiverGuardLoweringFrame {
    TZrUInt32 mergeSlot;
    TZrUInt32 guardedSlot;
    TZrSize nullLabelId;
    TZrSize endLabelId;
} SZrReceiverGuardLoweringFrame;

static TZrBool receiver_guard_segment_is_optional(const SZrAstNode *segment) {
    if (segment == ZR_NULL) {
        return ZR_FALSE;
    }
    if (segment->type == ZR_AST_MEMBER_EXPRESSION) {
        return segment->data.memberExpression.accessMode ==
               ZR_POSTFIX_ACCESS_OPTIONAL;
    }
    if (segment->type == ZR_AST_FUNCTION_CALL) {
        return segment->data.functionCall.accessMode ==
               ZR_POSTFIX_ACCESS_OPTIONAL;
    }
    return ZR_FALSE;
}

void compiler_receiver_guard_lowering_init(
        SZrCompilerState *cs,
        SZrReceiverGuardLoweringContext *context,
        TZrSize capacity) {
    if (cs != ZR_NULL && context != ZR_NULL) {
        ZrCore_Array_Init(
                cs->state,
                &context->frames,
                sizeof(SZrReceiverGuardLoweringFrame),
                capacity);
    }
}

void compiler_receiver_guard_lowering_free(
        SZrCompilerState *cs,
        SZrReceiverGuardLoweringContext *context) {
    if (cs != ZR_NULL && context != ZR_NULL && context->frames.isValid) {
        ZrCore_Array_Free(cs->state, &context->frames);
    }
}

TZrBool compiler_receiver_guard_prepare_facts(
        SZrCompilerState *cs,
        SZrAstNode *primaryNode,
        SZrAstNode *propertyNode,
        SZrAstNodeArray *members,
        EZrOwnershipQualifier rootOwnershipQualifier) {
    SZrInferredType propertyType;
    SZrInferredType expressionType;
    TZrBool needsGuardFacts =
            rootOwnershipQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK;

    if (cs == ZR_NULL || primaryNode == ZR_NULL || propertyNode == ZR_NULL ||
        members == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < members->count; index++) {
        SZrAstNode *segment = members->nodes[index];
        if (ZrParser_SemanticFacts_FindReceiverGuardByNode(
                    cs->semanticContext, segment) != ZR_NULL) {
            return ZR_TRUE;
        }
        if (receiver_guard_segment_is_optional(segment)) {
            needsGuardFacts = ZR_TRUE;
        }
    }

    ZrParser_InferredType_Init(cs->state, &propertyType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(cs, propertyNode, &propertyType)) {
        needsGuardFacts = (TZrBool)(needsGuardFacts || propertyType.isNullable ||
                propertyType.ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK);
    }
    ZrParser_InferredType_Free(cs->state, &propertyType);
    if (cs->hasError || !needsGuardFacts) {
        return (TZrBool)!cs->hasError;
    }

    ZrParser_InferredType_Init(cs->state, &expressionType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, primaryNode, &expressionType)) {
        ZrParser_InferredType_Free(cs->state, &expressionType);
        return ZR_FALSE;
    }
    ZrParser_InferredType_Free(cs->state, &expressionType);
    return (TZrBool)!cs->hasError;
}

static TZrBool receiver_guard_emit_working_value(
        SZrCompilerState *cs,
        const SZrReceiverGuardFact *fact,
        TZrUInt32 sourceSlot,
        TZrUInt32 destinationSlot) {
    TZrInstruction instruction;

    if (fact->kind == ZR_RECEIVER_GUARD_WEAK_WAKE) {
        instruction = create_instruction_2(
                ZR_INSTRUCTION_ENUM(OWN_WAKE),
                (TZrUInt16)destinationSlot,
                (TZrUInt16)sourceSlot,
                0u);
    } else {
        instruction = create_instruction_1(
                ZR_INSTRUCTION_ENUM(SET_STACK),
                (TZrUInt16)destinationSlot,
                (TZrInt32)sourceSlot);
    }
    emit_instruction(cs, instruction);
    return compiler_register_stack_slot_type_hint(
            cs, destinationSlot, &fact->guardedType);
}

TZrBool compiler_receiver_guard_begin_segment(
        SZrCompilerState *cs,
        SZrAstNode *segment,
        TZrSize segmentIndex,
        TZrUInt32 *ioCurrentSlot,
        SZrString **ioRootTypeName,
        TZrBool *ioRootIsTypeReference,
        EZrOwnershipQualifier *ioRootOwnershipQualifier,
        SZrReceiverGuardLoweringContext *context,
        TZrBool *outChangedSlot,
        TZrBool *outGuarded) {
    const SZrReceiverGuardFact *fact;
    TZrUInt32 sourceSlot;
    TZrUInt32 guardedSlot;

    if (outChangedSlot != ZR_NULL) {
        *outChangedSlot = ZR_FALSE;
    }
    if (outGuarded != ZR_NULL) {
        *outGuarded = ZR_FALSE;
    }
    if (cs == ZR_NULL || segment == ZR_NULL || ioCurrentSlot == ZR_NULL ||
        context == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    fact = ZrParser_SemanticFacts_FindReceiverGuardByNode(
            cs->semanticContext, segment);
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }
    if (fact->chainSegmentStart != segmentIndex) {
        ZrParser_Compiler_Error(
                cs, "Receiver guard fact does not match its chain segment", fact->range);
        return ZR_FALSE;
    }

    sourceSlot = *ioCurrentSlot;
    guardedSlot = sourceSlot;
    if (fact->mode == ZR_RECEIVER_GUARD_OPTIONAL) {
        SZrReceiverGuardLoweringFrame frame;

        frame.mergeSlot = allocate_fresh_stack_slot_after(cs, sourceSlot);
        frame.guardedSlot = allocate_fresh_stack_slot_after(cs, frame.mergeSlot);
        frame.nullLabelId = create_label(cs);
        frame.endLabelId = create_label(cs);
        if (frame.mergeSlot == ZR_PARSER_SLOT_NONE ||
            frame.guardedSlot == ZR_PARSER_SLOT_NONE ||
            frame.nullLabelId == ZR_PARSER_LABEL_ID_NONE ||
            frame.endLabelId == ZR_PARSER_LABEL_ID_NONE ||
            !receiver_guard_emit_working_value(
                    cs, fact, sourceSlot, frame.guardedSlot)) {
            ZrParser_Compiler_Error(
                    cs, "Failed to prepare optional receiver guard", fact->range);
            return ZR_FALSE;
        }

        guardedSlot = frame.guardedSlot;
        emit_instruction(
                cs,
                create_instruction_2(
                        ZR_INSTRUCTION_ENUM(JUMP_IF_NULL),
                        (TZrUInt16)guardedSlot,
                        0u,
                        0u));
        add_pending_jump(
                cs, cs->instructionCount - 1u, frame.nullLabelId);
        ZrCore_Array_Push(cs->state, &context->frames, &frame);
    } else {
        if (fact->kind == ZR_RECEIVER_GUARD_WEAK_WAKE) {
            guardedSlot = allocate_fresh_stack_slot_after(cs, sourceSlot);
            if (guardedSlot == ZR_PARSER_SLOT_NONE ||
                !receiver_guard_emit_working_value(
                        cs, fact, sourceSlot, guardedSlot)) {
                ZrParser_Compiler_Error(
                        cs, "Failed to wake direct weak receiver", fact->range);
                return ZR_FALSE;
            }
        }
        emit_instruction(
                cs,
                create_instruction_0(
                        ZR_INSTRUCTION_ENUM(REQUIRE_NON_NULL),
                        (TZrUInt16)guardedSlot));
    }

    *ioCurrentSlot = guardedSlot;
    if (ioRootTypeName != ZR_NULL) {
        *ioRootTypeName = fact->guardedType.typeName;
    }
    if (ioRootIsTypeReference != ZR_NULL) {
        *ioRootIsTypeReference = ZR_FALSE;
    }
    if (ioRootOwnershipQualifier != ZR_NULL) {
        *ioRootOwnershipQualifier = fact->guardedType.ownershipQualifier;
    }
    if (outChangedSlot != ZR_NULL) {
        *outChangedSlot = (TZrBool)(guardedSlot != sourceSlot);
    }
    if (outGuarded != ZR_NULL) {
        *outGuarded = ZR_TRUE;
    }
    return (TZrBool)!cs->hasError;
}

TZrBool compiler_receiver_guard_finish(
        SZrCompilerState *cs,
        TZrUInt32 *ioCurrentSlot,
        SZrReceiverGuardLoweringContext *context) {
    TZrUInt32 currentSlot;

    if (cs == ZR_NULL || ioCurrentSlot == ZR_NULL || context == ZR_NULL ||
        cs->hasError) {
        return ZR_FALSE;
    }

    currentSlot = *ioCurrentSlot;
    for (TZrSize index = context->frames.length; index > 0u; index--) {
        SZrReceiverGuardLoweringFrame *frame =
                (SZrReceiverGuardLoweringFrame *)ZrCore_Array_Get(
                        &context->frames, index - 1u);
        TZrSize jumpEndIndex;

        if (frame == ZR_NULL) {
            return ZR_FALSE;
        }

        emit_instruction(
                cs,
                create_instruction_1(
                        ZR_INSTRUCTION_ENUM(SET_STACK),
                        (TZrUInt16)frame->mergeSlot,
                        (TZrInt32)currentSlot));
        if (currentSlot != frame->mergeSlot) {
            emit_instruction(
                    cs,
                    create_instruction_0(
                            ZR_INSTRUCTION_ENUM(RESET_STACK_NULL),
                            (TZrUInt16)currentSlot));
        }
        if (frame->guardedSlot != currentSlot &&
            frame->guardedSlot != frame->mergeSlot) {
            emit_instruction(
                    cs,
                    create_instruction_0(
                            ZR_INSTRUCTION_ENUM(RESET_STACK_NULL),
                            (TZrUInt16)frame->guardedSlot));
        }
        jumpEndIndex = cs->instructionCount;
        emit_instruction(
                cs,
                create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0u, 0));
        add_pending_jump(cs, jumpEndIndex, frame->endLabelId);

        resolve_label(cs, frame->nullLabelId);
        emit_instruction(
                cs,
                create_instruction_0(
                        ZR_INSTRUCTION_ENUM(RESET_STACK_NULL),
                        (TZrUInt16)frame->mergeSlot));
        emit_instruction(
                cs,
                create_instruction_0(
                        ZR_INSTRUCTION_ENUM(RESET_STACK_NULL),
                        (TZrUInt16)frame->guardedSlot));
        resolve_label(cs, frame->endLabelId);
        currentSlot = frame->mergeSlot;
        collapse_stack_to_slot(cs, currentSlot);
    }

    *ioCurrentSlot = currentSlot;
    return (TZrBool)!cs->hasError;
}
