#include "compile_expression_contiguous_view.h"

#include "zr_vm_common/zr_contract_conf.h"

#include <stdint.h>

typedef struct SZrCompilerContiguousViewContract {
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *source;
    SZrTypeMemberInfo *start;
    SZrTypeMemberInfo *length;
    TZrUInt32 sourceMemberId;
    TZrUInt32 startMemberId;
    TZrUInt32 lengthMemberId;
    TZrBool isMutable;
    TZrBool isReadonly;
} SZrCompilerContiguousViewContract;

static TZrBool contiguous_view_integer_literal(
        const SZrAstNode *node,
        TZrInt64 *outValue) {
    if (outValue != ZR_NULL) {
        *outValue = 0;
    }
    if (node == ZR_NULL || outValue == ZR_NULL ||
        node->type != ZR_AST_INTEGER_LITERAL) {
        return ZR_FALSE;
    }
    *outValue = node->data.integerLiteral.value;
    return ZR_TRUE;
}

static TZrBool contiguous_view_try_add_int64(
        TZrInt64 left,
        TZrInt64 right,
        TZrInt64 *outValue) {
    if (outValue == ZR_NULL ||
        (right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right)) {
        return ZR_FALSE;
    }
    *outValue = left + right;
    return ZR_TRUE;
}

static SZrTypeMemberInfo *contiguous_view_find_member_by_role(
        SZrTypePrototypeInfo *prototype,
        TZrUInt32 role) {
    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0u; index < prototype->members.length; index++) {
        SZrTypeMemberInfo *member = (SZrTypeMemberInfo *)ZrCore_Array_Get(
                &prototype->members, index);
        if (member != ZR_NULL && member->contractRole == role) {
            return member;
        }
    }
    return ZR_NULL;
}

static TZrBool contiguous_view_resolve_member_id(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *member,
        TZrUInt32 *outMemberId) {
    TZrUInt32 memberId;

    if (cs == ZR_NULL || member == ZR_NULL || member->name == ZR_NULL ||
        outMemberId == ZR_NULL) {
        return ZR_FALSE;
    }
    memberId = compiler_get_or_add_member_entry_for_type_member(
            cs, member->name, member, 0u);
    if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
        return ZR_FALSE;
    }
    *outMemberId = memberId;
    return ZR_TRUE;
}

static TZrBool contiguous_view_resolve_contract(
        SZrCompilerState *cs,
        SZrString *typeName,
        SZrCompilerContiguousViewContract *outContract) {
    SZrTypePrototypeInfo *prototype;
    TZrUInt64 protocolMask;

    if (outContract != ZR_NULL) {
        memset(outContract, 0, sizeof(*outContract));
    }
    if (cs == ZR_NULL || typeName == ZR_NULL || outContract == ZR_NULL) {
        return ZR_FALSE;
    }

    prototype = find_compiler_type_prototype(cs, typeName);
    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }
    protocolMask = prototype->protocolMask;
    outContract->isMutable =
            (protocolMask & ZR_PROTOCOL_BIT(
                                    ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_MUTABLE)) != 0u;
    outContract->isReadonly =
            (protocolMask & ZR_PROTOCOL_BIT(
                                    ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_READONLY)) != 0u;
    if (!outContract->isMutable && !outContract->isReadonly) {
        return ZR_FALSE;
    }

    outContract->prototype = prototype;
    outContract->source = contiguous_view_find_member_by_role(
            prototype, ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SOURCE);
    outContract->start = contiguous_view_find_member_by_role(
            prototype, ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_START);
    outContract->length = contiguous_view_find_member_by_role(
            prototype, ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH);
    return outContract->source != ZR_NULL && outContract->start != ZR_NULL &&
           outContract->length != ZR_NULL &&
           contiguous_view_resolve_member_id(
                   cs, outContract->source, &outContract->sourceMemberId) &&
           contiguous_view_resolve_member_id(
                   cs, outContract->start, &outContract->startMemberId) &&
           contiguous_view_resolve_member_id(
                   cs, outContract->length, &outContract->lengthMemberId);
}

