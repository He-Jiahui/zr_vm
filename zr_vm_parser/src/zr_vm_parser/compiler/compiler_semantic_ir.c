#include "compiler_internal.h"

typedef struct SZrCompilerSemanticIrSlot {
    TZrUInt32 stackSlot;
    TZrPlaceId placeId;
    TZrValueId valueId;
    TZrTypeId typeId;
    TZrSymbolId symbolId;
    TZrLoanId loanId;
    TZrRegionId regionId;
} SZrCompilerSemanticIrSlot;

static SZrCompilerSemanticIrSlot *compiler_semantic_ir_find_slot(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot) {
    TZrSize index;

    if (cs == ZR_NULL || !cs->preSemanticIrSlots.isValid) {
        return ZR_NULL;
    }
    for (index = cs->preSemanticIrSlots.length; index > 0U; index--) {
        SZrCompilerSemanticIrSlot *slot =
                (SZrCompilerSemanticIrSlot *)ZrCore_Array_Get(
                        &cs->preSemanticIrSlots, index - 1U);
        if (slot != ZR_NULL && slot->stackSlot == stackSlot) {
            return slot;
        }
    }
    return ZR_NULL;
}

static TZrBool compiler_semantic_ir_slot_is_unique_owner(
        const SZrCompilerState *cs,
        const SZrCompilerSemanticIrSlot *slot) {
    const SZrCanonicalTypeNode *type;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || slot == ZR_NULL ||
        slot->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    type = ZrParser_CanonicalType_Find(cs->semanticContext, slot->typeId);
    return (TZrBool)(type != ZR_NULL &&
                     type->kind == ZR_CANONICAL_TYPE_OWNER &&
                     type->data.owner.ownerKind == ZR_CANONICAL_OWNER_UNIQUE);
}

