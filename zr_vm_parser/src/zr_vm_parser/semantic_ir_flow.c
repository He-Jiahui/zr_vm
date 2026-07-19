#include "zr_vm_parser/semantic_ir.h"
#include "semantic_ir_flow_internal.h"

#include <string.h>

static void semantic_borrow_state_init(SZrState *state,
                                       SZrSemanticBorrowState *borrowing) {
    memset(borrowing, 0, sizeof(*borrowing));
    ZrCore_Array_Init(
            state, &borrowing->sharedLoanIds, sizeof(TZrLoanId), 2U);
}

static void semantic_borrow_state_free(SZrState *state,
                                       SZrSemanticBorrowState *borrowing) {
    if (borrowing == ZR_NULL) {
        return;
    }
    ZrCore_Array_Free(state, &borrowing->sharedLoanIds);
    borrowing->mutableLoanId = ZR_SEMANTIC_LOAN_ID_INVALID;
}

static TZrBool semantic_borrow_contains(const SZrSemanticBorrowState *borrowing,
                                        TZrLoanId loanId) {
    TZrSize index;

    if (borrowing == ZR_NULL || !borrowing->sharedLoanIds.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < borrowing->sharedLoanIds.length; index++) {
        const TZrLoanId *candidate = (const TZrLoanId *)ZrCore_Array_Get(
                (SZrArray *)&borrowing->sharedLoanIds, index);
        if (candidate != ZR_NULL && *candidate == loanId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool semantic_borrow_add(SZrState *state,
                                   SZrSemanticBorrowState *borrowing,
                                   TZrLoanId loanId) {
    if (state == ZR_NULL || borrowing == ZR_NULL ||
        loanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
        semantic_borrow_contains(borrowing, loanId)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(state, &borrowing->sharedLoanIds, &loanId);
    return ZR_TRUE;
}

static TZrBool semantic_borrow_remove(SZrSemanticBorrowState *borrowing,
                                      TZrLoanId loanId) {
    TZrSize index;

    if (borrowing == ZR_NULL || !borrowing->sharedLoanIds.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < borrowing->sharedLoanIds.length; index++) {
        TZrLoanId *candidate = (TZrLoanId *)ZrCore_Array_Get(
                &borrowing->sharedLoanIds, index);
        if (candidate != ZR_NULL && *candidate == loanId) {
            TZrSize tailCount = borrowing->sharedLoanIds.length - index - 1U;
            if (tailCount > 0U) {
                memmove(candidate,
                        candidate + 1,
                        tailCount * sizeof(TZrLoanId));
            }
            borrowing->sharedLoanIds.length--;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void semantic_place_state_init(SZrState *state,
                                      SZrSemanticPlaceFlowState *placeState,
                                      const SZrParserPlace *place) {
    memset(placeState, 0, sizeof(*placeState));
    placeState->initialization =
            place != ZR_NULL &&
                    (place->base.kind == ZR_PARSER_PLACE_BASE_PARAMETER ||
                     place->base.kind == ZR_PARSER_PLACE_BASE_THIS ||
                     place->base.kind == ZR_PARSER_PLACE_BASE_STATIC ||
                     place->base.kind == ZR_PARSER_PLACE_BASE_EXTERNAL_HANDLE)
                    ? ZR_SEMANTIC_INITIALIZATION_INITIALIZED
                    : ZR_SEMANTIC_INITIALIZATION_UNINITIALIZED;
    placeState->availability = ZR_SEMANTIC_AVAILABILITY_AVAILABLE;
    placeState->escape = ZR_SEMANTIC_ESCAPE_LOCAL;
    semantic_borrow_state_init(state, &placeState->borrowing);
}

static void semantic_place_state_free(SZrState *state,
                                      SZrSemanticPlaceFlowState *placeState) {
    if (placeState != ZR_NULL) {
        semantic_borrow_state_free(state, &placeState->borrowing);
    }
}

static void semantic_state_array_free(SZrState *state, SZrArray *states) {
    TZrSize index;

    if (state == ZR_NULL || states == ZR_NULL || !states->isValid) {
        return;
    }
    for (index = 0; index < states->length; index++) {
        semantic_place_state_free(
                state,
                (SZrSemanticPlaceFlowState *)ZrCore_Array_Get(states, index));
    }
    ZrCore_Array_Free(state, states);
}

static TZrBool semantic_state_array_init(
        SZrState *state,
        SZrArray *states,
        const SZrSemanticIrFunction *function) {
    TZrSize index;

    if (state == ZR_NULL || states == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(
            state,
            states,
            sizeof(SZrSemanticPlaceFlowState),
            function->places.places.length > 0U
                    ? function->places.places.length
                    : 1U);
    for (index = 0; index < function->places.places.length; index++) {
        const SZrParserPlace *place = (const SZrParserPlace *)ZrCore_Array_Get(
                (SZrArray *)&function->places.places, index);
        SZrSemanticPlaceFlowState placeState;

        semantic_place_state_init(state, &placeState, place);
        ZrCore_Array_Push(state, states, &placeState);
    }
    return ZR_TRUE;
}

static TZrBool semantic_place_state_copy(
        SZrState *state,
        SZrSemanticPlaceFlowState *destination,
        const SZrSemanticPlaceFlowState *source) {
    TZrSize index;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL) {
        return ZR_FALSE;
    }
    destination->initialization = source->initialization;
    destination->availability = source->availability;
    destination->escape = source->escape;
    destination->borrowing.mutableLoanId = source->borrowing.mutableLoanId;
    destination->borrowing.sharedLoanIds.length = 0U;
    for (index = 0; index < source->borrowing.sharedLoanIds.length; index++) {
        const TZrLoanId *loanId = (const TZrLoanId *)ZrCore_Array_Get(
                (SZrArray *)&source->borrowing.sharedLoanIds, index);
        if (loanId != ZR_NULL) {
            semantic_borrow_add(state, &destination->borrowing, *loanId);
        }
    }
    return ZR_TRUE;
}

static TZrBool semantic_state_array_copy(SZrState *state,
                                         SZrArray *destination,
                                         const SZrArray *source) {
    TZrSize index;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL ||
        destination->length != source->length) {
        return ZR_FALSE;
    }
    for (index = 0; index < source->length; index++) {
        if (!semantic_place_state_copy(
                    state,
                    (SZrSemanticPlaceFlowState *)ZrCore_Array_Get(
                            destination, index),
                    (const SZrSemanticPlaceFlowState *)ZrCore_Array_Get(
                            (SZrArray *)source, index))) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static EZrSemanticInitializationState semantic_join_initialization(
        EZrSemanticInitializationState left,
        EZrSemanticInitializationState right) {
    return left == right ? left : ZR_SEMANTIC_INITIALIZATION_MAYBE_INITIALIZED;
}

static EZrSemanticAvailabilityState semantic_join_availability(
        EZrSemanticAvailabilityState left,
        EZrSemanticAvailabilityState right) {
    return left == right ? left : ZR_SEMANTIC_AVAILABILITY_MAYBE_MOVED;
}

static TZrLoanId semantic_join_mutable_loan(TZrLoanId left,
                                            TZrLoanId right) {
    if (left == right) {
        return left;
    }
    if (left == ZR_SEMANTIC_LOAN_ID_INVALID) {
        return right;
    }
    if (right == ZR_SEMANTIC_LOAN_ID_INVALID) {
        return left;
    }
    return ZR_SEMANTIC_LOAN_ID_MULTIPLE;
}

static TZrBool semantic_state_array_join(SZrState *state,
                                         SZrArray *destination,
                                         const SZrArray *source) {
    TZrBool changed = ZR_FALSE;
    TZrSize index;

    if (state == ZR_NULL || destination == ZR_NULL || source == ZR_NULL ||
        destination->length != source->length) {
        return ZR_FALSE;
    }
    for (index = 0; index < source->length; index++) {
        SZrSemanticPlaceFlowState *dst =
                (SZrSemanticPlaceFlowState *)ZrCore_Array_Get(destination, index);
        const SZrSemanticPlaceFlowState *src =
                (const SZrSemanticPlaceFlowState *)ZrCore_Array_Get(
                        (SZrArray *)source, index);
        EZrSemanticInitializationState initialization =
                semantic_join_initialization(dst->initialization, src->initialization);
        EZrSemanticAvailabilityState availability =
                semantic_join_availability(dst->availability, src->availability);
        TZrLoanId mutableLoan = semantic_join_mutable_loan(
                dst->borrowing.mutableLoanId,
                src->borrowing.mutableLoanId);
        TZrSize loanIndex;

        if (initialization != dst->initialization) {
            dst->initialization = initialization;
            changed = ZR_TRUE;
        }
        if (availability != dst->availability) {
            dst->availability = availability;
            changed = ZR_TRUE;
        }
        if (src->escape > dst->escape) {
            dst->escape = src->escape;
            changed = ZR_TRUE;
        }
        if (mutableLoan != dst->borrowing.mutableLoanId) {
            dst->borrowing.mutableLoanId = mutableLoan;
            changed = ZR_TRUE;
        }
        for (loanIndex = 0;
             loanIndex < src->borrowing.sharedLoanIds.length;
             loanIndex++) {
            const TZrLoanId *loanId = (const TZrLoanId *)ZrCore_Array_Get(
                    (SZrArray *)&src->borrowing.sharedLoanIds, loanIndex);
            if (loanId != ZR_NULL &&
                semantic_borrow_add(state, &dst->borrowing, *loanId)) {
                changed = ZR_TRUE;
            }
        }
    }
    return changed;
}

static void semantic_block_facts_free(SZrState *state,
                                      SZrSemanticBlockFlowFacts *facts) {
    if (facts == ZR_NULL) {
        return;
    }
    semantic_state_array_free(state, &facts->entryStates);
    semantic_state_array_free(state, &facts->exitStates);
    memset(facts, 0, sizeof(*facts));
}

static void semantic_flow_result_reset(SZrState *state,
                                       SZrSemanticFlowResult *result) {
    TZrSize index;

    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    if (result->blockFacts.isValid) {
        for (index = 0; index < result->blockFacts.length; index++) {
            semantic_block_facts_free(
                    state,
                    (SZrSemanticBlockFlowFacts *)ZrCore_Array_Get(
                            &result->blockFacts, index));
        }
        result->blockFacts.length = 0U;
    }
    if (result->diagnostics.isValid) {
        result->diagnostics.length = 0U;
    }
    if (result->loanLiveness.isValid) {
        for (index = 0U; index < result->loanLiveness.length; index++) {
            SZrSemanticInstructionLoanLiveness *fact =
                    (SZrSemanticInstructionLoanLiveness *)ZrCore_Array_Get(
                            &result->loanLiveness, index);
            if (fact != ZR_NULL) {
                ZrCore_Array_Free(state, &fact->liveInLoanIds);
                ZrCore_Array_Free(state, &fact->liveOutLoanIds);
                ZrCore_Array_Free(state, &fact->activeInLoanIds);
                ZrCore_Array_Free(state, &fact->activeOutLoanIds);
            }
        }
        result->loanLiveness.length = 0U;
    }
    if (result->loanRegions.isValid) {
        result->loanRegions.length = 0U;
    }
    result->placeCount = 0U;
    result->loanCount = 0U;
}

void ZrParser_SemanticFlowResult_Init(SZrState *state,
                                      SZrSemanticFlowResult *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->state = state;
    ZrCore_Array_Init(
            state,
            &result->blockFacts,
            sizeof(SZrSemanticBlockFlowFacts),
            8U);
    ZrCore_Array_Init(
            state,
            &result->diagnostics,
            sizeof(SZrSemanticFlowDiagnostic),
            8U);
    ZrCore_Array_Init(
            state,
            &result->loanLiveness,
            sizeof(SZrSemanticInstructionLoanLiveness),
            8U);
    ZrCore_Array_Init(
            state,
            &result->loanRegions,
            sizeof(SZrSemanticLoanRegionFact),
            8U);
}

void ZrParser_SemanticFlowResult_Free(SZrState *state,
                                      SZrSemanticFlowResult *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }
    semantic_flow_result_reset(state, result);
    ZrCore_Array_Free(state, &result->blockFacts);
    ZrCore_Array_Free(state, &result->diagnostics);
    ZrCore_Array_Free(state, &result->loanLiveness);
    ZrCore_Array_Free(state, &result->loanRegions);
    memset(result, 0, sizeof(*result));
}

static SZrSemanticBlockFlowFacts *semantic_flow_block_facts_mutable(
        SZrSemanticFlowResult *result,
        TZrUInt32 blockId) {
    if (result == ZR_NULL || !result->blockFacts.isValid ||
        blockId >= result->blockFacts.length) {
        return ZR_NULL;
    }
    return (SZrSemanticBlockFlowFacts *)ZrCore_Array_Get(
            &result->blockFacts, blockId);
}

const SZrSemanticBlockFlowFacts *ZrParser_SemanticFlow_BlockFacts(
        const SZrSemanticFlowResult *result,
        TZrUInt32 blockId) {
    return semantic_flow_block_facts_mutable(
            (SZrSemanticFlowResult *)result, blockId);
}

const SZrSemanticPlaceFlowState *ZrParser_SemanticFlow_PlaceState(
        const SZrSemanticBlockFlowFacts *facts,
        TZrPlaceId placeId,
        TZrBool entryState) {
    const SZrArray *states;

    if (facts == ZR_NULL || placeId == ZR_PLACE_ID_INVALID) {
        return ZR_NULL;
    }
    states = entryState ? &facts->entryStates : &facts->exitStates;
    if (!states->isValid || placeId > states->length) {
        return ZR_NULL;
    }
    return (const SZrSemanticPlaceFlowState *)ZrCore_Array_Get(
            (SZrArray *)states, (TZrSize)placeId - 1U);
}

static TZrBool semantic_flow_diagnostic_exists(
        const SZrSemanticFlowResult *result,
        EZrSemanticFlowDiagnosticKind kind,
        TZrUInt32 blockId,
        TZrSemanticInstructionId instructionId,
        TZrPlaceId placeId) {
    TZrSize index;

    for (index = 0; index < result->diagnostics.length; index++) {
        const SZrSemanticFlowDiagnostic *diagnostic =
                (const SZrSemanticFlowDiagnostic *)ZrCore_Array_Get(
                        (SZrArray *)&result->diagnostics, index);
        if (diagnostic != ZR_NULL && diagnostic->kind == kind &&
            diagnostic->blockId == blockId &&
            diagnostic->instructionId == instructionId &&
            diagnostic->placeId == placeId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void semantic_flow_add_diagnostic(
        SZrSemanticFlowResult *result,
        EZrSemanticFlowDiagnosticKind kind,
        TZrUInt32 blockId,
        const SZrSemanticIrInstruction *instruction,
        TZrLoanId relatedLoanId) {
    SZrSemanticFlowDiagnostic diagnostic;

    if (result == ZR_NULL || instruction == ZR_NULL ||
        semantic_flow_diagnostic_exists(
                result,
                kind,
                blockId,
                instruction->id,
                instruction->placeId)) {
        return;
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.kind = kind;
    diagnostic.blockId = blockId;
    diagnostic.instructionId = instruction->id;
    diagnostic.placeId = instruction->placeId;
    diagnostic.relatedLoanId = relatedLoanId;
    diagnostic.sourceRange = instruction->sourceRange;
    ZrCore_Array_Push(result->state, &result->diagnostics, &diagnostic);
}

TZrBool ZrParser_SemanticFlow_HasDiagnostic(
        const SZrSemanticFlowResult *result,
        EZrSemanticFlowDiagnosticKind kind,
        TZrPlaceId placeId) {
    TZrSize index;

    if (result == ZR_NULL || !result->diagnostics.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < result->diagnostics.length; index++) {
        const SZrSemanticFlowDiagnostic *diagnostic =
                (const SZrSemanticFlowDiagnostic *)ZrCore_Array_Get(
                        (SZrArray *)&result->diagnostics, index);
        if (diagnostic != ZR_NULL && diagnostic->kind == kind &&
            diagnostic->placeId == placeId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void semantic_flow_check_read(
        SZrSemanticFlowResult *result,
        TZrUInt32 blockId,
        const SZrSemanticIrInstruction *instruction,
        const SZrSemanticPlaceFlowState *placeState,
        TZrBool recordDiagnostics) {
    if (!recordDiagnostics || result == ZR_NULL || instruction == ZR_NULL ||
        placeState == ZR_NULL) {
        return;
    }
    if (placeState->initialization == ZR_SEMANTIC_INITIALIZATION_UNINITIALIZED) {
        semantic_flow_add_diagnostic(
                result, ZR_SEMANTIC_FLOW_UNINITIALIZED, blockId, instruction, 0U);
    } else if (placeState->initialization ==
               ZR_SEMANTIC_INITIALIZATION_MAYBE_INITIALIZED) {
        semantic_flow_add_diagnostic(
                result, ZR_SEMANTIC_FLOW_MAYBE_UNINITIALIZED, blockId, instruction, 0U);
    }
    if (placeState->availability == ZR_SEMANTIC_AVAILABILITY_MOVED) {
        semantic_flow_add_diagnostic(
                result, ZR_SEMANTIC_FLOW_USE_AFTER_MOVE, blockId, instruction, 0U);
    } else if (placeState->availability ==
               ZR_SEMANTIC_AVAILABILITY_MAYBE_MOVED) {
        semantic_flow_add_diagnostic(
                result, ZR_SEMANTIC_FLOW_MAYBE_MOVED, blockId, instruction, 0U);
    } else if (placeState->availability == ZR_SEMANTIC_AVAILABILITY_DROPPED) {
        semantic_flow_add_diagnostic(
                result, ZR_SEMANTIC_FLOW_USE_AFTER_DROP, blockId, instruction, 0U);
    }
}

static void semantic_flow_check_exclusive_access(
        SZrSemanticFlowResult *result,
        TZrUInt32 blockId,
        const SZrSemanticIrInstruction *instruction,
        const SZrSemanticPlaceFlowState *placeState,
        TZrBool recordDiagnostics) {
    ZR_UNUSED_PARAMETER(result);
    ZR_UNUSED_PARAMETER(blockId);
    ZR_UNUSED_PARAMETER(instruction);
    ZR_UNUSED_PARAMETER(placeState);
    ZR_UNUSED_PARAMETER(recordDiagnostics);
}

static void semantic_flow_begin_loan(
        SZrState *state,
        SZrSemanticFlowResult *result,
        TZrUInt32 blockId,
        const SZrSemanticIrInstruction *instruction,
        SZrSemanticPlaceFlowState *placeState,
        EZrSemanticLoanAccess access,
        TZrBool recordDiagnostics) {
    semantic_flow_check_read(
            result, blockId, instruction, placeState, recordDiagnostics);
    if (access == ZR_SEMANTIC_LOAN_MUTABLE) {
        placeState->borrowing.mutableLoanId = semantic_join_mutable_loan(
                placeState->borrowing.mutableLoanId,
                instruction->loanId);
    } else {
        (void)semantic_borrow_add(
                state, &placeState->borrowing, instruction->loanId);
    }
    if (recordDiagnostics &&
        placeState->escape >= ZR_SEMANTIC_ESCAPE_CALLER) {
        semantic_flow_add_diagnostic(
                result,
                ZR_SEMANTIC_FLOW_ESCAPE_VIOLATION,
                blockId,
                instruction,
                instruction->loanId);
    }
}

static void semantic_flow_transfer_instruction(
        SZrState *state,
        const SZrSemanticIrFunction *function,
        SZrSemanticFlowResult *result,
        TZrUInt32 blockId,
        const SZrSemanticIrInstruction *instruction,
        SZrArray *placeStates,
        TZrBool recordDiagnostics) {
    SZrSemanticPlaceFlowState *placeState;

    if (state == ZR_NULL || instruction == ZR_NULL || placeStates == ZR_NULL ||
        instruction->placeId == ZR_PLACE_ID_INVALID ||
        instruction->placeId > placeStates->length) {
        return;
    }
    placeState = (SZrSemanticPlaceFlowState *)ZrCore_Array_Get(
            placeStates, (TZrSize)instruction->placeId - 1U);

    switch (instruction->opcode) {
        case ZR_SEMANTIC_IR_INITIALIZE:
            semantic_flow_check_exclusive_access(
                    result,
                    blockId,
                    instruction,
                    placeState,
                    recordDiagnostics);
            placeState->initialization = ZR_SEMANTIC_INITIALIZATION_INITIALIZED;
            placeState->availability = ZR_SEMANTIC_AVAILABILITY_AVAILABLE;
            break;
        case ZR_SEMANTIC_IR_LOAD:
        case ZR_SEMANTIC_IR_COPY:
            semantic_flow_check_read(
                    result,
                    blockId,
                    instruction,
                    placeState,
                    recordDiagnostics);
            break;
        case ZR_SEMANTIC_IR_STORE:
            semantic_flow_check_exclusive_access(
                    result,
                    blockId,
                    instruction,
                    placeState,
                    recordDiagnostics);
            placeState->initialization = ZR_SEMANTIC_INITIALIZATION_INITIALIZED;
            placeState->availability = ZR_SEMANTIC_AVAILABILITY_AVAILABLE;
            break;
        case ZR_SEMANTIC_IR_MOVE:
            semantic_flow_check_read(
                    result,
                    blockId,
                    instruction,
                    placeState,
                    recordDiagnostics);
            semantic_flow_check_exclusive_access(
                    result,
                    blockId,
                    instruction,
                    placeState,
                    recordDiagnostics);
            placeState->availability = ZR_SEMANTIC_AVAILABILITY_MOVED;
            break;
        case ZR_SEMANTIC_IR_DROP:
            semantic_flow_check_read(
                    result,
                    blockId,
                    instruction,
                    placeState,
                    recordDiagnostics);
            semantic_flow_check_exclusive_access(
                    result,
                    blockId,
                    instruction,
                    placeState,
                    recordDiagnostics);
            placeState->availability = ZR_SEMANTIC_AVAILABILITY_DROPPED;
            break;
        case ZR_SEMANTIC_IR_BORROW_SHARED:
            semantic_flow_begin_loan(
                    state,
                    result,
                    blockId,
                    instruction,
                    placeState,
                    ZR_SEMANTIC_LOAN_SHARED,
                    recordDiagnostics);
            break;
        case ZR_SEMANTIC_IR_BORROW_MUT:
            semantic_flow_begin_loan(
                    state,
                    result,
                    blockId,
                    instruction,
                    placeState,
                    ZR_SEMANTIC_LOAN_MUTABLE,
                    recordDiagnostics);
            break;
        case ZR_SEMANTIC_IR_REBORROW: {
            const SZrSemanticIrLoanFact *loan =
                    ZrParser_SemanticIr_Loan(function, instruction->loanId);
            semantic_flow_begin_loan(
                    state,
                    result,
                    blockId,
                    instruction,
                    placeState,
                    loan != ZR_NULL ? loan->access : ZR_SEMANTIC_LOAN_SHARED,
                    recordDiagnostics);
            break;
        }
        case ZR_SEMANTIC_IR_END_LOAN:
            semantic_borrow_remove(&placeState->borrowing, instruction->loanId);
            if (placeState->borrowing.mutableLoanId == instruction->loanId) {
                placeState->borrowing.mutableLoanId =
                        ZR_SEMANTIC_LOAN_ID_INVALID;
            }
            break;
        case ZR_SEMANTIC_IR_CALL_TYPED:
        case ZR_SEMANTIC_IR_CALL_VIRTUAL:
        case ZR_SEMANTIC_IR_CALL_DYNAMIC:
        case ZR_SEMANTIC_IR_CALL_META:
            if (instruction->escape > placeState->escape) {
                placeState->escape = instruction->escape;
            }
            break;
        case ZR_SEMANTIC_IR_RETURN:
            if (placeState->escape < ZR_SEMANTIC_ESCAPE_CALLER) {
                placeState->escape = ZR_SEMANTIC_ESCAPE_CALLER;
            }
            if (recordDiagnostics &&
                (placeState->borrowing.sharedLoanIds.length > 0U ||
                 placeState->borrowing.mutableLoanId !=
                         ZR_SEMANTIC_LOAN_ID_INVALID)) {
                semantic_flow_add_diagnostic(
                        result,
                        ZR_SEMANTIC_FLOW_ESCAPE_VIOLATION,
                        blockId,
                        instruction,
                        placeState->borrowing.mutableLoanId);
            }
            break;
        default:
            break;
    }
}

static TZrBool semantic_flow_transfer_block(
        SZrState *state,
        const SZrSemanticIrFunction *function,
        const SZrParserCfgBlock *block,
        SZrArray *placeStates,
        SZrSemanticFlowResult *result,
        TZrBool recordDiagnostics) {
    TZrUInt32 offset;

    if (state == ZR_NULL || function == ZR_NULL || block == ZR_NULL ||
        placeStates == ZR_NULL ||
        block->firstInstructionIndex > function->instructions.length ||
        block->instructionCount >
                function->instructions.length - block->firstInstructionIndex) {
        return ZR_FALSE;
    }
    for (offset = 0; offset < block->instructionCount; offset++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(
                        function,
                        (TZrSize)block->firstInstructionIndex + offset);
        semantic_flow_transfer_instruction(
                state,
                function,
                result,
                block->id,
                instruction,
                placeStates,
                recordDiagnostics);
    }
    return ZR_TRUE;
}

static TZrBool semantic_flow_initialize_blocks(
        SZrState *state,
        const SZrSemanticIrFunction *function,
        const SZrParserCfg *cfg,
        SZrSemanticFlowResult *result) {
    TZrSize index;

    for (index = 0; index < cfg->blocks.length; index++) {
        SZrSemanticBlockFlowFacts facts;

        memset(&facts, 0, sizeof(facts));
        facts.blockId = (TZrUInt32)index;
        if (!semantic_state_array_init(state, &facts.entryStates, function) ||
            !semantic_state_array_init(state, &facts.exitStates, function)) {
            semantic_block_facts_free(state, &facts);
            return ZR_FALSE;
        }
        ZrCore_Array_Push(state, &result->blockFacts, &facts);
    }
    result->placeCount = function->places.places.length;
    return ZR_TRUE;
}

static TZrBool semantic_flow_enqueue(TZrUInt32 *queue,
                                     TZrBool *queued,
                                     TZrSize capacity,
                                     TZrSize *tail,
                                     TZrSize *count,
                                     TZrUInt32 blockId) {
    if (queue == ZR_NULL || queued == ZR_NULL || tail == ZR_NULL ||
        count == ZR_NULL || blockId >= capacity || *count >= capacity) {
        return ZR_FALSE;
    }
    if (queued[blockId]) {
        return ZR_TRUE;
    }
    queue[*tail] = blockId;
    *tail = (*tail + 1U) % capacity;
    (*count)++;
    queued[blockId] = ZR_TRUE;
    return ZR_TRUE;
}

static TZrBool semantic_flow_record_diagnostics(
        SZrState *state,
        const SZrSemanticIrFunction *function,
        const SZrParserCfg *cfg,
        SZrSemanticFlowResult *result) {
    SZrArray scratch;
    TZrSize blockIndex;

    if (!semantic_state_array_init(state, &scratch, function)) {
        return ZR_FALSE;
    }
    result->diagnostics.length = 0U;
    for (blockIndex = 0; blockIndex < cfg->blocks.length; blockIndex++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&cfg->blocks, blockIndex);
        const SZrSemanticBlockFlowFacts *facts =
                ZrParser_SemanticFlow_BlockFacts(result, (TZrUInt32)blockIndex);
        if (facts == ZR_NULL || !facts->isReachable) {
            continue;
        }
        semantic_state_array_copy(state, &scratch, &facts->entryStates);
        if (!semantic_flow_transfer_block(
                    state, function, block, &scratch, result, ZR_TRUE)) {
            semantic_state_array_free(state, &scratch);
            return ZR_FALSE;
        }
    }
    semantic_state_array_free(state, &scratch);
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFlow_Analyze(
        SZrState *state,
        const SZrSemanticIrFunction *function,
        const SZrParserCfg *cfg,
        SZrSemanticFlowResult *result) {
    TZrUInt32 *queue;
    TZrBool *queued;
    TZrSize blockCount;
    TZrSize head = 0U;
    TZrSize tail = 0U;
    TZrSize count = 0U;
    SZrSemanticBlockFlowFacts *entryFacts;

    if (state == ZR_NULL || function == ZR_NULL || cfg == ZR_NULL ||
        result == ZR_NULL || result->state != state ||
        cfg != &function->cfg ||
        !ZrParser_SemanticIr_Validate(function) ||
        !cfg->blocks.isValid || cfg->blocks.length == 0U ||
        cfg->entryBlockId == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        cfg->entryBlockId >= cfg->blocks.length) {
        return ZR_FALSE;
    }

    semantic_flow_result_reset(state, result);
    if (!semantic_flow_initialize_blocks(state, function, cfg, result)) {
        semantic_flow_result_reset(state, result);
        return ZR_FALSE;
    }

    blockCount = cfg->blocks.length;
    queue = (TZrUInt32 *)ZrCore_Memory_RawMalloc(
            state->global, blockCount * sizeof(TZrUInt32));
    queued = (TZrBool *)ZrCore_Memory_RawMalloc(
            state->global, blockCount * sizeof(TZrBool));
    if (queue == ZR_NULL || queued == ZR_NULL) {
        if (queue != ZR_NULL) {
            ZrCore_Memory_RawFree(
                    state->global, queue, blockCount * sizeof(TZrUInt32));
        }
        if (queued != ZR_NULL) {
            ZrCore_Memory_RawFree(
                    state->global, queued, blockCount * sizeof(TZrBool));
        }
        semantic_flow_result_reset(state, result);
        return ZR_FALSE;
    }
    memset(queue, 0, blockCount * sizeof(TZrUInt32));
    memset(queued, 0, blockCount * sizeof(TZrBool));

    entryFacts = semantic_flow_block_facts_mutable(result, cfg->entryBlockId);
    entryFacts->isReachable = ZR_TRUE;
    entryFacts->hasEntryState = ZR_TRUE;
    semantic_flow_enqueue(
            queue,
            queued,
            blockCount,
            &tail,
            &count,
            cfg->entryBlockId);

    while (count > 0U) {
        TZrUInt32 blockId = queue[head];
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&cfg->blocks, blockId);
        SZrSemanticBlockFlowFacts *facts =
                semantic_flow_block_facts_mutable(result, blockId);
        TZrSize edgeIndex;

        head = (head + 1U) % blockCount;
        count--;
        queued[blockId] = ZR_FALSE;
        if (block == ZR_NULL || facts == ZR_NULL || !facts->isReachable ||
            !semantic_state_array_copy(
                    state, &facts->exitStates, &facts->entryStates) ||
            !semantic_flow_transfer_block(
                    state,
                    function,
                    block,
                    &facts->exitStates,
                    ZR_NULL,
                    ZR_FALSE)) {
            ZrCore_Memory_RawFree(
                    state->global, queue, blockCount * sizeof(TZrUInt32));
            ZrCore_Memory_RawFree(
                    state->global, queued, blockCount * sizeof(TZrBool));
            semantic_flow_result_reset(state, result);
            return ZR_FALSE;
        }

        for (edgeIndex = 0; edgeIndex < block->successorCount; edgeIndex++) {
            TZrUInt32 successorId =
                    ZrParser_Cfg_BlockSuccessorIdAt(block, edgeIndex);
            SZrSemanticBlockFlowFacts *successor =
                    semantic_flow_block_facts_mutable(result, successorId);
            TZrBool changed;

            if (successor == ZR_NULL) {
                continue;
            }
            if (!successor->hasEntryState) {
                semantic_state_array_copy(
                        state, &successor->entryStates, &facts->exitStates);
                successor->hasEntryState = ZR_TRUE;
                successor->isReachable = ZR_TRUE;
                changed = ZR_TRUE;
            } else {
                changed = semantic_state_array_join(
                        state, &successor->entryStates, &facts->exitStates);
            }
            if (changed &&
                !semantic_flow_enqueue(
                        queue,
                        queued,
                        blockCount,
                        &tail,
                        &count,
                        successorId)) {
                ZrCore_Memory_RawFree(
                        state->global, queue, blockCount * sizeof(TZrUInt32));
                ZrCore_Memory_RawFree(
                        state->global, queued, blockCount * sizeof(TZrBool));
                semantic_flow_result_reset(state, result);
                return ZR_FALSE;
            }
        }
    }

    ZrCore_Memory_RawFree(
            state->global, queue, blockCount * sizeof(TZrUInt32));
    ZrCore_Memory_RawFree(
            state->global, queued, blockCount * sizeof(TZrBool));
    if (!semantic_flow_record_diagnostics(state, function, cfg, result) ||
        !semantic_loan_liveness_analyze(state, function, cfg, result)) {
        semantic_flow_result_reset(state, result);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