static TZrBool contiguous_view_source_kind(
        const SZrTypePrototypeInfo *prototype,
        EZrSemanticContiguousSourceKind *outKind) {
    TZrBool isOwner;
    TZrBool isNativePinned;

    if (prototype == ZR_NULL || outKind == ZR_NULL) {
        return ZR_FALSE;
    }
    isOwner = (TZrBool)((prototype->protocolMask &
                         ZR_PROTOCOL_BIT(
                                 ZR_PROTOCOL_ID_CONTIGUOUS_SOURCE_OWNER)) != 0U);
    isNativePinned =
            (TZrBool)((prototype->protocolMask &
                       ZR_PROTOCOL_BIT(
                               ZR_PROTOCOL_ID_CONTIGUOUS_SOURCE_NATIVE_PINNED)) !=
                      0U);
    if (isOwner && isNativePinned) {
        return ZR_FALSE;
    }
    if (isOwner) {
        *outKind = ZR_SEMANTIC_CONTIGUOUS_SOURCE_OWNER;
    } else if (isNativePinned) {
        *outKind = ZR_SEMANTIC_CONTIGUOUS_SOURCE_NATIVE_PINNED;
    } else if ((prototype->protocolMask &
                ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ARRAY_LIKE)) != 0U) {
        *outKind = ZR_SEMANTIC_CONTIGUOUS_SOURCE_ARRAY;
    } else {
        *outKind = ZR_SEMANTIC_CONTIGUOUS_SOURCE_VIEW;
    }
    return ZR_TRUE;
}

static TZrUInt32 contiguous_view_emit_zero(
        SZrCompilerState *cs,
        SZrFileRange location) {
    SZrTypeValue zero;
    TZrUInt32 slot;

    if (cs == ZR_NULL) {
        return ZR_PARSER_SLOT_NONE;
    }
    slot = allocate_stack_slot(cs);
    if (slot == ZR_PARSER_SLOT_NONE) {
        return ZR_PARSER_SLOT_NONE;
    }
    ZrCore_Value_InitAsInt(cs->state, &zero, 0);
    emit_constant_to_slot_local(cs, slot, &zero, location);
    return cs->hasError ? ZR_PARSER_SLOT_NONE : slot;
}

static TZrBool contiguous_view_emit_false_jump(
        SZrCompilerState *cs,
        TZrUInt32 conditionSlot,
        TZrSize errorLabel) {
    TZrSize instructionIndex;

    if (cs == ZR_NULL || conditionSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    instructionIndex = cs->instructionCount;
    emit_instruction(
            cs,
            create_instruction_1(
                    ZR_INSTRUCTION_ENUM(JUMP_IF),
                    ZR_COMPILE_SLOT_U16(conditionSlot),
                    0));
    add_pending_jump(cs, instructionIndex, errorLabel);
    return !cs->hasError;
}

static TZrBool contiguous_view_emit_signed_condition(
        SZrCompilerState *cs,
        EZrInstructionCode opcode,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        TZrSize errorLabel) {
    TZrUInt32 conditionSlot = allocate_stack_slot(cs);

    if (conditionSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    emit_instruction(
            cs,
            create_instruction_2(
                    opcode,
                    ZR_COMPILE_SLOT_U16(conditionSlot),
                    ZR_COMPILE_SLOT_U16(leftSlot),
                    ZR_COMPILE_SLOT_U16(rightSlot)));
    return contiguous_view_emit_false_jump(cs, conditionSlot, errorLabel);
}

static TZrBool contiguous_view_emit_jump(
        SZrCompilerState *cs,
        TZrSize label) {
    TZrSize instructionIndex;

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    instructionIndex = cs->instructionCount;
    emit_instruction(
            cs,
            create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0u, 0));
    add_pending_jump(cs, instructionIndex, label);
    return !cs->hasError;
}

static TZrBool contiguous_view_emit_failure_block(
        SZrCompilerState *cs,
        TZrSize errorLabel,
        TZrSize endLabel,
        const TZrChar *message) {
    SZrString *text;
    TZrUInt32 messageSlot;

    if (cs == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!contiguous_view_emit_jump(cs, endLabel)) {
        return ZR_FALSE;
    }
    resolve_label(cs, errorLabel);
    text = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)message);
    messageSlot = emit_string_constant(cs, text);
    if (messageSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    emit_instruction(
            cs,
            create_instruction_1(
                    ZR_INSTRUCTION_ENUM(THROW),
                    ZR_COMPILE_SLOT_U16(messageSlot),
                    0));
    resolve_label(cs, endLabel);
    return !cs->hasError;
}