static TZrBool compiler_semantic_ir_is_receiver_loan(
        const SZrCompilerState *cs,
        TZrLoanId loanId) {
    if (cs == ZR_NULL || loanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
        !cs->preSemanticIrReceiverLoanIds.isValid) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U;
         index < cs->preSemanticIrReceiverLoanIds.length;
         index++) {
        const TZrLoanId *receiverLoanId =
                (const TZrLoanId *)ZrCore_Array_Get(
                        (SZrArray *)&cs->preSemanticIrReceiverLoanIds, index);
        if (receiverLoanId != ZR_NULL && *receiverLoanId == loanId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool compiler_semantic_ir_is_contiguous_view_source_loan(
        const SZrCompilerState *cs,
        TZrLoanId loanId) {
    TZrSize index;

    if (cs == ZR_NULL || loanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
        !cs->preSemanticIr.contiguousViewFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U;
         index < cs->preSemanticIr.contiguousViewFacts.length;
         index++) {
        const SZrSemanticContiguousViewFact *fact =
                ZrParser_SemanticIr_ContiguousViewFactAt(
                        &cs->preSemanticIr, index);
        if (fact != ZR_NULL && fact->sourceLoanId == loanId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool compiler_semantic_ir_emit(
        SZrCompilerState *cs,
        const SZrSemanticIrInstructionSpec *spec) {
    if (cs == ZR_NULL || spec == ZR_NULL || !cs->preSemanticIrInitialized) {
        return ZR_FALSE;
    }
    cs->preSemanticIrValidated = ZR_FALSE;
    return (TZrBool)(ZrParser_SemanticIr_Emit(&cs->preSemanticIr, spec) !=
                     ZR_SEMANTIC_INSTRUCTION_ID_INVALID);
}

TZrBool compiler_semantic_ir_record_iterator_yield(
        SZrCompilerState *cs,
        TZrTypeId elementTypeId,
        SZrFileRange sourceRange) {
    SZrSemanticIrInstructionSpec spec;
    TZrValueId valueId;

    if (cs == ZR_NULL || !cs->preSemanticIrInitialized ||
        elementTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    valueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, elementTypeId, sourceRange);
    if (valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_YIELD_VALUE;
    spec.typeId = elementTypeId;
    spec.valueId = valueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_YIELD_SUSPEND;
    spec.valueId = valueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_YIELD_RESUME;
    spec.valueId = valueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

TZrBool compiler_semantic_ir_record_iterator_complete(
        SZrCompilerState *cs,
        SZrFileRange sourceRange) {
    SZrSemanticIrInstructionSpec spec;

    if (cs == ZR_NULL || !cs->preSemanticIrInitialized) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_ITERATOR_COMPLETE;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

static const SZrSemanticIrInstruction *compiler_semantic_ir_last_instruction(
        const SZrCompilerState *cs) {
    if (cs == ZR_NULL || !cs->preSemanticIrInitialized ||
        cs->preSemanticIr.instructions.length == 0U) {
        return ZR_NULL;
    }
    return ZrParser_SemanticIr_InstructionAt(
            &cs->preSemanticIr,
            cs->preSemanticIr.instructions.length - 1U);
}

static EZrInstructionCode compiler_semantic_ir_exec_opcode(
        const SZrSemanticIrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
    switch (instruction->opcode) {
        case ZR_SEMANTIC_IR_LOAD:
            return ZR_INSTRUCTION_ENUM(GET_STACK);
        case ZR_SEMANTIC_IR_STORE:
            return ZR_INSTRUCTION_ENUM(SET_STACK);
        case ZR_SEMANTIC_IR_MOVE:
            return ZR_INSTRUCTION_ENUM(OWN_UNIQUE);
        case ZR_SEMANTIC_IR_DROP:
            return ZR_INSTRUCTION_ENUM(OWN_RELEASE);
        case ZR_SEMANTIC_IR_BORROW_SHARED:
            return ZR_INSTRUCTION_ENUM(OWN_VIEW_SHARED);
        case ZR_SEMANTIC_IR_BORROW_MUT:
            return ZR_INSTRUCTION_ENUM(OWN_VIEW_MUT);
        case ZR_SEMANTIC_IR_OWN_CONSTRUCT:
            switch (instruction->ownershipOperation) {
                case ZR_SEMANTIC_OWNERSHIP_UNIQUE:
                    return ZR_INSTRUCTION_ENUM(OWN_UNIQUE);
                case ZR_SEMANTIC_OWNERSHIP_SHARE:
                    return ZR_INSTRUCTION_ENUM(OWN_SHARE);
                case ZR_SEMANTIC_OWNERSHIP_WEAK:
                    return ZR_INSTRUCTION_ENUM(OWN_WEAK);
                case ZR_SEMANTIC_OWNERSHIP_UPGRADE:
                    return ZR_INSTRUCTION_ENUM(OWN_UPGRADE);
                case ZR_SEMANTIC_OWNERSHIP_INTO_GC_BOX:
                    return ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX);
                case ZR_SEMANTIC_OWNERSHIP_RETURN_TO_GC:
                    return ZR_INSTRUCTION_ENUM(OWN_RETURN_TO_GC);
                case ZR_SEMANTIC_OWNERSHIP_NONE:
                case ZR_SEMANTIC_OWNERSHIP_ENUM_MAX:
                default:
                    return ZR_INSTRUCTION_ENUM(ENUM_MAX);
            }
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static SZrCompilerSemanticIrSlot *compiler_semantic_ir_add_temporary_slot(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot slot;
    SZrParserPlaceBase base;
    SZrSemanticIrInstructionSpec spec;

    if (cs == ZR_NULL || !cs->preSemanticIrInitialized) {
        return ZR_NULL;
    }
    memset(&slot, 0, sizeof(slot));
    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_TEMPORARY;
    base.identity = stackSlot;
    slot.stackSlot = stackSlot;
    slot.placeId = ZrParser_PlaceGraph_AddBase(
            &cs->preSemanticIr.places,
            &base,
            ZR_SEMANTIC_ID_INVALID,
            sourceRange);
    if (slot.placeId == ZR_PLACE_ID_INVALID) {
        return ZR_NULL;
    }
    slot.valueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, ZR_SEMANTIC_ID_INVALID, sourceRange);
    if (slot.valueId == ZR_VALUE_ID_INVALID) {
        return ZR_NULL;
    }
    ZrCore_Array_Push(cs->state, &cs->preSemanticIrSlots, &slot);
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    spec.placeId = slot.placeId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_NULL;
    }
    spec.opcode = ZR_SEMANTIC_IR_INITIALIZE;
    spec.valueId = slot.valueId;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_NULL;
    }
    return (SZrCompilerSemanticIrSlot *)ZrCore_Array_Get(
            &cs->preSemanticIrSlots, cs->preSemanticIrSlots.length - 1U);
}

static SZrCompilerSemanticIrSlot *compiler_semantic_ir_materialize_slot(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot =
            compiler_semantic_ir_find_slot(cs, stackSlot);
    TZrSize index;

    if (slot != ZR_NULL || cs == ZR_NULL || !cs->localVars.isValid) {
        return slot;
    }
    for (index = cs->localVars.length; index > 0U; index--) {
        const SZrFunctionLocalVariable *local =
                (const SZrFunctionLocalVariable *)ZrCore_Array_Get(
                        &cs->localVars, index - 1U);
        if (local != ZR_NULL && local->stackSlot == stackSlot &&
            local->name != ZR_NULL) {
            if (compiler_semantic_ir_register_local(
                        cs,
                        local->name,
                        stackSlot,
                        sourceRange,
                        ZR_TRUE)) {
                return compiler_semantic_ir_find_slot(cs, stackSlot);
            }
            break;
        }
    }
    return compiler_semantic_ir_add_temporary_slot(
            cs, stackSlot, sourceRange);
}

static TZrLoanId compiler_semantic_ir_begin_receiver_call_internal(
        SZrCompilerState *cs,
        TZrPlaceId receiverPlaceId,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrBool useTwoPhaseMutableLoan,
        SZrFileRange sourceRange) {
    const SZrParserPlace *receiverPlace;
    SZrSemanticIrInstructionSpec spec;
    EZrSemanticLoanAccess access;
    EZrSemanticLoanPhase phase;
    TZrValueId borrowValueId;
    TZrLoanId loanId;

    if (cs == ZR_NULL || receiverPlaceId == ZR_PLACE_ID_INVALID ||
        receiverEffect == ZR_CANONICAL_RECEIVER_NONE) {
        return ZR_SEMANTIC_LOAN_ID_INVALID;
    }
    receiverPlace = ZrParser_PlaceGraph_Get(
            &cs->preSemanticIr.places, receiverPlaceId);
    if (receiverPlace == ZR_NULL) {
        return ZR_SEMANTIC_LOAN_ID_INVALID;
    }
    access = receiverEffect == ZR_CANONICAL_RECEIVER_READONLY
                     ? ZR_SEMANTIC_LOAN_SHARED
                     : ZR_SEMANTIC_LOAN_MUTABLE;
    phase = receiverEffect == ZR_CANONICAL_RECEIVER_MUTABLE &&
                    useTwoPhaseMutableLoan
                    ? ZR_SEMANTIC_LOAN_TWO_PHASE
                    : ZR_SEMANTIC_LOAN_IMMEDIATE;
    borrowValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, receiverPlace->typeId, sourceRange);
    if (borrowValueId == ZR_VALUE_ID_INVALID) {
        return ZR_SEMANTIC_LOAN_ID_INVALID;
    }
    loanId = ZrParser_SemanticIr_AddLoanEx(
            &cs->preSemanticIr,
            receiverPlaceId,
            access,
            phase,
            1U,
            sourceRange,
            sourceRange,
            borrowValueId);
    if (loanId == ZR_SEMANTIC_LOAN_ID_INVALID) {
        return loanId;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = phase == ZR_SEMANTIC_LOAN_TWO_PHASE
                          ? ZR_SEMANTIC_IR_RESERVE_BORROW_MUT
                          : (access == ZR_SEMANTIC_LOAN_MUTABLE
                                     ? ZR_SEMANTIC_IR_BORROW_MUT
                                     : ZR_SEMANTIC_IR_BORROW_SHARED);
    spec.typeId = receiverPlace->typeId;
    spec.placeId = receiverPlaceId;
    spec.resultValueId = borrowValueId;
    spec.loanId = loanId;
    spec.regionId = 1U;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_SEMANTIC_LOAN_ID_INVALID;
    }
    ZrCore_Array_Push(
            cs->state, &cs->preSemanticIrReceiverLoanIds, &loanId);
    return loanId;
}

TZrPlaceId compiler_semantic_ir_place_for_slot(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot;

    if (cs == ZR_NULL || stackSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_PLACE_ID_INVALID;
    }
    slot = compiler_semantic_ir_materialize_slot(cs, stackSlot, sourceRange);
    return slot != ZR_NULL ? slot->placeId : ZR_PLACE_ID_INVALID;
}

TZrBool compiler_semantic_ir_bind_slot_to_place(
        SZrCompilerState *cs,
        TZrUInt32 stackSlot,
        TZrPlaceId placeId) {
    const SZrParserPlace *place;
    SZrCompilerSemanticIrSlot slot;

    if (cs == ZR_NULL || stackSlot == ZR_PARSER_SLOT_NONE ||
        placeId == ZR_PLACE_ID_INVALID ||
        compiler_semantic_ir_find_slot(cs, stackSlot) != ZR_NULL) {
        return ZR_FALSE;
    }
    place = ZrParser_PlaceGraph_Get(&cs->preSemanticIr.places, placeId);
    if (place == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&slot, 0, sizeof(slot));
    slot.stackSlot = stackSlot;
    slot.placeId = placeId;
    slot.valueId = ZR_VALUE_ID_INVALID;
    slot.typeId = place->typeId;
    ZrCore_Array_Push(cs->state, &cs->preSemanticIrSlots, &slot);
    return ZR_TRUE;
}

const SZrSemanticContiguousViewFact *
compiler_semantic_ir_contiguous_view_fact_for_slot(
        SZrCompilerState *cs,
        TZrUInt32 viewSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot;

    (void)sourceRange;
    if (cs == ZR_NULL || viewSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_NULL;
    }
    slot = compiler_semantic_ir_find_slot(cs, viewSlot);
    if (slot == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrParser_SemanticIr_FindContiguousViewFact(
            &cs->preSemanticIr, slot->placeId);
}

static TZrLoanId compiler_semantic_ir_begin_contiguous_source_loan(
        SZrCompilerState *cs,
        TZrPlaceId sourcePlaceId,
        EZrSemanticContiguousSourceKind sourceKind,
        TZrRegionId regionId,
        SZrFileRange sourceRange) {
    const SZrParserPlace *sourcePlace;
    EZrSemanticLoanAccess access;
    TZrValueId borrowValueId;
    TZrLoanId loanId;
    SZrSemanticIrInstructionSpec spec;

    if (cs == ZR_NULL || sourcePlaceId == ZR_PLACE_ID_INVALID ||
        (sourceKind != ZR_SEMANTIC_CONTIGUOUS_SOURCE_OWNER &&
         sourceKind != ZR_SEMANTIC_CONTIGUOUS_SOURCE_NATIVE_PINNED)) {
        return ZR_SEMANTIC_LOAN_ID_INVALID;
    }
    sourcePlace = ZrParser_PlaceGraph_Get(
            &cs->preSemanticIr.places, sourcePlaceId);
    if (sourcePlace == ZR_NULL) {
        return ZR_SEMANTIC_LOAN_ID_INVALID;
    }
    access = sourceKind == ZR_SEMANTIC_CONTIGUOUS_SOURCE_OWNER
                     ? ZR_SEMANTIC_LOAN_MUTABLE
                     : ZR_SEMANTIC_LOAN_SHARED;
    borrowValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, sourcePlace->typeId, sourceRange);
    if (borrowValueId == ZR_VALUE_ID_INVALID) {
        return ZR_SEMANTIC_LOAN_ID_INVALID;
    }
    loanId = ZrParser_SemanticIr_AddLoanEx(
            &cs->preSemanticIr,
            sourcePlaceId,
            access,
            ZR_SEMANTIC_LOAN_IMMEDIATE,
            regionId,
            sourceRange,
            sourceRange,
            borrowValueId);
    if (loanId == ZR_SEMANTIC_LOAN_ID_INVALID) {
        return loanId;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = access == ZR_SEMANTIC_LOAN_MUTABLE
                          ? ZR_SEMANTIC_IR_BORROW_MUT
                          : ZR_SEMANTIC_IR_BORROW_SHARED;
    spec.typeId = sourcePlace->typeId;
    spec.placeId = sourcePlaceId;
    spec.resultValueId = borrowValueId;
    spec.loanId = loanId;
    spec.regionId = regionId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_SEMANTIC_LOAN_ID_INVALID;
    }
    return loanId;
}

TZrBool compiler_semantic_ir_record_contiguous_view(
        SZrCompilerState *cs,
        TZrUInt32 viewSlot,
        TZrPlaceId sourcePlaceId,
        TZrUInt32 startSlot,
        TZrUInt32 lengthSlot,
        EZrSemanticContiguousSourceKind sourceKind,
        TZrRegionId regionId,
        TZrBool isReadOnly,
        TZrBool hasKnownStart,
        TZrInt64 knownStart,
        TZrBool hasKnownLength,
        TZrInt64 knownLength,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *view;
    SZrCompilerSemanticIrSlot *start;
    SZrCompilerSemanticIrSlot *length;
    SZrSemanticContiguousViewFact fact;
    TZrLoanId sourceLoanId = ZR_SEMANTIC_LOAN_ID_INVALID;

    if (cs == ZR_NULL || sourcePlaceId == ZR_PLACE_ID_INVALID ||
        startSlot == ZR_PARSER_SLOT_NONE || lengthSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    if (sourceKind == ZR_SEMANTIC_CONTIGUOUS_SOURCE_OWNER ||
        sourceKind == ZR_SEMANTIC_CONTIGUOUS_SOURCE_NATIVE_PINNED) {
        for (TZrSize index = cs->preSemanticIr.contiguousViewFacts.length;
             index > 0U;
             index--) {
            const SZrSemanticContiguousViewFact *existing =
                    ZrParser_SemanticIr_ContiguousViewFactAt(
                            &cs->preSemanticIr, index - 1U);
            if (existing != ZR_NULL &&
                existing->sourcePlaceId == sourcePlaceId &&
                existing->sourceKind == sourceKind &&
                existing->sourceLoanId != ZR_SEMANTIC_LOAN_ID_INVALID) {
                sourceLoanId = existing->sourceLoanId;
                break;
            }
        }
        if (sourceLoanId == ZR_SEMANTIC_LOAN_ID_INVALID) {
            sourceLoanId = compiler_semantic_ir_begin_contiguous_source_loan(
                    cs,
                    sourcePlaceId,
                    sourceKind,
                    regionId,
                    sourceRange);
            if (sourceLoanId == ZR_SEMANTIC_LOAN_ID_INVALID) {
                return ZR_FALSE;
            }
        }
    }

    view = compiler_semantic_ir_add_temporary_slot(
            cs, viewSlot, sourceRange);
    start = compiler_semantic_ir_materialize_slot(cs, startSlot, sourceRange);
    length = compiler_semantic_ir_materialize_slot(cs, lengthSlot, sourceRange);
    if (view == ZR_NULL || start == ZR_NULL || length == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&fact, 0, sizeof(fact));
    fact.viewPlaceId = view->placeId;
    fact.viewValueId = view->valueId;
    fact.sourcePlaceId = sourcePlaceId;
    fact.startValueId = start->valueId;
    fact.lengthValueId = length->valueId;
    fact.regionId = regionId;
    fact.sourceLoanId = sourceLoanId;
    fact.sourceKind = sourceKind;
    fact.isReadOnly = isReadOnly;
    fact.hasKnownStart = hasKnownStart;
    fact.hasKnownLength = hasKnownLength;
    fact.knownStart = knownStart;
    fact.knownLength = knownLength;
    fact.sourceRange = sourceRange;
    return ZrParser_SemanticIr_AddContiguousViewFact(
                   &cs->preSemanticIr, &fact) !=
           ZR_SEMANTIC_CONTIGUOUS_VIEW_FACT_ID_INVALID;
}

TZrBool compiler_semantic_ir_record_bounds_fact(
        SZrCompilerState *cs,
        TZrUInt32 viewSlot,
        TZrUInt32 indexSlot,
        TZrUInt32 lengthSlot,
        TZrBool hasKnownIndex,
        TZrInt64 knownIndex,
        SZrFileRange sourceRange,
        TZrBool *outCheckElided) {
    SZrCompilerSemanticIrSlot *view;
    SZrCompilerSemanticIrSlot *index;
    SZrCompilerSemanticIrSlot *length;
    const SZrSemanticContiguousViewFact *viewFact;
    SZrSemanticBoundsFact fact;

    if (outCheckElided != ZR_NULL) {
        *outCheckElided = ZR_FALSE;
    }
    if (cs == ZR_NULL || outCheckElided == ZR_NULL) {
        return ZR_FALSE;
    }
    view = compiler_semantic_ir_materialize_slot(cs, viewSlot, sourceRange);
    index = compiler_semantic_ir_materialize_slot(cs, indexSlot, sourceRange);
    length = compiler_semantic_ir_materialize_slot(cs, lengthSlot, sourceRange);
    if (view == ZR_NULL || index == ZR_NULL || length == ZR_NULL) {
        return ZR_FALSE;
    }
    viewFact = ZrParser_SemanticIr_FindContiguousViewFact(
            &cs->preSemanticIr, view->placeId);

    memset(&fact, 0, sizeof(fact));
    fact.contiguousViewFactId =
            viewFact != ZR_NULL
                    ? viewFact->factId
                    : ZR_SEMANTIC_CONTIGUOUS_VIEW_FACT_ID_INVALID;
    fact.viewPlaceId = view->placeId;
    fact.indexValueId = index->valueId;
    fact.lengthValueId = length->valueId;
    fact.hasKnownIndex = hasKnownIndex;
    fact.hasKnownLength =
            (TZrBool)(viewFact != ZR_NULL && viewFact->hasKnownLength);
    fact.knownIndex = knownIndex;
    fact.knownLength = fact.hasKnownLength ? viewFact->knownLength : 0;
    fact.lowerBoundProven =
            (TZrBool)(fact.hasKnownIndex && fact.knownIndex >= 0);
    fact.upperBoundProven =
            (TZrBool)(fact.hasKnownIndex && fact.hasKnownLength &&
                      fact.knownIndex < fact.knownLength);
    fact.checkElided =
            (TZrBool)(fact.lowerBoundProven && fact.upperBoundProven);
    fact.proofKind = fact.checkElided
                             ? ZR_SEMANTIC_BOUNDS_PROOF_CONSTANT_RANGE
                             : ZR_SEMANTIC_BOUNDS_PROOF_RUNTIME_CHECK;
    fact.sourceRange = sourceRange;
    if (ZrParser_SemanticIr_AddBoundsFact(&cs->preSemanticIr, &fact) ==
        ZR_SEMANTIC_BOUNDS_FACT_ID_INVALID) {
        return ZR_FALSE;
    }
    *outCheckElided = fact.checkElided;
    return ZR_TRUE;
}

static TZrBool compiler_semantic_ir_propagate_contiguous_view(
        SZrCompilerState *cs,
        TZrUInt32 destinationSlot,
        TZrUInt32 sourceSlot,
        SZrFileRange sourceRange,
        TZrBool destinationIsFreshValue) {
    const SZrSemanticContiguousViewFact *sourceFact;
    SZrCompilerSemanticIrSlot *destination;
    SZrSemanticContiguousViewFact copiedFact;

    sourceFact = compiler_semantic_ir_contiguous_view_fact_for_slot(
            cs, sourceSlot, sourceRange);
    if (sourceFact == ZR_NULL) {
        return ZR_TRUE;
    }
    destination = destinationIsFreshValue
                          ? compiler_semantic_ir_add_temporary_slot(
                                    cs, destinationSlot, sourceRange)
                          : compiler_semantic_ir_materialize_slot(
                                    cs, destinationSlot, sourceRange);
    if (destination == ZR_NULL) {
        return ZR_FALSE;
    }
    copiedFact = *sourceFact;
    copiedFact.factId = ZR_SEMANTIC_CONTIGUOUS_VIEW_FACT_ID_INVALID;
    copiedFact.viewPlaceId = destination->placeId;
    copiedFact.viewValueId = destination->valueId;
    copiedFact.sourceRange = sourceRange;
    return ZrParser_SemanticIr_AddContiguousViewFact(
                   &cs->preSemanticIr, &copiedFact) !=
           ZR_SEMANTIC_CONTIGUOUS_VIEW_FACT_ID_INVALID;
}

TZrPlaceId compiler_semantic_ir_project_field(
        SZrCompilerState *cs,
        TZrPlaceId parentPlaceId,
        TZrSymbolId fieldIdentity,
        SZrFileRange sourceRange) {
    SZrParserPlaceProjection projection;
    SZrSemanticIrInstructionSpec spec;
    TZrPlaceId placeId;

    if (cs == ZR_NULL || parentPlaceId == ZR_PLACE_ID_INVALID ||
        fieldIdentity == ZR_SEMANTIC_ID_INVALID) {
        return ZR_PLACE_ID_INVALID;
    }
    memset(&projection, 0, sizeof(projection));
    projection.kind = ZR_PARSER_PLACE_PROJECTION_FIELD;
    projection.data.symbolId = fieldIdentity;
    placeId = ZrParser_PlaceGraph_Project(
            &cs->preSemanticIr.places,
            parentPlaceId,
            &projection,
            ZR_SEMANTIC_ID_INVALID,
            sourceRange);
    if (placeId == ZR_PLACE_ID_INVALID) {
        return placeId;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_PLACE_PROJECT;
    spec.placeId = placeId;
    spec.symbolId = fieldIdentity;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_PLACE_ID_INVALID;
    }
    return placeId;
}

TZrPlaceId compiler_semantic_ir_project_index(
        SZrCompilerState *cs,
        TZrPlaceId parentPlaceId,
        TZrUInt32 indexStackSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *indexSlot;
    SZrParserPlaceProjection projection;
    SZrSemanticIrInstructionSpec spec;
    TZrPlaceId placeId;

    if (cs == ZR_NULL || parentPlaceId == ZR_PLACE_ID_INVALID ||
        indexStackSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_PLACE_ID_INVALID;
    }
    indexSlot = compiler_semantic_ir_materialize_slot(
            cs, indexStackSlot, sourceRange);
    if (indexSlot == ZR_NULL || indexSlot->valueId == ZR_VALUE_ID_INVALID) {
        return ZR_PLACE_ID_INVALID;
    }
    memset(&projection, 0, sizeof(projection));
    projection.kind = ZR_PARSER_PLACE_PROJECTION_INDEX;
    projection.data.valueId = indexSlot->valueId;
    placeId = ZrParser_PlaceGraph_Project(
            &cs->preSemanticIr.places,
            parentPlaceId,
            &projection,
            ZR_SEMANTIC_ID_INVALID,
            sourceRange);
    if (placeId == ZR_PLACE_ID_INVALID) {
        return placeId;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_PLACE_PROJECT;
    spec.placeId = placeId;
    spec.auxiliaryValueId = indexSlot->valueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_PLACE_ID_INVALID;
    }
    return placeId;
}

TZrLoanId compiler_semantic_ir_begin_receiver_call_place(
        SZrCompilerState *cs,
        TZrPlaceId receiverPlaceId,
        EZrCanonicalReceiverEffect receiverEffect,
        SZrFileRange sourceRange) {
    return compiler_semantic_ir_begin_receiver_call_internal(
            cs, receiverPlaceId, receiverEffect, ZR_TRUE, sourceRange);
}

TZrLoanId compiler_semantic_ir_begin_receiver_call(
        SZrCompilerState *cs,
        TZrUInt32 receiverSlot,
        EZrCanonicalReceiverEffect receiverEffect,
        SZrFileRange sourceRange) {
    TZrPlaceId receiverPlaceId = compiler_semantic_ir_place_for_slot(
            cs, receiverSlot, sourceRange);
    return compiler_semantic_ir_begin_receiver_call_internal(
            cs, receiverPlaceId, receiverEffect, ZR_TRUE, sourceRange);
}

TZrBool compiler_semantic_ir_activate_receiver_call(
        SZrCompilerState *cs,
        TZrLoanId loanId,
        SZrFileRange sourceRange) {
    const SZrSemanticIrLoanFact *loan;
    SZrSemanticIrInstructionSpec spec;

    if (cs == ZR_NULL || loanId == ZR_SEMANTIC_LOAN_ID_INVALID) {
        return ZR_TRUE;
    }
    loan = ZrParser_SemanticIr_Loan(&cs->preSemanticIr, loanId);
    if (loan == ZR_NULL || loan->phase == ZR_SEMANTIC_LOAN_IMMEDIATE) {
        return loan != ZR_NULL;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_ACTIVATE_LOAN;
    spec.placeId = loan->sourcePlaceId;
    spec.loanId = loanId;
    spec.regionId = loan->regionId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

TZrBool compiler_semantic_ir_project_receiver_call_result(
        SZrCompilerState *cs,
        TZrLoanId loanId,
        TZrUInt32 resultSlot,
        SZrFileRange sourceRange) {
    const SZrSemanticIrLoanFact *loan;
    const SZrParserPlace *sourcePlace;
    SZrCompilerSemanticIrSlot *result;
    SZrSemanticIrInstructionSpec spec;

    if (cs == ZR_NULL || loanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
        resultSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    loan = ZrParser_SemanticIr_Loan(&cs->preSemanticIr, loanId);
    if (loan == ZR_NULL || loan->createdByValueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    sourcePlace = ZrParser_PlaceGraph_Get(
            &cs->preSemanticIr.places, loan->sourcePlaceId);
    result = compiler_semantic_ir_add_temporary_slot(
            cs, resultSlot, sourceRange);
    if (sourcePlace == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    result->loanId = loanId;
    result->regionId = loan->regionId;

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_CONVERT;
    spec.typeId = sourcePlace->typeId;
    spec.valueId = loan->createdByValueId;
    spec.resultValueId = result->valueId;
    spec.regionId = loan->regionId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

static TZrTypeId compiler_semantic_ir_reference_pointee_type_id(
        const SZrCompilerState *cs,
        TZrTypeId referenceTypeId) {
    const SZrCanonicalTypeNode *type;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        referenceTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    type = ZrParser_CanonicalType_Find(
            cs->semanticContext, referenceTypeId);
    return type != ZR_NULL && type->kind == ZR_CANONICAL_TYPE_REF
                   ? type->data.refType.pointeeTypeId
                   : ZR_SEMANTIC_ID_INVALID;
}

static void compiler_semantic_ir_set_slot_type(
        SZrCompilerState *cs,
        SZrCompilerSemanticIrSlot *slot,
        TZrTypeId typeId) {
    SZrSemanticIrValue *value;

    if (cs == ZR_NULL || slot == ZR_NULL) {
        return;
    }
    slot->typeId = typeId;
    value = slot->valueId != ZR_VALUE_ID_INVALID
                    ? (SZrSemanticIrValue *)ZrCore_Array_Get(
                              &cs->preSemanticIr.values,
                              (TZrSize)slot->valueId - 1U)
                    : ZR_NULL;
    if (value != ZR_NULL) {
        value->typeId = typeId;
    }
}

TZrBool compiler_semantic_ir_record_property_ref_get(
        SZrCompilerState *cs,
        TZrUInt32 receiverSlot,
        TZrUInt32 resultSlot,
        const SZrTypeMemberInfo *propertyMember,
        const SZrTypeMemberInfo *getterAccessor,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *receiver;
    SZrCompilerSemanticIrSlot *result;
    const SZrSemanticIrLoanFact *loan = ZR_NULL;
    SZrSemanticIrInstructionSpec spec;
    TZrLoanId loanId = ZR_SEMANTIC_LOAN_ID_INVALID;
    TZrRegionId regionId = 1U;

    if (propertyMember == ZR_NULL ||
        propertyMember->structuredReturnType.referenceAccess ==
                ZR_REFERENCE_ACCESS_NONE) {
        return ZR_TRUE;
    }
    if (cs == ZR_NULL || getterAccessor == ZR_NULL ||
        receiverSlot == ZR_PARSER_SLOT_NONE ||
        resultSlot == ZR_PARSER_SLOT_NONE ||
        propertyMember->symbolId == ZR_SEMANTIC_ID_INVALID ||
        getterAccessor->symbolId == ZR_SEMANTIC_ID_INVALID ||
        propertyMember->propertyValueTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    receiver = compiler_semantic_ir_materialize_slot(
            cs, receiverSlot, sourceRange);
    if (receiver == ZR_NULL) {
        return ZR_FALSE;
    }
    result = compiler_semantic_ir_add_temporary_slot(
            cs, resultSlot, sourceRange);
    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    compiler_semantic_ir_set_slot_type(
            cs, result, propertyMember->propertyValueTypeId);
    if (getterAccessor->receiverEffect != ZR_CANONICAL_RECEIVER_NONE) {
        loanId = compiler_semantic_ir_begin_receiver_call_internal(
                cs,
                receiver->placeId,
                getterAccessor->receiverEffect,
                ZR_FALSE,
                sourceRange);
        if (loanId == ZR_SEMANTIC_LOAN_ID_INVALID) {
            return ZR_FALSE;
        }
        loan = ZrParser_SemanticIr_Loan(&cs->preSemanticIr, loanId);
        if (loan == ZR_NULL) {
            return ZR_FALSE;
        }
        if (!compiler_semantic_ir_activate_receiver_call(
                    cs, loanId, sourceRange)) {
            return ZR_FALSE;
        }
        regionId = loan->regionId;
    }
    result->loanId = loanId;
    result->regionId = regionId;

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_PROPERTY_REF_GET;
    spec.typeId = propertyMember->propertyValueTypeId;
    spec.placeId = receiver->placeId;
    spec.valueId = loan != ZR_NULL
                           ? loan->createdByValueId
                           : receiver->valueId;
    spec.resultValueId = result->valueId;
    spec.symbolId = propertyMember->symbolId;
    spec.accessorSymbolId = getterAccessor->symbolId;
    spec.loanId = loanId;
    spec.regionId = regionId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

static TZrPlaceId compiler_semantic_ir_project_dereference(
        SZrCompilerState *cs,
        const SZrCompilerSemanticIrSlot *reference,
        TZrTypeId pointeeTypeId,
        SZrFileRange sourceRange,
        TZrValueId *outDereferenceValueId) {
    SZrParserPlaceProjection projection;
    SZrSemanticIrInstructionSpec spec;
    TZrPlaceId placeId;
    TZrValueId valueId;

    if (outDereferenceValueId != ZR_NULL) {
        *outDereferenceValueId = ZR_VALUE_ID_INVALID;
    }
    if (cs == ZR_NULL || reference == ZR_NULL ||
        reference->placeId == ZR_PLACE_ID_INVALID ||
        reference->valueId == ZR_VALUE_ID_INVALID ||
        pointeeTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_PLACE_ID_INVALID;
    }
    memset(&projection, 0, sizeof(projection));
    projection.kind = ZR_PARSER_PLACE_PROJECTION_DEREFERENCE;
    placeId = ZrParser_PlaceGraph_Project(
            &cs->preSemanticIr.places,
            reference->placeId,
            &projection,
            pointeeTypeId,
            sourceRange);
    valueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, pointeeTypeId, sourceRange);
    if (placeId == ZR_PLACE_ID_INVALID ||
        valueId == ZR_VALUE_ID_INVALID) {
        return ZR_PLACE_ID_INVALID;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_DEREFERENCE;
    spec.typeId = pointeeTypeId;
    spec.placeId = placeId;
    spec.valueId = reference->valueId;
    spec.resultValueId = valueId;
    spec.loanId = reference->loanId;
    spec.regionId = reference->regionId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_PLACE_ID_INVALID;
    }
    if (outDereferenceValueId != ZR_NULL) {
        *outDereferenceValueId = valueId;
    }
    return placeId;
}

TZrBool compiler_semantic_ir_record_property_ref_load(
        SZrCompilerState *cs,
        TZrUInt32 referenceSlot,
        TZrUInt32 targetSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *reference;
    SZrCompilerSemanticIrSlot target;
    SZrSemanticIrInstructionSpec spec;
    TZrTypeId pointeeTypeId;
    TZrPlaceId placeId;
    TZrValueId dereferenceValueId;
    TZrValueId loadedValueId;

    reference = compiler_semantic_ir_find_slot(cs, referenceSlot);
    if (reference == ZR_NULL) {
        return ZR_FALSE;
    }
    pointeeTypeId = compiler_semantic_ir_reference_pointee_type_id(
            cs, reference->typeId);
    placeId = compiler_semantic_ir_project_dereference(
            cs,
            reference,
            pointeeTypeId,
            sourceRange,
            &dereferenceValueId);
    loadedValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, pointeeTypeId, sourceRange);
    if (placeId == ZR_PLACE_ID_INVALID ||
        loadedValueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_LOAD;
    spec.typeId = pointeeTypeId;
    spec.placeId = placeId;
    spec.valueId = dereferenceValueId;
    spec.resultValueId = loadedValueId;
    spec.loanId = reference->loanId;
    spec.regionId = reference->regionId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    memset(&target, 0, sizeof(target));
    target.stackSlot = targetSlot;
    target.placeId = placeId;
    target.valueId = loadedValueId;
    target.typeId = pointeeTypeId;
    target.regionId = reference->regionId;
    ZrCore_Array_Push(cs->state, &cs->preSemanticIrSlots, &target);
    return compiler_semantic_ir_end_receiver_call(
            cs, reference->loanId, sourceRange);
}

TZrBool compiler_semantic_ir_record_property_ref_store(
        SZrCompilerState *cs,
        TZrUInt32 referenceSlot,
        TZrUInt32 sourceSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *reference;
    SZrCompilerSemanticIrSlot *source;
    SZrSemanticIrInstructionSpec spec;
    TZrTypeId pointeeTypeId;
    TZrPlaceId placeId;
    TZrValueId dereferenceValueId;

    reference = compiler_semantic_ir_find_slot(cs, referenceSlot);
    source = compiler_semantic_ir_materialize_slot(
            cs, sourceSlot, sourceRange);
    if (reference == ZR_NULL || source == ZR_NULL ||
        source->valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    pointeeTypeId = compiler_semantic_ir_reference_pointee_type_id(
            cs, reference->typeId);
    placeId = compiler_semantic_ir_project_dereference(
            cs,
            reference,
            pointeeTypeId,
            sourceRange,
            &dereferenceValueId);
    if (placeId == ZR_PLACE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_STORE;
    spec.typeId = pointeeTypeId;
    spec.placeId = placeId;
    spec.valueId = source->valueId;
    spec.auxiliaryValueId = dereferenceValueId;
    spec.loanId = reference->loanId;
    spec.regionId = reference->regionId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    return compiler_semantic_ir_end_receiver_call(
            cs, reference->loanId, sourceRange);
}

TZrBool compiler_semantic_ir_end_receiver_call(
        SZrCompilerState *cs,
        TZrLoanId loanId,
        SZrFileRange sourceRange) {
    const SZrSemanticIrLoanFact *loan;
    SZrSemanticIrInstructionSpec spec;

    if (cs == ZR_NULL || loanId == ZR_SEMANTIC_LOAN_ID_INVALID) {
        return ZR_TRUE;
    }
    loan = ZrParser_SemanticIr_Loan(&cs->preSemanticIr, loanId);
    if (loan == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_END_LOAN;
    spec.placeId = loan->sourcePlaceId;
    spec.loanId = loanId;
    spec.regionId = loan->regionId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

void compiler_semantic_ir_init(SZrCompilerState *cs) {
    SZrFileRange emptyRange;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return;
    }
    memset(&emptyRange, 0, sizeof(emptyRange));
    ZrParser_SemanticIrFunction_Init(
            cs->state,
            &cs->preSemanticIr,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID);
    ZrCore_Array_Init(
            cs->state,
            &cs->preSemanticIrSlots,
            sizeof(SZrCompilerSemanticIrSlot),
            ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(
            cs->state,
            &cs->preSemanticIrReceiverLoanIds,
            sizeof(TZrLoanId),
            ZR_PARSER_INITIAL_CAPACITY_SMALL);
    cs->preSemanticIrInitialized = ZR_TRUE;
    cs->preSemanticIrValidated = ZR_FALSE;
    (void)ZrParser_SemanticIr_AddRegion(
            &cs->preSemanticIr,
            ZR_SEMANTIC_REGION_ID_INVALID,
            ZR_SEMANTIC_ESCAPE_FUNCTION,
            emptyRange);
}

void compiler_semantic_ir_reset(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return;
    }
    compiler_semantic_ir_free(cs);
    compiler_semantic_ir_init(cs);
}

void compiler_semantic_ir_free(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        !cs->preSemanticIrInitialized) {
        return;
    }
    ZrParser_SemanticIrFunction_Free(cs->state, &cs->preSemanticIr);
    ZrCore_Array_Free(cs->state, &cs->preSemanticIrSlots);
    ZrCore_Array_Free(cs->state, &cs->preSemanticIrReceiverLoanIds);
    cs->preSemanticIrInitialized = ZR_FALSE;
    cs->preSemanticIrValidated = ZR_FALSE;
}

const SZrSemanticIrFunction *ZrParser_Compiler_PreSemanticIr(
        const SZrCompilerState *cs) {
    if (cs == ZR_NULL || !cs->preSemanticIrInitialized) {
        return ZR_NULL;
    }
    return &cs->preSemanticIr;
}

TZrBool ZrParser_Compiler_PreSemanticIrIsValidated(
        const SZrCompilerState *cs) {
    return (TZrBool)(cs != ZR_NULL && cs->preSemanticIrInitialized &&
                     cs->preSemanticIrValidated);
}

TZrBool ZrParser_Compiler_ValidatePreSemanticIr(SZrCompilerState *cs) {
    SZrSemanticFlowResult flowResult;
    TZrUInt32 entryBlock;
    TZrUInt32 exitBlock;
    TZrBool analyzed;
    TZrBool hasTrackedLoanConflict = ZR_FALSE;

    if (cs == ZR_NULL || !cs->preSemanticIrInitialized) {
        return ZR_FALSE;
    }
    cs->preSemanticIrValidated = ZR_FALSE;
    if (!ZrParser_SemanticIr_Validate(&cs->preSemanticIr)) {
        return ZR_FALSE;
    }

    ZrParser_Cfg_Free(cs->state, &cs->preSemanticIr.cfg);
    ZrParser_Cfg_Init(cs->state, &cs->preSemanticIr.cfg);
    entryBlock = ZrParser_Cfg_AppendBlock(
            cs->state,
            &cs->preSemanticIr.cfg,
            ZR_PARSER_CFG_BLOCK_ENTRY,
            ZR_NULL);
    exitBlock = ZrParser_Cfg_AppendBlock(
            cs->state,
            &cs->preSemanticIr.cfg,
            ZR_PARSER_CFG_BLOCK_EXIT,
            ZR_NULL);
    if (entryBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        exitBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    cs->preSemanticIr.cfg.entryBlockId = entryBlock;
    cs->preSemanticIr.cfg.exitBlockId = exitBlock;
    if (!ZrParser_Cfg_Connect(
                &cs->preSemanticIr.cfg,
                entryBlock,
                exitBlock,
                ZR_PARSER_CFG_EDGE_RETURN,
                ZR_NULL) ||
        !ZrParser_SemanticIr_BindBlockRange(
                &cs->preSemanticIr,
                &cs->preSemanticIr.cfg,
                entryBlock,
                0U,
                (TZrUInt32)cs->preSemanticIr.instructions.length,
                ZR_PARSER_CFG_TERMINATOR_RETURN) ||
        !ZrParser_SemanticIr_BindBlockRange(
                &cs->preSemanticIr,
                &cs->preSemanticIr.cfg,
                exitBlock,
                (TZrUInt32)cs->preSemanticIr.instructions.length,
                0U,
                ZR_PARSER_CFG_TERMINATOR_EXIT)) {
        return ZR_FALSE;
    }

    ZrParser_SemanticFlowResult_Init(cs->state, &flowResult);
    analyzed = ZrParser_SemanticFlow_Analyze(
            cs->state,
            &cs->preSemanticIr,
            &cs->preSemanticIr.cfg,
            &flowResult);
    if (analyzed) {
        for (TZrSize index = 0U;
             index < flowResult.diagnostics.length;
             index++) {
            const SZrSemanticFlowDiagnostic *diagnostic =
                    (const SZrSemanticFlowDiagnostic *)ZrCore_Array_Get(
                            &flowResult.diagnostics, index);
            const SZrSemanticIrInstruction *instruction;
            TZrBool receiverLoanConflict;
            TZrBool contiguousSourceLoanConflict;

            if (diagnostic == ZR_NULL ||
                diagnostic->kind != ZR_SEMANTIC_FLOW_LOAN_CONFLICT) {
                continue;
            }
            instruction = diagnostic->instructionId ==
                                  ZR_SEMANTIC_INSTRUCTION_ID_INVALID
                                  ? ZR_NULL
                                  : ZrParser_SemanticIr_InstructionAt(
                                            &cs->preSemanticIr,
                                            (TZrSize)diagnostic->instructionId - 1U);
            receiverLoanConflict =
                    (TZrBool)(compiler_semantic_ir_is_receiver_loan(
                                      cs, diagnostic->relatedLoanId) ||
                              (instruction != ZR_NULL &&
                               compiler_semantic_ir_is_receiver_loan(
                                       cs, instruction->loanId)));
            contiguousSourceLoanConflict =
                    (TZrBool)(compiler_semantic_ir_is_contiguous_view_source_loan(
                                      cs, diagnostic->relatedLoanId) ||
                              (instruction != ZR_NULL &&
                               compiler_semantic_ir_is_contiguous_view_source_loan(
                                       cs, instruction->loanId)));
            if (!receiverLoanConflict && !contiguousSourceLoanConflict) {
                continue;
            }
            hasTrackedLoanConflict = ZR_TRUE;
            ZrParser_Compiler_Error(
                    cs,
                    contiguousSourceLoanConflict
                            ? "Active contiguous view prevents source move or drop"
                            : "Receiver borrow conflicts during argument evaluation",
                    diagnostic->sourceRange);
            break;
        }
    }
    cs->preSemanticIrValidated =
            (TZrBool)(analyzed && !hasTrackedLoanConflict);
    ZrParser_SemanticFlowResult_Free(cs->state, &flowResult);
    return cs->preSemanticIrValidated;
}

TZrBool compiler_semantic_ir_register_local(SZrCompilerState *cs,
                                             SZrString *name,
                                             TZrUInt32 stackSlot,
                                             SZrFileRange sourceRange,
                                             TZrBool initialized) {
    const SZrTypeBinding *binding;
    SZrCompilerSemanticIrSlot slot;
    SZrParserPlaceBase base;
    SZrSemanticIrInstructionSpec spec;
    const SZrCompilerSemanticIrSlot *priorSlot;
    const SZrParserPlace *priorPlace = ZR_NULL;
    TZrValueId priorTemporaryValueId = ZR_VALUE_ID_INVALID;
    SZrSemanticContiguousViewFact priorViewFact;
    const SZrSemanticContiguousViewFact *priorViewFactRef;
    TZrBool hasPriorViewFact;

    if (cs == ZR_NULL || name == ZR_NULL || cs->typeEnv == ZR_NULL ||
        stackSlot == ZR_PARSER_SLOT_NONE || !cs->preSemanticIrInitialized) {
        return ZR_FALSE;
    }
    binding = ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv, name);
    if (binding == ZR_NULL || binding->typeId == ZR_SEMANTIC_ID_INVALID ||
        binding->symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    priorSlot = compiler_semantic_ir_find_slot(cs, stackSlot);
    if (priorSlot != ZR_NULL &&
        priorSlot->placeId != ZR_PLACE_ID_INVALID &&
        priorSlot->valueId != ZR_VALUE_ID_INVALID) {
        priorPlace = ZrParser_PlaceGraph_Get(
                &cs->preSemanticIr.places, priorSlot->placeId);
        if (priorPlace != ZR_NULL &&
            priorPlace->base.kind == ZR_PARSER_PLACE_BASE_TEMPORARY) {
            priorTemporaryValueId = priorSlot->valueId;
        }
    }
    priorViewFactRef = compiler_semantic_ir_contiguous_view_fact_for_slot(
            cs, stackSlot, sourceRange);
    hasPriorViewFact = (TZrBool)(priorViewFactRef != ZR_NULL);
    if (hasPriorViewFact) {
        priorViewFact = *priorViewFactRef;
    }

    memset(&slot, 0, sizeof(slot));
    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    base.identity = stackSlot;
    slot.stackSlot = stackSlot;
    slot.typeId = binding->typeId;
    slot.symbolId = binding->symbolId;
    slot.placeId = ZrParser_SemanticIr_AddLocal(
            &cs->preSemanticIr,
            slot.symbolId,
            &base,
            slot.typeId,
            sourceRange,
            ZR_FALSE);
    if (slot.placeId == ZR_PLACE_ID_INVALID) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(cs->state, &cs->preSemanticIrSlots, &slot);

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    spec.typeId = slot.typeId;
    spec.placeId = slot.placeId;
    spec.symbolId = slot.symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    if (!initialized) {
        return ZR_TRUE;
    }

    slot.valueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, slot.typeId, sourceRange);
    if (slot.valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    *(SZrCompilerSemanticIrSlot *)ZrCore_Array_Get(
            &cs->preSemanticIrSlots, cs->preSemanticIrSlots.length - 1U) = slot;
    if (priorTemporaryValueId != ZR_VALUE_ID_INVALID) {
        memset(&spec, 0, sizeof(spec));
        spec.opcode = ZR_SEMANTIC_IR_CONVERT;
        spec.typeId = slot.typeId;
        spec.valueId = priorTemporaryValueId;
        spec.resultValueId = slot.valueId;
        spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        spec.sourceRange = sourceRange;
        if (!compiler_semantic_ir_emit(cs, &spec)) {
            return ZR_FALSE;
        }
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_INITIALIZE;
    spec.typeId = slot.typeId;
    spec.placeId = slot.placeId;
    spec.valueId = slot.valueId;
    spec.symbolId = slot.symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    if (hasPriorViewFact) {
        priorViewFact.factId = ZR_SEMANTIC_CONTIGUOUS_VIEW_FACT_ID_INVALID;
        priorViewFact.viewPlaceId = slot.placeId;
        priorViewFact.viewValueId = slot.valueId;
        priorViewFact.sourceRange = sourceRange;
        if (ZrParser_SemanticIr_AddContiguousViewFact(
                    &cs->preSemanticIr, &priorViewFact) ==
            ZR_SEMANTIC_CONTIGUOUS_VIEW_FACT_ID_INVALID) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool compiler_semantic_ir_emit_load(SZrCompilerState *cs,
                                              TZrUInt32 stackSlot,
                                              SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot =
            compiler_semantic_ir_materialize_slot(cs, stackSlot, sourceRange);
    SZrSemanticIrInstructionSpec spec;
    TZrValueId resultValueId;

    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }
    resultValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, slot->typeId, sourceRange);
    if (resultValueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_LOAD;
    spec.typeId = slot->typeId;
    spec.placeId = slot->placeId;
    spec.resultValueId = resultValueId;
    spec.symbolId = slot->symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

static TZrBool compiler_semantic_ir_emit_store(SZrCompilerState *cs,
                                               TZrUInt32 stackSlot,
                                               SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot =
            compiler_semantic_ir_materialize_slot(cs, stackSlot, sourceRange);
    SZrSemanticIrInstructionSpec spec;
    TZrValueId valueId;

    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }
    valueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, slot->typeId, sourceRange);
    if (valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    slot->valueId = valueId;
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_STORE;
    spec.typeId = slot->typeId;
    spec.placeId = slot->placeId;
    spec.valueId = valueId;
    spec.symbolId = slot->symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    return compiler_semantic_ir_emit(cs, &spec);
}

static TZrBool compiler_semantic_ir_emit_ownership(
        SZrCompilerState *cs,
        EZrOwnershipBuiltinKind builtinKind,
        TZrUInt32 sourceSlot,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *slot =
            compiler_semantic_ir_materialize_slot(cs, sourceSlot, sourceRange);
    SZrSemanticIrInstructionSpec spec;
    TZrValueId resultValueId = ZR_VALUE_ID_INVALID;

    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.typeId = slot->typeId;
    spec.placeId = slot->placeId;
    spec.valueId = slot->valueId;
    spec.symbolId = slot->symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    switch (builtinKind) {
        case ZR_OWNERSHIP_BUILTIN_KIND_RELEASE:
            spec.opcode = ZR_SEMANTIC_IR_DROP;
            slot->valueId = ZR_VALUE_ID_INVALID;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_DETACH:
            spec.opcode = ZR_SEMANTIC_IR_OWN_CONSTRUCT;
            spec.ownershipOperation = ZR_SEMANTIC_OWNERSHIP_RETURN_TO_GC;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            slot->valueId = ZR_VALUE_ID_INVALID;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_BORROW:
            spec.opcode = ZR_SEMANTIC_IR_BORROW_SHARED;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            spec.regionId = 1U;
            spec.loanId = ZrParser_SemanticIr_AddLoan(
                    &cs->preSemanticIr,
                    slot->placeId,
                    ZR_SEMANTIC_LOAN_SHARED,
                    spec.regionId,
                    sourceRange,
                    sourceRange,
                    resultValueId);
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_LOAN:
            spec.opcode = ZR_SEMANTIC_IR_BORROW_MUT;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            spec.regionId = 1U;
            spec.loanId = ZrParser_SemanticIr_AddLoan(
                    &cs->preSemanticIr,
                    slot->placeId,
                    ZR_SEMANTIC_LOAN_MUTABLE,
                    spec.regionId,
                    sourceRange,
                    sourceRange,
                    resultValueId);
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE:
            if (compiler_semantic_ir_slot_is_unique_owner(cs, slot)) {
                spec.opcode = ZR_SEMANTIC_IR_MOVE;
                slot->valueId = ZR_VALUE_ID_INVALID;
            } else {
                spec.opcode = ZR_SEMANTIC_IR_OWN_CONSTRUCT;
                spec.ownershipOperation = ZR_SEMANTIC_OWNERSHIP_UNIQUE;
            }
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_SHARED:
            spec.opcode = ZR_SEMANTIC_IR_OWN_CONSTRUCT;
            spec.ownershipOperation = ZR_SEMANTIC_OWNERSHIP_SHARE;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_WEAK:
            spec.opcode = ZR_SEMANTIC_IR_OWN_CONSTRUCT;
            spec.ownershipOperation = ZR_SEMANTIC_OWNERSHIP_WEAK;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_UPGRADE:
            spec.opcode = ZR_SEMANTIC_IR_OWN_CONSTRUCT;
            spec.ownershipOperation = ZR_SEMANTIC_OWNERSHIP_UPGRADE;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_INTO_GC:
            spec.opcode = ZR_SEMANTIC_IR_OWN_CONSTRUCT;
            spec.ownershipOperation = ZR_SEMANTIC_OWNERSHIP_INTO_GC_BOX;
            resultValueId = ZrParser_SemanticIr_AddValue(
                    &cs->preSemanticIr, slot->typeId, sourceRange);
            spec.resultValueId = resultValueId;
            slot->valueId = ZR_VALUE_ID_INVALID;
            break;
        case ZR_OWNERSHIP_BUILTIN_KIND_NONE:
        case ZR_OWNERSHIP_BUILTIN_KIND_RETURN_LOAN:
        default:
            return ZR_FALSE;
    }
    if (((spec.opcode == ZR_SEMANTIC_IR_MOVE ||
          spec.opcode == ZR_SEMANTIC_IR_BORROW_SHARED ||
          spec.opcode == ZR_SEMANTIC_IR_BORROW_MUT ||
          spec.opcode == ZR_SEMANTIC_IR_OWN_CONSTRUCT) &&
         resultValueId == ZR_VALUE_ID_INVALID) ||
        ((spec.opcode == ZR_SEMANTIC_IR_BORROW_SHARED ||
          spec.opcode == ZR_SEMANTIC_IR_BORROW_MUT) &&
         spec.loanId == ZR_SEMANTIC_LOAN_ID_INVALID)) {
        return ZR_FALSE;
    }
    return compiler_semantic_ir_emit(cs, &spec);
}

TZrBool compiler_semantic_ir_lower_load(SZrCompilerState *cs,
                                        TZrUInt32 stackSlot,
                                        TZrUInt32 resultSlot,
                                        SZrFileRange sourceRange) {
    const SZrSemanticIrInstruction *instruction;
    EZrInstructionCode opcode;

    if (!compiler_semantic_ir_emit_load(cs, stackSlot, sourceRange)) {
        return ZR_FALSE;
    }
    instruction = compiler_semantic_ir_last_instruction(cs);
    opcode = compiler_semantic_ir_exec_opcode(instruction);
    if (opcode != ZR_INSTRUCTION_ENUM(GET_STACK)) {
        return ZR_FALSE;
    }
    if (!compiler_semantic_ir_propagate_contiguous_view(
                cs,
                resultSlot,
                stackSlot,
                sourceRange,
                ZR_TRUE)) {
        return ZR_FALSE;
    }
    emit_instruction(
            cs,
            create_instruction_1(
                    opcode,
                    (TZrUInt16)resultSlot,
                    (TZrInt32)stackSlot));
    return ZR_TRUE;
}

TZrBool compiler_semantic_ir_lower_store(SZrCompilerState *cs,
                                         TZrUInt32 stackSlot,
                                         TZrUInt32 valueSlot,
                                         SZrFileRange sourceRange) {
    const SZrSemanticIrInstruction *instruction;
    EZrInstructionCode opcode;

    if (!compiler_semantic_ir_emit_store(cs, stackSlot, sourceRange)) {
        return ZR_FALSE;
    }
    if (!compiler_semantic_ir_propagate_contiguous_view(
                cs,
                stackSlot,
                valueSlot,
                sourceRange,
                ZR_FALSE)) {
        return ZR_FALSE;
    }
    instruction = compiler_semantic_ir_last_instruction(cs);
    opcode = compiler_semantic_ir_exec_opcode(instruction);
    if (opcode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
        return ZR_FALSE;
    }
    emit_instruction(
            cs,
            create_instruction_1(
                    opcode,
                    (TZrUInt16)stackSlot,
                    (TZrInt32)valueSlot));
    return ZR_TRUE;
}

TZrBool compiler_semantic_ir_lower_ownership(
        SZrCompilerState *cs,
        EZrOwnershipBuiltinKind builtinKind,
        TZrUInt32 sourceSlot,
        TZrUInt32 resultSlot,
        SZrFileRange sourceRange) {
    const SZrSemanticIrInstruction *instruction;
    EZrInstructionCode opcode;

    if (!compiler_semantic_ir_emit_ownership(
                cs, builtinKind, sourceSlot, sourceRange)) {
        return ZR_FALSE;
    }
    instruction = compiler_semantic_ir_last_instruction(cs);
    opcode = builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE
                     ? ZR_INSTRUCTION_ENUM(OWN_UNIQUE)
                     : compiler_semantic_ir_exec_opcode(instruction);
    if (opcode == ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
        return ZR_FALSE;
    }
    emit_instruction(
            cs,
            create_instruction_2(
                    opcode,
                    (TZrUInt16)resultSlot,
                    (TZrUInt16)sourceSlot,
                    0));
    return ZR_TRUE;
}

TZrBool compiler_semantic_ir_lower_value_construct(
        SZrCompilerState *cs,
        TZrUInt32 destinationSlot,
        TZrTypeId typeId,
        TZrSymbolId constructorId,
        const TZrUInt32 *argumentSlots,
        TZrSize argumentCount,
        SZrFileRange sourceRange) {
    return compiler_semantic_ir_lower_value_construct_to_place(
            cs,
            destinationSlot,
            ZR_PLACE_ID_INVALID,
            typeId,
            constructorId,
            argumentSlots,
            argumentCount,
            sourceRange);
}

TZrBool compiler_semantic_ir_lower_value_construct_to_place(
        SZrCompilerState *cs,
        TZrUInt32 destinationSlot,
        TZrPlaceId destinationPlaceId,
        TZrTypeId typeId,
        TZrSymbolId constructorId,
        const TZrUInt32 *argumentSlots,
        TZrSize argumentCount,
        SZrFileRange sourceRange) {
    SZrCompilerSemanticIrSlot *destination;
    const SZrParserPlace *destinationPlace;
    SZrSemanticIrInstructionSpec spec;
    TZrValueId *operands = ZR_NULL;
    TZrValueId resultValueId;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID ||
        constructorId == ZR_SEMANTIC_ID_INVALID ||
        (argumentCount > 0U && argumentSlots == ZR_NULL)) {
        return ZR_FALSE;
    }
    destination = compiler_semantic_ir_materialize_slot(
            cs, destinationSlot, sourceRange);
    if (destination == ZR_NULL) {
        return ZR_FALSE;
    }
    if (destinationPlaceId == ZR_PLACE_ID_INVALID) {
        destinationPlaceId = destination->placeId;
    }
    destinationPlace = ZrParser_PlaceGraph_Get(
            &cs->preSemanticIr.places, destinationPlaceId);
    if (destinationPlace == ZR_NULL) {
        return ZR_FALSE;
    }
    if (argumentCount > 0U) {
        operands = (TZrValueId *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(TZrValueId) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (operands == ZR_NULL) {
            return ZR_FALSE;
        }
        for (TZrSize index = 0U; index < argumentCount; index++) {
            SZrCompilerSemanticIrSlot *argument =
                    compiler_semantic_ir_materialize_slot(
                            cs, argumentSlots[index], sourceRange);
            if (argument == ZR_NULL || argument->valueId == ZR_VALUE_ID_INVALID) {
                goto cleanup;
            }
            operands[index] = argument->valueId;
        }
    }
    resultValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, typeId, sourceRange);
    if (resultValueId == ZR_VALUE_ID_INVALID) {
        goto cleanup;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_VALUE_CONSTRUCT;
    spec.typeId = typeId;
    spec.placeId = destinationPlaceId;
    spec.resultValueId = resultValueId;
    spec.symbolId = destination->symbolId;
    spec.constructorId = constructorId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.operands = operands;
    spec.operandCount = argumentCount;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        goto cleanup;
    }
    destination->typeId = typeId;
    destination->valueId = resultValueId;
    success = ZR_TRUE;

cleanup:
    if (operands != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                operands,
                sizeof(TZrValueId) * argumentCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}
