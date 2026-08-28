#include "compile_expression_internal.h"

#include "zr_vm_parser/semantic_facts.h"

typedef struct SZrReceiverGuardLoweringFrame {
    TZrUInt32 mergeSlot;
    TZrUInt32 guardedSlot;
    TZrSize nullLabelId;
    TZrSize endLabelId;
    TZrSize chainSegmentEnd;
    EZrReceiverGuardResultLift resultLift;
    SZrFileRange range;
    TZrBool hasOptionalBranch;
    TZrBool hasWakeCleanup;
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

static TZrBool receiver_guard_segment_access_mode(
        const SZrAstNode *segment,
        EZrPostfixAccessMode *outMode) {
    if (segment == ZR_NULL || outMode == ZR_NULL) {
        return ZR_FALSE;
    }
    if (segment->type == ZR_AST_MEMBER_EXPRESSION) {
        *outMode = segment->data.memberExpression.accessMode;
        return ZR_TRUE;
    }
    if (segment->type == ZR_AST_FUNCTION_CALL) {
        *outMode = segment->data.functionCall.accessMode;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static SZrAstNode *receiver_guard_expected_receiver(
        const SZrReceiverGuardLoweringContext *context,
        TZrSize segmentIndex) {
    if (context == ZR_NULL || context->segments == ZR_NULL ||
        segmentIndex >= context->segments->count) {
        return ZR_NULL;
    }
    return segmentIndex == 0u
                   ? context->propertyNode
                   : context->segments->nodes[segmentIndex - 1u];
}

void compiler_receiver_guard_lowering_init(
        SZrCompilerState *cs,
        SZrReceiverGuardLoweringContext *context,
        SZrAstNode *primaryNode,
        SZrAstNode *propertyNode,
        SZrAstNodeArray *segments) {
    if (cs != ZR_NULL && context != ZR_NULL) {
        TZrSize capacity = segments != ZR_NULL ? segments->count : 0u;

        context->primaryNode = primaryNode;
        context->propertyNode = propertyNode;
        context->segments = segments;
        context->chainSegmentCount = capacity;
        ZrCore_Array_Init(
                cs->state,
                &context->frames,
                sizeof(SZrReceiverGuardLoweringFrame),
                capacity);
    }
}

static TZrBool receiver_guard_validate_fact(
        SZrCompilerState *cs,
        const SZrReceiverGuardFact *fact,
        SZrAstNode *segment,
        TZrSize segmentIndex,
        const SZrReceiverGuardLoweringContext *context) {
    const SZrSemanticExpressionFact *chainResultFact;
    const SZrSemanticExpressionFact *receiverFact;
    const SZrInferredType *canonicalReceiverType;
    EZrPostfixAccessMode accessMode;
    EZrReceiverGuardKind expectedKind;
    EZrReceiverGuardMode expectedMode;
    EZrReceiverGuardResultLift expectedLift;
    SZrInferredType expectedGuardedType;

    if (fact->kind != ZR_RECEIVER_GUARD_NULL &&
        fact->kind != ZR_RECEIVER_GUARD_WEAK_WAKE) {
        ZrParser_Compiler_Error(
                cs, "Receiver guard fact has invalid kind", fact->range);
        return ZR_FALSE;
    }
    if (fact->mode != ZR_RECEIVER_GUARD_DIRECT &&
        fact->mode != ZR_RECEIVER_GUARD_OPTIONAL) {
        ZrParser_Compiler_Error(
                cs, "Receiver guard fact has invalid mode", fact->range);
        return ZR_FALSE;
    }
    if (fact->firstSegment != segment ||
        fact->node != segment ||
        fact->receiver != receiver_guard_expected_receiver(context, segmentIndex) ||
        fact->chainSegmentStart != segmentIndex) {
        ZrParser_Compiler_Error(
                cs, "Receiver guard fact does not match its chain segment", fact->range);
        return ZR_FALSE;
    }
    if (context == ZR_NULL || fact->chainSegmentEnd <= segmentIndex ||
        fact->chainSegmentEnd != context->chainSegmentCount) {
        ZrParser_Compiler_Error(
                cs, "Receiver guard fact has invalid chain bounds", fact->range);
        return ZR_FALSE;
    }

    if (!receiver_guard_segment_access_mode(segment, &accessMode)) {
        ZrParser_Compiler_Error(
                cs, "Receiver guard fact targets an unsupported chain segment", fact->range);
        return ZR_FALSE;
    }
    expectedMode = accessMode == ZR_POSTFIX_ACCESS_OPTIONAL
                           ? ZR_RECEIVER_GUARD_OPTIONAL
                           : ZR_RECEIVER_GUARD_DIRECT;
    if (fact->mode != expectedMode) {
        ZrParser_Compiler_Error(
                cs, "Receiver guard fact mode does not match access syntax", fact->range);
        return ZR_FALSE;
    }

    receiverFact = ZrParser_SemanticFacts_FindExpressionByNode(
            cs->semanticContext, fact->receiver);
    if (receiverFact == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs, "Receiver guard canonical receiver fact is missing", fact->range);
        return ZR_FALSE;
    }
    canonicalReceiverType = &receiverFact->inferredType;
    if (!ZrParser_InferredType_Equal(
                canonicalReceiverType, &fact->receiverType)) {
        ZrParser_Compiler_Error(
                cs,
                "Receiver guard fact receiver type does not match canonical receiver",
                fact->range);
        return ZR_FALSE;
    }

    if (canonicalReceiverType->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK) {
        expectedKind = ZR_RECEIVER_GUARD_WEAK_WAKE;
    } else if (canonicalReceiverType->isNullable) {
        expectedKind = ZR_RECEIVER_GUARD_NULL;
    } else {
        ZrParser_Compiler_Error(
                cs, "Receiver guard fact has an unguarded receiver type", fact->range);
        return ZR_FALSE;
    }
    if (fact->kind != expectedKind) {
        ZrParser_Compiler_Error(
                cs, "Receiver guard fact kind does not match receiver type", fact->range);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &expectedGuardedType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(
            cs->state, &expectedGuardedType, canonicalReceiverType);
    expectedGuardedType.isNullable = ZR_FALSE;
    if (expectedKind == ZR_RECEIVER_GUARD_WEAK_WAKE) {
        expectedGuardedType.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
    }
    if (!ZrParser_InferredType_Equal(
                &expectedGuardedType, &fact->guardedType)) {
        ZrParser_InferredType_Free(cs->state, &expectedGuardedType);
        ZrParser_Compiler_Error(
                cs, "Receiver guard fact has an invalid guarded type", fact->range);
        return ZR_FALSE;
    }
    ZrParser_InferredType_Free(cs->state, &expectedGuardedType);

    if (expectedMode == ZR_RECEIVER_GUARD_DIRECT) {
        expectedLift = ZR_RECEIVER_GUARD_RESULT_UNCHANGED;
    } else {
        chainResultFact = ZrParser_SemanticFacts_FindExpressionByNode(
                cs->semanticContext, context->primaryNode);
        if (chainResultFact == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs, "Receiver guard chain result fact is missing", fact->range);
            return ZR_FALSE;
        }
        expectedLift = chainResultFact->inferredType.baseType == ZR_VALUE_TYPE_NULL
                               ? ZR_RECEIVER_GUARD_RESULT_VOID_NOOP
                               : ZR_RECEIVER_GUARD_RESULT_NULLABLE;
    }
    if (fact->resultLift != expectedLift) {
        ZrParser_Compiler_Error(
                cs,
                "Receiver guard fact result lift does not match chain result",
                fact->range);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool receiver_guard_segment_requires_fact(
        SZrCompilerState *cs,
        SZrAstNode *segment,
        TZrSize segmentIndex,
        EZrOwnershipQualifier currentOwnershipQualifier,
        const SZrReceiverGuardLoweringContext *context) {
    const SZrSemanticExpressionFact *receiverFact;
    SZrAstNode *receiver;

    if (segment == ZR_NULL ||
        (segment->type != ZR_AST_MEMBER_EXPRESSION &&
         segment->type != ZR_AST_FUNCTION_CALL)) {
        return ZR_FALSE;
    }
    if (currentOwnershipQualifier == ZR_OWNERSHIP_QUALIFIER_WEAK) {
        return ZR_TRUE;
    }
    receiver = receiver_guard_expected_receiver(context, segmentIndex);
    receiverFact = ZrParser_SemanticFacts_FindExpressionByNode(
            cs != ZR_NULL ? cs->semanticContext : ZR_NULL, receiver);
    if (receiverFact != ZR_NULL &&
        (receiverFact->inferredType.isNullable ||
         receiverFact->inferredType.ownershipQualifier ==
                 ZR_OWNERSHIP_QUALIFIER_WEAK)) {
        return ZR_TRUE;
    }
    if (receiver_guard_segment_is_optional(segment)) {
        return ZR_TRUE;
    }
    if (receiverFact == ZR_NULL && context != ZR_NULL &&
        context->segments != ZR_NULL) {
        for (TZrSize index = 0u; index < context->segments->count; index++) {
            const SZrReceiverGuardFact *chainFact =
                    ZrParser_SemanticFacts_FindReceiverGuardByNode(
                            cs != ZR_NULL ? cs->semanticContext : ZR_NULL,
                            context->segments->nodes[index]);
            if (chainFact != ZR_NULL &&
                chainFact->chainSegmentStart > segmentIndex) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
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
    if (fact->kind == ZR_RECEIVER_GUARD_WEAK_WAKE) {
        emit_instruction(
                cs,
                create_instruction_0(
                        ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED),
                        (TZrUInt16)destinationSlot));
    }
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
        if (receiver_guard_segment_requires_fact(
                    cs,
                    segment,
                    segmentIndex,
                    ioRootOwnershipQualifier != ZR_NULL
                            ? *ioRootOwnershipQualifier
                            : ZR_OWNERSHIP_QUALIFIER_NONE,
                    context)) {
            ZrParser_Compiler_Error(
                    cs,
                    "Receiver guard fact is missing for guarded chain segment",
                    segment->location);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }
    if (!receiver_guard_validate_fact(
                cs, fact, segment, segmentIndex, context)) {
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
        frame.chainSegmentEnd = fact->chainSegmentEnd;
        frame.resultLift = fact->resultLift;
        frame.range = fact->range;
        frame.hasOptionalBranch = ZR_TRUE;
        frame.hasWakeCleanup =
                (TZrBool)(fact->kind == ZR_RECEIVER_GUARD_WEAK_WAKE);
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
            SZrReceiverGuardLoweringFrame frame;

            guardedSlot = allocate_fresh_stack_slot_after(cs, sourceSlot);
            if (guardedSlot == ZR_PARSER_SLOT_NONE ||
                !receiver_guard_emit_working_value(
                        cs, fact, sourceSlot, guardedSlot)) {
                ZrParser_Compiler_Error(
                        cs, "Failed to wake direct weak receiver", fact->range);
                return ZR_FALSE;
            }
            frame.mergeSlot = ZR_PARSER_SLOT_NONE;
            frame.guardedSlot = guardedSlot;
            frame.nullLabelId = ZR_PARSER_LABEL_ID_NONE;
            frame.endLabelId = ZR_PARSER_LABEL_ID_NONE;
            frame.chainSegmentEnd = fact->chainSegmentEnd;
            frame.resultLift = fact->resultLift;
            frame.range = fact->range;
            frame.hasOptionalBranch = ZR_FALSE;
            frame.hasWakeCleanup = ZR_TRUE;
            ZrCore_Array_Push(cs->state, &context->frames, &frame);
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

static void receiver_guard_emit_wake_cleanup(
        SZrCompilerState *cs,
        const SZrReceiverGuardLoweringFrame *frame,
        TZrUInt32 preservedSlot) {
    if (frame == ZR_NULL || !frame->hasWakeCleanup) {
        return;
    }
    if (frame->guardedSlot != preservedSlot) {
        emit_instruction(
                cs,
                create_instruction_0(
                        ZR_INSTRUCTION_ENUM(RESET_STACK_NULL),
                        (TZrUInt16)frame->guardedSlot));
    }
    emit_instruction(
            cs,
            create_instruction_0(
                    ZR_INSTRUCTION_ENUM(CLOSE_SCOPE),
                    1u));
}

static TZrBool receiver_guard_emit_absent_result(
        SZrCompilerState *cs,
        const SZrReceiverGuardLoweringFrame *frame) {
    switch (frame->resultLift) {
        case ZR_RECEIVER_GUARD_RESULT_NULLABLE:
        case ZR_RECEIVER_GUARD_RESULT_VOID_NOOP:
            emit_instruction(
                    cs,
                    create_instruction_0(
                            ZR_INSTRUCTION_ENUM(RESET_STACK_NULL),
                            (TZrUInt16)frame->mergeSlot));
            return (TZrBool)!cs->hasError;
        default:
            ZrParser_Compiler_Error(
                    cs,
                    "Receiver guard frame has invalid result lift",
                    frame->range);
            return ZR_FALSE;
    }
}

TZrBool compiler_receiver_guard_finish(
        SZrCompilerState *cs,
        TZrUInt32 *ioCurrentSlot,
        SZrReceiverGuardLoweringContext *context,
        TZrSize chainSegmentEnd) {
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
        if (chainSegmentEnd != context->chainSegmentCount ||
            frame->chainSegmentEnd != chainSegmentEnd) {
            ZrParser_Compiler_Error(
                    cs,
                    "Receiver guard frame does not match its chain end",
                    frame->range);
            return ZR_FALSE;
        }

        if (!frame->hasOptionalBranch) {
            receiver_guard_emit_wake_cleanup(cs, frame, currentSlot);
            continue;
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
        receiver_guard_emit_wake_cleanup(
                cs, frame, frame->guardedSlot);
        jumpEndIndex = cs->instructionCount;
        emit_instruction(
                cs,
                create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0u, 0));
        add_pending_jump(cs, jumpEndIndex, frame->endLabelId);

        resolve_label(cs, frame->nullLabelId);
        if (!receiver_guard_emit_absent_result(cs, frame)) {
            return ZR_FALSE;
        }
        emit_instruction(
                cs,
                create_instruction_0(
                        ZR_INSTRUCTION_ENUM(RESET_STACK_NULL),
                        (TZrUInt16)frame->guardedSlot));
        receiver_guard_emit_wake_cleanup(
                cs, frame, frame->guardedSlot);
        resolve_label(cs, frame->endLabelId);
        currentSlot = frame->mergeSlot;
        collapse_stack_to_slot(cs, currentSlot);
    }

    *ioCurrentSlot = currentSlot;
    return (TZrBool)!cs->hasError;
}