static TZrBool contiguous_view_load_state(
        SZrCompilerState *cs,
        const SZrCompilerContiguousViewContract *contract,
        TZrUInt32 receiverSlot,
        TZrUInt32 *outSourceSlot,
        TZrUInt32 *outStartSlot,
        TZrUInt32 *outLengthSlot,
        SZrFileRange location) {
    TZrUInt32 sourceSlot;
    TZrUInt32 startSlot;
    TZrUInt32 lengthSlot;

    if (cs == ZR_NULL || contract == ZR_NULL || outSourceSlot == ZR_NULL ||
        outStartSlot == ZR_NULL || outLengthSlot == ZR_NULL) {
        return ZR_FALSE;
    }
    sourceSlot = allocate_stack_slot(cs);
    startSlot = allocate_stack_slot(cs);
    lengthSlot = allocate_stack_slot(cs);
    if (sourceSlot == ZR_PARSER_SLOT_NONE || startSlot == ZR_PARSER_SLOT_NONE ||
        lengthSlot == ZR_PARSER_SLOT_NONE ||
        !emit_member_slot_get(
                cs,
                sourceSlot,
                receiverSlot,
                contract->sourceMemberId,
                location) ||
        !emit_member_slot_get(
                cs,
                startSlot,
                receiverSlot,
                contract->startMemberId,
                location) ||
        !emit_member_slot_get(
                cs,
                lengthSlot,
                receiverSlot,
                contract->lengthMemberId,
                location)) {
        return ZR_FALSE;
    }
    *outSourceSlot = sourceSlot;
    *outStartSlot = startSlot;
    *outLengthSlot = lengthSlot;
    return ZR_TRUE;
}

static TZrBool contiguous_view_store_state(
        SZrCompilerState *cs,
        const SZrCompilerContiguousViewContract *contract,
        TZrUInt32 resultSlot,
        TZrUInt32 sourceSlot,
        TZrUInt32 startSlot,
        TZrUInt32 lengthSlot,
        SZrFileRange location) {
    return cs != ZR_NULL && contract != ZR_NULL &&
           emit_member_slot_set(
                   cs,
                   sourceSlot,
                   resultSlot,
                   contract->sourceMemberId,
                   location) &&
           emit_member_slot_set(
                   cs,
                   startSlot,
                   resultSlot,
                   contract->startMemberId,
                   location) &&
           emit_member_slot_set(
                   cs,
                   lengthSlot,
                   resultSlot,
                   contract->lengthMemberId,
                   location);
}

static TZrBool contiguous_view_resolve_result_contract(
        SZrCompilerState *cs,
        TZrUInt32 resultSlot,
        const SZrInferredType *resultType,
        SZrCompilerContiguousViewContract *outContract) {
    SZrString *resultTypeName;

    if (cs == ZR_NULL || resultType == ZR_NULL || outContract == ZR_NULL ||
        !compiler_register_stack_slot_type_hint(cs, resultSlot, resultType)) {
        return ZR_FALSE;
    }
    resultTypeName = get_type_name_from_inferred_type(cs, resultType);
    return resultTypeName != ZR_NULL &&
           contiguous_view_resolve_contract(cs, resultTypeName, outContract);
}

static EZrCompilerContiguousViewLoweringResult
contiguous_view_lower_create(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        SZrString *receiverTypeName,
        TZrUInt32 receiverSlot,
        TZrPlaceId receiverPlaceId,
        TZrUInt32 resultSlot,
        const SZrInferredType *resultType,
        SZrFileRange location) {
    SZrTypePrototypeInfo *receiverPrototype;
    SZrTypeMemberInfo *receiverLength;
    SZrCompilerContiguousViewContract resultContract;
    EZrSemanticContiguousSourceKind sourceKind;
    TZrPlaceId sourcePlaceId;
    TZrUInt32 receiverLengthMemberId;
    TZrUInt32 zeroSlot;
    TZrUInt32 lengthSlot;

    if (memberInfo == ZR_NULL ||
        memberInfo->contractRole !=
                ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_CREATE) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_NOT_APPLICABLE;
    }
    receiverPrototype = find_compiler_type_prototype(cs, receiverTypeName);
    receiverLength = contiguous_view_find_member_by_role(
            receiverPrototype, ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH);
    if (receiverLength == ZR_NULL ||
        !contiguous_view_source_kind(receiverPrototype, &sourceKind) ||
        !contiguous_view_resolve_member_id(
                cs, receiverLength, &receiverLengthMemberId) ||
        !contiguous_view_resolve_result_contract(
                cs, resultSlot, resultType, &resultContract)) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    sourcePlaceId = receiverPlaceId != ZR_PLACE_ID_INVALID
                            ? receiverPlaceId
                            : compiler_semantic_ir_place_for_slot(
                                      cs, receiverSlot, location);
    zeroSlot = contiguous_view_emit_zero(cs, location);
    lengthSlot = allocate_stack_slot(cs);
    if (zeroSlot == ZR_PARSER_SLOT_NONE ||
        lengthSlot == ZR_PARSER_SLOT_NONE ||
        !emit_member_slot_get(
                cs,
                lengthSlot,
                receiverSlot,
                receiverLengthMemberId,
                location) ||
        !contiguous_view_store_state(
                cs,
                &resultContract,
                resultSlot,
                receiverSlot,
                zeroSlot,
                lengthSlot,
                location) ||
        !compiler_semantic_ir_record_contiguous_view(
                cs,
                resultSlot,
                sourcePlaceId,
                zeroSlot,
                lengthSlot,
                sourceKind,
                1U,
                resultContract.isReadonly,
                ZR_TRUE,
                0,
                ZR_FALSE,
                0,
                location)) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    return ZR_COMPILER_CONTIGUOUS_VIEW_LOWERED;
}

static EZrCompilerContiguousViewLoweringResult
contiguous_view_lower_copy_or_slice(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        SZrString *receiverTypeName,
        TZrUInt32 receiverSlot,
        TZrUInt32 argumentStartSlot,
        TZrUInt32 argumentCount,
        const SZrAstNodeArray *argumentNodes,
        TZrUInt32 resultSlot,
        const SZrInferredType *resultType,
        SZrFileRange location) {
    SZrCompilerContiguousViewContract sourceContract;
    SZrCompilerContiguousViewContract resultContract;
    TZrUInt32 sourceSlot;
    TZrUInt32 startSlot;
    TZrUInt32 lengthSlot;
    const SZrSemanticContiguousViewFact *parentFact;
    TZrPlaceId sourcePlaceId;
    EZrSemanticContiguousSourceKind sourceKind;
    TZrRegionId regionId;

    if (memberInfo == ZR_NULL ||
        (memberInfo->contractRole !=
                 ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SLICE &&
         memberInfo->contractRole !=
                 ZR_MEMBER_CONTRACT_ROLE_READONLY_VIEW_CONVERSION)) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_NOT_APPLICABLE;
    }
    if (!contiguous_view_resolve_contract(
                cs, receiverTypeName, &sourceContract) ||
        !contiguous_view_resolve_result_contract(
                cs, resultSlot, resultType, &resultContract) ||
        !contiguous_view_load_state(
                cs,
                &sourceContract,
                receiverSlot,
                &sourceSlot,
                &startSlot,
                &lengthSlot,
                location)) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    parentFact = compiler_semantic_ir_contiguous_view_fact_for_slot(
            cs, receiverSlot, location);
    sourcePlaceId =
            parentFact != ZR_NULL
                    ? parentFact->sourcePlaceId
                    : compiler_semantic_ir_place_for_slot(
                              cs, sourceSlot, location);
    sourceKind = parentFact != ZR_NULL
                         ? parentFact->sourceKind
                         : ZR_SEMANTIC_CONTIGUOUS_SOURCE_VIEW;
    regionId = parentFact != ZR_NULL ? parentFact->regionId : 1U;

    if (memberInfo->contractRole ==
            ZR_MEMBER_CONTRACT_ROLE_READONLY_VIEW_CONVERSION) {
        if (argumentCount != 0u || !sourceContract.isMutable ||
            !resultContract.isReadonly ||
            !contiguous_view_store_state(
                    cs,
                    &resultContract,
                    resultSlot,
                    sourceSlot,
                    startSlot,
                    lengthSlot,
                    location) ||
            !compiler_semantic_ir_record_contiguous_view(
                    cs,
                    resultSlot,
                    sourcePlaceId,
                    startSlot,
                    lengthSlot,
                    sourceKind,
                    regionId,
                    resultContract.isReadonly,
                    parentFact != ZR_NULL && parentFact->hasKnownStart,
                    parentFact != ZR_NULL ? parentFact->knownStart : 0,
                    parentFact != ZR_NULL && parentFact->hasKnownLength,
                    parentFact != ZR_NULL ? parentFact->knownLength : 0,
                    location)) {
            return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
        }
        return ZR_COMPILER_CONTIGUOUS_VIEW_LOWERED;
    }

    if (argumentCount != 2u) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    {
        TZrUInt32 sliceStartSlot = argumentStartSlot;
        TZrUInt32 sliceLengthSlot = argumentStartSlot + 1u;
        TZrUInt32 zeroSlot = contiguous_view_emit_zero(cs, location);
        TZrUInt32 remainingSlot;
        TZrUInt32 absoluteStartSlot;
        TZrInt64 sliceStart = 0;
        TZrInt64 sliceLength = 0;
        TZrInt64 absoluteStart = 0;
        TZrBool hasKnownSliceStart =
                (TZrBool)(argumentNodes != ZR_NULL &&
                          argumentNodes->count == 2U &&
                          contiguous_view_integer_literal(
                                  argumentNodes->nodes[0], &sliceStart));
        TZrBool hasKnownSliceLength =
                (TZrBool)(argumentNodes != ZR_NULL &&
                          argumentNodes->count == 2U &&
                          contiguous_view_integer_literal(
                                  argumentNodes->nodes[1], &sliceLength));
        TZrBool hasKnownAbsoluteStart =
                (TZrBool)(parentFact != ZR_NULL &&
                          parentFact->hasKnownStart &&
                          hasKnownSliceStart &&
                          contiguous_view_try_add_int64(
                                  parentFact->knownStart,
                                  sliceStart,
                                  &absoluteStart));
        TZrSize errorLabel = create_label(cs);
        TZrSize endLabel = create_label(cs);

        if (zeroSlot == ZR_PARSER_SLOT_NONE ||
            !contiguous_view_emit_signed_condition(
                    cs,
                    ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED),
                    sliceStartSlot,
                    zeroSlot,
                    errorLabel) ||
            !contiguous_view_emit_signed_condition(
                    cs,
                    ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED),
                    sliceLengthSlot,
                    zeroSlot,
                    errorLabel) ||
            !contiguous_view_emit_signed_condition(
                    cs,
                    ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED),
                    sliceStartSlot,
                    lengthSlot,
                    errorLabel)) {
            return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
        }
        remainingSlot = allocate_stack_slot(cs);
        absoluteStartSlot = allocate_stack_slot(cs);
        if (remainingSlot == ZR_PARSER_SLOT_NONE ||
            absoluteStartSlot == ZR_PARSER_SLOT_NONE) {
            return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
        }
        emit_instruction(
                cs,
                create_instruction_2(
                        ZR_INSTRUCTION_ENUM(SUB_SIGNED),
                        ZR_COMPILE_SLOT_U16(remainingSlot),
                        ZR_COMPILE_SLOT_U16(lengthSlot),
                        ZR_COMPILE_SLOT_U16(sliceStartSlot)));
        if (!contiguous_view_emit_signed_condition(
                    cs,
                    ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED),
                    sliceLengthSlot,
                    remainingSlot,
                    errorLabel)) {
            return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
        }
        emit_instruction(
                cs,
                create_instruction_2(
                        ZR_INSTRUCTION_ENUM(ADD_SIGNED),
                        ZR_COMPILE_SLOT_U16(absoluteStartSlot),
                        ZR_COMPILE_SLOT_U16(startSlot),
                        ZR_COMPILE_SLOT_U16(sliceStartSlot)));
        if (!contiguous_view_store_state(
                    cs,
                    &resultContract,
                    resultSlot,
                    sourceSlot,
                    absoluteStartSlot,
                    sliceLengthSlot,
                    location) ||
            !compiler_semantic_ir_record_contiguous_view(
                    cs,
                    resultSlot,
                    sourcePlaceId,
                    absoluteStartSlot,
                    sliceLengthSlot,
                    sourceKind,
                    regionId,
                    resultContract.isReadonly,
                    hasKnownAbsoluteStart,
                    absoluteStart,
                    hasKnownSliceLength,
                    sliceLength,
                    location) ||
            !contiguous_view_emit_failure_block(
                    cs,
                    errorLabel,
                    endLabel,
                    "Contiguous view slice out of range")) {
            return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
        }
    }
    return ZR_COMPILER_CONTIGUOUS_VIEW_LOWERED;
}

TZrBool compiler_contiguous_view_member_is_structural(
        const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    return memberInfo->contractRole ==
                   ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_CREATE ||
           memberInfo->contractRole ==
                   ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_SLICE ||
           memberInfo->contractRole ==
                   ZR_MEMBER_CONTRACT_ROLE_READONLY_VIEW_CONVERSION;
}

TZrBool compiler_contiguous_view_type_is_readonly(
        SZrCompilerState *cs,
        SZrString *typeName) {
    SZrCompilerContiguousViewContract contract;
    return contiguous_view_resolve_contract(cs, typeName, &contract) &&
           contract.isReadonly;
}

EZrCompilerContiguousViewLoweringResult
compiler_contiguous_view_lower_member_call(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo,
        SZrString *receiverTypeName,
        TZrUInt32 receiverSlot,
        TZrPlaceId receiverPlaceId,
        TZrUInt32 argumentStartSlot,
        TZrUInt32 argumentCount,
        const SZrAstNodeArray *argumentNodes,
        TZrUInt32 resultSlot,
        const SZrInferredType *resultType,
        SZrFileRange location) {
    EZrCompilerContiguousViewLoweringResult result;

    if (!compiler_contiguous_view_member_is_structural(memberInfo)) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_NOT_APPLICABLE;
    }
    result = contiguous_view_lower_create(
            cs,
            memberInfo,
            receiverTypeName,
            receiverSlot,
            receiverPlaceId,
            resultSlot,
            resultType,
            location);
    if (result != ZR_COMPILER_CONTIGUOUS_VIEW_NOT_APPLICABLE) {
        return result;
    }
    return contiguous_view_lower_copy_or_slice(
            cs,
            memberInfo,
            receiverTypeName,
            receiverSlot,
            argumentStartSlot,
            argumentCount,
            argumentNodes,
            resultSlot,
            resultType,
            location);
}

EZrCompilerContiguousViewLoweringResult
compiler_contiguous_view_lower_index_get(
        SZrCompilerState *cs,
        SZrString *receiverTypeName,
        TZrUInt32 receiverSlot,
        TZrUInt32 indexSlot,
        const SZrAstNode *indexExpression,
        TZrUInt32 resultSlot,
        SZrFileRange location) {
    SZrCompilerContiguousViewContract contract;
    TZrUInt32 sourceSlot;
    TZrUInt32 startSlot;
    TZrUInt32 lengthSlot;
    TZrUInt32 absoluteIndexSlot;
    TZrInt64 knownIndex = 0;
    TZrBool hasKnownIndex;
    TZrBool checkElided;
    TZrSize errorLabel = 0U;
    TZrSize endLabel = 0U;

    if (!contiguous_view_resolve_contract(cs, receiverTypeName, &contract)) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_NOT_APPLICABLE;
    }
    if (!contiguous_view_load_state(
                cs,
                &contract,
                receiverSlot,
                &sourceSlot,
                &startSlot,
                &lengthSlot,
                location)) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    hasKnownIndex = contiguous_view_integer_literal(
            indexExpression, &knownIndex);
    if (!compiler_semantic_ir_record_bounds_fact(
                cs,
                receiverSlot,
                indexSlot,
                lengthSlot,
                hasKnownIndex,
                knownIndex,
                location,
                &checkElided)) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    absoluteIndexSlot = allocate_stack_slot(cs);
    if (absoluteIndexSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    if (!checkElided) {
        TZrUInt32 zeroSlot = contiguous_view_emit_zero(cs, location);
        errorLabel = create_label(cs);
        endLabel = create_label(cs);
        if (zeroSlot == ZR_PARSER_SLOT_NONE ||
            !contiguous_view_emit_signed_condition(
                    cs,
                    ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED),
                    indexSlot,
                    zeroSlot,
                    errorLabel) ||
            !contiguous_view_emit_signed_condition(
                    cs,
                    ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED),
                    indexSlot,
                    lengthSlot,
                    errorLabel)) {
            return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
        }
    }
    emit_instruction(
            cs,
            create_instruction_2(
                    ZR_INSTRUCTION_ENUM(ADD_SIGNED),
                    ZR_COMPILE_SLOT_U16(absoluteIndexSlot),
                    ZR_COMPILE_SLOT_U16(startSlot),
                    ZR_COMPILE_SLOT_U16(indexSlot)));
    emit_instruction(
            cs,
            create_instruction_2(
                    ZR_INSTRUCTION_ENUM(GET_BY_INDEX),
                    ZR_COMPILE_SLOT_U16(resultSlot),
                    ZR_COMPILE_SLOT_U16(sourceSlot),
                    ZR_COMPILE_SLOT_U16(absoluteIndexSlot)));
    if (!checkElided && !contiguous_view_emit_failure_block(
                cs,
                errorLabel,
                endLabel,
                "Contiguous view index out of range")) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    return ZR_COMPILER_CONTIGUOUS_VIEW_LOWERED;
}

EZrCompilerContiguousViewLoweringResult
compiler_contiguous_view_lower_index_set(
        SZrCompilerState *cs,
        SZrString *receiverTypeName,
        TZrUInt32 receiverSlot,
        TZrUInt32 indexSlot,
        const SZrAstNode *indexExpression,
        TZrUInt32 valueSlot,
        SZrFileRange location) {
    SZrCompilerContiguousViewContract contract;
    TZrUInt32 sourceSlot;
    TZrUInt32 startSlot;
    TZrUInt32 lengthSlot;
    TZrUInt32 absoluteIndexSlot;
    TZrInt64 knownIndex = 0;
    TZrBool hasKnownIndex;
    TZrBool checkElided;
    TZrSize errorLabel = 0U;
    TZrSize endLabel = 0U;

    if (!contiguous_view_resolve_contract(cs, receiverTypeName, &contract)) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_NOT_APPLICABLE;
    }
    if (!contract.isMutable || !contiguous_view_load_state(
                cs,
                &contract,
                receiverSlot,
                &sourceSlot,
                &startSlot,
                &lengthSlot,
                location)) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    hasKnownIndex = contiguous_view_integer_literal(
            indexExpression, &knownIndex);
    if (!compiler_semantic_ir_record_bounds_fact(
                cs,
                receiverSlot,
                indexSlot,
                lengthSlot,
                hasKnownIndex,
                knownIndex,
                location,
                &checkElided)) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    absoluteIndexSlot = allocate_stack_slot(cs);
    if (absoluteIndexSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    if (!checkElided) {
        TZrUInt32 zeroSlot = contiguous_view_emit_zero(cs, location);
        errorLabel = create_label(cs);
        endLabel = create_label(cs);
        if (zeroSlot == ZR_PARSER_SLOT_NONE ||
            !contiguous_view_emit_signed_condition(
                    cs,
                    ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED),
                    indexSlot,
                    zeroSlot,
                    errorLabel) ||
            !contiguous_view_emit_signed_condition(
                    cs,
                    ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED),
                    indexSlot,
                    lengthSlot,
                    errorLabel)) {
            return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
        }
    }
    emit_instruction(
            cs,
            create_instruction_2(
                    ZR_INSTRUCTION_ENUM(ADD_SIGNED),
                    ZR_COMPILE_SLOT_U16(absoluteIndexSlot),
                    ZR_COMPILE_SLOT_U16(startSlot),
                    ZR_COMPILE_SLOT_U16(indexSlot)));
    emit_instruction(
            cs,
            create_instruction_2(
                    ZR_INSTRUCTION_ENUM(SET_BY_INDEX),
                    ZR_COMPILE_SLOT_U16(valueSlot),
                    ZR_COMPILE_SLOT_U16(sourceSlot),
                    ZR_COMPILE_SLOT_U16(absoluteIndexSlot)));
    if (!checkElided && !contiguous_view_emit_failure_block(
                cs,
                errorLabel,
                endLabel,
                "Contiguous view index out of range")) {
        return ZR_COMPILER_CONTIGUOUS_VIEW_ERROR;
    }
    return ZR_COMPILER_CONTIGUOUS_VIEW_LOWERED;
}
