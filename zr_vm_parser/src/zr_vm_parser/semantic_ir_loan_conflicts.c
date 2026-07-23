#include "semantic_ir_flow_internal.h"

#include <string.h>

typedef enum ESemanticLoanInstructionAccess {
    SEMANTIC_LOAN_ACCESS_NONE = 0,
    SEMANTIC_LOAN_ACCESS_READ,
    SEMANTIC_LOAN_ACCESS_EXCLUSIVE,
    SEMANTIC_LOAN_ACCESS_BORROW_SHARED,
    SEMANTIC_LOAN_ACCESS_RESERVE_MUTABLE,
    SEMANTIC_LOAN_ACCESS_BORROW_MUTABLE
} ESemanticLoanInstructionAccess;

typedef enum ESemanticLoanEffectiveState {
    SEMANTIC_LOAN_STATE_SHARED = 0,
    SEMANTIC_LOAN_STATE_RESERVED_MUTABLE,
    SEMANTIC_LOAN_STATE_ACTIVE_MUTABLE
} ESemanticLoanEffectiveState;

static const TZrBool *loan_conflict_row(
        const TZrBool *matrix,
        TZrSize row,
        TZrSize width) {
    return matrix + row * width;
}

static ESemanticLoanInstructionAccess loan_instruction_access(
        const SSemanticLoanAnalysis *analysis,
        const SZrSemanticIrInstruction *instruction) {
    if (instruction->opcode == ZR_SEMANTIC_IR_BORROW_SHARED) {
        return SEMANTIC_LOAN_ACCESS_BORROW_SHARED;
    }
    if (instruction->opcode == ZR_SEMANTIC_IR_BORROW_MUT) {
        return SEMANTIC_LOAN_ACCESS_BORROW_MUTABLE;
    }
    if (instruction->opcode == ZR_SEMANTIC_IR_RESERVE_BORROW_MUT) {
        return SEMANTIC_LOAN_ACCESS_RESERVE_MUTABLE;
    }
    if (instruction->opcode == ZR_SEMANTIC_IR_ACTIVATE_LOAN) {
        return SEMANTIC_LOAN_ACCESS_BORROW_MUTABLE;
    }
    if (instruction->opcode == ZR_SEMANTIC_IR_REBORROW) {
        const SZrSemanticIrLoanFact *loan = ZrParser_SemanticIr_Loan(
                analysis->function, instruction->loanId);
        return loan != ZR_NULL && loan->access == ZR_SEMANTIC_LOAN_MUTABLE
                       ? SEMANTIC_LOAN_ACCESS_BORROW_MUTABLE
                       : SEMANTIC_LOAN_ACCESS_BORROW_SHARED;
    }
    if (instruction->opcode == ZR_SEMANTIC_IR_OWN_CONSTRUCT &&
        (instruction->ownershipOperation == ZR_SEMANTIC_OWNERSHIP_SHARE ||
         instruction->ownershipOperation == ZR_SEMANTIC_OWNERSHIP_INTO_GC_BOX ||
         instruction->ownershipOperation == ZR_SEMANTIC_OWNERSHIP_RETURN_TO_GC)) {
        return SEMANTIC_LOAN_ACCESS_EXCLUSIVE;
    }
    switch (instruction->opcode) {
        case ZR_SEMANTIC_IR_LOAD:
        case ZR_SEMANTIC_IR_COPY:
        case ZR_SEMANTIC_IR_DEREFERENCE:
        case ZR_SEMANTIC_IR_PROPERTY_GET:
        case ZR_SEMANTIC_IR_PROPERTY_REF_GET:
            return SEMANTIC_LOAN_ACCESS_READ;
        case ZR_SEMANTIC_IR_STORE:
        case ZR_SEMANTIC_IR_INITIALIZE:
        case ZR_SEMANTIC_IR_MOVE:
        case ZR_SEMANTIC_IR_DROP:
        case ZR_SEMANTIC_IR_PROPERTY_SET:
        case ZR_SEMANTIC_IR_FIELD_INITIALIZE:
        case ZR_SEMANTIC_IR_DESTRUCTURE_LEAF_ASSIGN:
        case ZR_SEMANTIC_IR_DESTRUCTURE_LEAF_BIND:
            return SEMANTIC_LOAN_ACCESS_EXCLUSIVE;
        default:
            return SEMANTIC_LOAN_ACCESS_NONE;
    }
}

static TZrBool loan_is_ancestor_or_self(
        const SSemanticLoanAnalysis *analysis,
        TZrLoanId candidate,
        TZrLoanId loanId) {
    if (candidate == loanId) {
        return ZR_TRUE;
    }
    return (TZrBool)(candidate != ZR_SEMANTIC_LOAN_ID_INVALID &&
                     candidate <= analysis->loanCount &&
                     loanId != ZR_SEMANTIC_LOAN_ID_INVALID &&
                     loanId <= analysis->loanCount &&
                     loan_conflict_row(
                             analysis->ancestorLoans,
                             (TZrSize)loanId - 1U,
                             analysis->loanCount)[
                             (TZrSize)candidate - 1U]);
}

static TZrBool loan_capability_authorizes_access(
        const SSemanticLoanAnalysis *analysis,
        const SZrSemanticIrInstruction *instruction,
        TZrLoanId loanId,
        ESemanticLoanInstructionAccess access) {
    const SZrSemanticIrLoanFact *loan = ZrParser_SemanticIr_Loan(
            analysis->function, loanId);
    if (loan == ZR_NULL) {
        return ZR_FALSE;
    }
    if (access == SEMANTIC_LOAN_ACCESS_READ ||
        access == SEMANTIC_LOAN_ACCESS_BORROW_SHARED) {
        return ZR_TRUE;
    }
    return (TZrBool)(loan->access == ZR_SEMANTIC_LOAN_MUTABLE &&
                     (loan->phase == ZR_SEMANTIC_LOAN_IMMEDIATE ||
                      ZrParser_SemanticFlow_LoanIsActiveAt(
                              analysis->result,
                              instruction->id,
                              loanId,
                              ZR_TRUE)));
}

static TZrBool loan_instruction_authorizes_candidate(
        const SSemanticLoanAnalysis *analysis,
        const SZrSemanticIrInstruction *instruction,
        ESemanticLoanInstructionAccess access,
        TZrLoanId candidateLoanId) {
    if (instruction->opcode == ZR_SEMANTIC_IR_BORROW_SHARED ||
        instruction->opcode == ZR_SEMANTIC_IR_BORROW_MUT ||
        instruction->opcode == ZR_SEMANTIC_IR_RESERVE_BORROW_MUT) {
        return ZR_FALSE;
    }
    if (instruction->opcode == ZR_SEMANTIC_IR_ACTIVATE_LOAN &&
        instruction->loanId == candidateLoanId) {
        return ZR_TRUE;
    }
    if (instruction->opcode == ZR_SEMANTIC_IR_REBORROW) {
        const TZrBool *directParents;
        if (instruction->loanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
            instruction->loanId > analysis->loanCount) {
            return ZR_FALSE;
        }
        directParents = loan_conflict_row(
                analysis->directParentLoans,
                (TZrSize)instruction->loanId - 1U,
                analysis->loanCount);
        for (TZrSize parentIndex = 0U;
             parentIndex < analysis->loanCount;
             parentIndex++) {
            TZrLoanId parentLoanId = (TZrLoanId)(parentIndex + 1U);
            if (directParents[parentIndex] &&
                loan_capability_authorizes_access(
                        analysis, instruction, parentLoanId, access) &&
                loan_is_ancestor_or_self(
                        analysis, candidateLoanId, parentLoanId)) {
                return ZR_TRUE;
            }
        }
        return ZR_FALSE;
    }
    return (TZrBool)(instruction->loanId !=
                             ZR_SEMANTIC_LOAN_ID_INVALID &&
                     instruction->loanId <= analysis->loanCount &&
                      loan_capability_authorizes_access(
                              analysis,
                              instruction,
                              instruction->loanId,
                              access) &&
                     loan_is_ancestor_or_self(
                             analysis,
                             candidateLoanId,
                             instruction->loanId));
}

static ESemanticLoanEffectiveState loan_effective_state(
        const SSemanticLoanAnalysis *analysis,
        const SZrSemanticIrInstruction *instruction,
        const SZrSemanticIrLoanFact *loan) {
    const TZrBool *mayActive;
    if (loan->access == ZR_SEMANTIC_LOAN_SHARED) {
        return SEMANTIC_LOAN_STATE_SHARED;
    }
    if (loan->phase == ZR_SEMANTIC_LOAN_TWO_PHASE) {
        mayActive = loan_conflict_row(
                analysis->instructionMayActiveIn,
                (TZrSize)instruction->id - 1U,
                analysis->loanCount);
        if (!mayActive[(TZrSize)loan->loanId - 1U]) {
            return SEMANTIC_LOAN_STATE_RESERVED_MUTABLE;
        }
    }
    return SEMANTIC_LOAN_STATE_ACTIVE_MUTABLE;
}

static TZrBool loan_access_conflicts(
        ESemanticLoanInstructionAccess access,
        ESemanticLoanEffectiveState activeState) {
    switch (access) {
        case SEMANTIC_LOAN_ACCESS_READ:
        case SEMANTIC_LOAN_ACCESS_BORROW_SHARED:
            return activeState == SEMANTIC_LOAN_STATE_ACTIVE_MUTABLE;
        case SEMANTIC_LOAN_ACCESS_RESERVE_MUTABLE:
            return activeState != SEMANTIC_LOAN_STATE_SHARED;
        case SEMANTIC_LOAN_ACCESS_EXCLUSIVE:
        case SEMANTIC_LOAN_ACCESS_BORROW_MUTABLE:
            return ZR_TRUE;
        case SEMANTIC_LOAN_ACCESS_NONE:
        default:
            return ZR_FALSE;
    }
}

static const SZrSemanticIrLoanFact *loan_reborrow_capability_conflict(
        const SSemanticLoanAnalysis *analysis,
        const SZrSemanticIrInstruction *instruction,
        ESemanticLoanInstructionAccess access) {
    const TZrBool *directParents;
    if (instruction->opcode != ZR_SEMANTIC_IR_REBORROW ||
        access != SEMANTIC_LOAN_ACCESS_BORROW_MUTABLE ||
        instruction->loanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
        instruction->loanId > analysis->loanCount) {
        return ZR_NULL;
    }
    directParents = loan_conflict_row(
            analysis->directParentLoans,
            (TZrSize)instruction->loanId - 1U,
            analysis->loanCount);
    for (TZrSize parentIndex = 0U;
         parentIndex < analysis->loanCount;
         parentIndex++) {
        const SZrSemanticIrLoanFact *parentLoan;
        if (!directParents[parentIndex]) {
            continue;
        }
        parentLoan = ZrParser_SemanticIr_Loan(
                analysis->function, (TZrLoanId)(parentIndex + 1U));
        if (parentLoan != ZR_NULL &&
            parentLoan->access != ZR_SEMANTIC_LOAN_MUTABLE) {
            return parentLoan;
        }
    }
    return ZR_NULL;
}

static void loan_add_conflict_diagnostic(
        SSemanticLoanAnalysis *analysis,
        const SZrSemanticIrInstruction *instruction,
        const SZrSemanticIrLoanFact *loan,
        EZrParserPlaceOverlap overlap) {
    SZrSemanticFlowDiagnostic diagnostic;
    const SZrSemanticLoanRegionFact *region = ZrParser_SemanticFlow_LoanRegion(
            analysis->result, loan->loanId);
    const SZrParserPlace *sourcePlace = ZrParser_PlaceGraph_Get(
            &analysis->function->places, loan->sourcePlaceId);
    for (TZrSize index = 0U;
         index < analysis->result->diagnostics.length;
         index++) {
        const SZrSemanticFlowDiagnostic *existing =
                (const SZrSemanticFlowDiagnostic *)ZrCore_Array_Get(
                        &analysis->result->diagnostics, index);
        if (existing != ZR_NULL &&
            existing->kind == ZR_SEMANTIC_FLOW_LOAN_CONFLICT &&
            existing->instructionId == instruction->id) {
            return;
        }
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.kind = ZR_SEMANTIC_FLOW_LOAN_CONFLICT;
    diagnostic.blockId =
            analysis->instructionBlockIds[(TZrSize)instruction->id - 1U];
    diagnostic.instructionId = instruction->id;
    diagnostic.placeId = instruction->placeId;
    diagnostic.relatedLoanId = loan->loanId;
    diagnostic.relatedPlaceId = loan->sourcePlaceId;
    diagnostic.overlap = overlap;
    diagnostic.sourceRange = instruction->sourceRange;
    if (sourcePlace != ZR_NULL) {
        diagnostic.placeDeclarationRange = sourcePlace->sourceRange;
    }
    diagnostic.loanOriginRange = loan->originRange;
    diagnostic.loanLastUseRange =
            region != ZR_NULL ? region->lastUseRange : loan->lastUseRange;
    ZrCore_Array_Push(
            analysis->state, &analysis->result->diagnostics, &diagnostic);
}

void semantic_loan_check_conflicts(SSemanticLoanAnalysis *analysis) {
    for (TZrSize instructionIndex = 0U;
         instructionIndex < analysis->instructionCount;
         instructionIndex++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(
                        analysis->function, instructionIndex);
        ESemanticLoanInstructionAccess access =
                loan_instruction_access(analysis, instruction);
        const SZrSemanticIrLoanFact *capabilityConflict;
        const TZrBool *liveIn;

        if (analysis->instructionBlockIds[instructionIndex] ==
                    ZR_PARSER_CFG_INVALID_BLOCK_ID ||
            access == SEMANTIC_LOAN_ACCESS_NONE ||
            instruction->placeId == ZR_PLACE_ID_INVALID) {
            continue;
        }
        capabilityConflict = loan_reborrow_capability_conflict(
                analysis, instruction, access);
        if (capabilityConflict != ZR_NULL) {
            loan_add_conflict_diagnostic(
                    analysis,
                    instruction,
                    capabilityConflict,
                    ZrParser_PlaceGraph_Overlap(
                            &analysis->function->places,
                            instruction->placeId,
                            capabilityConflict->sourcePlaceId));
            continue;
        }
        liveIn = loan_conflict_row(
                analysis->instructionLiveIn,
                instructionIndex,
                analysis->loanCount);
        for (TZrSize loanIndex = 0U;
             loanIndex < analysis->loanCount;
             loanIndex++) {
            const SZrSemanticIrLoanFact *loan;
            EZrParserPlaceOverlap overlap;
            if (!liveIn[loanIndex]) {
                continue;
            }
            loan = ZrParser_SemanticIr_Loan(
                    analysis->function, (TZrLoanId)(loanIndex + 1U));
            if (loan == ZR_NULL) {
                continue;
            }
            if (loan_instruction_authorizes_candidate(
                        analysis,
                        instruction,
                        access,
                        loan->loanId)) {
                continue;
            }
            overlap = ZrParser_PlaceGraph_Overlap(
                    &analysis->function->places,
                    instruction->placeId,
                    loan->sourcePlaceId);
            if (overlap != ZR_PARSER_PLACE_DISJOINT &&
                loan_access_conflicts(
                        access,
                        loan_effective_state(
                                analysis, instruction, loan))) {
                loan_add_conflict_diagnostic(
                        analysis, instruction, loan, overlap);
                break;
            }
        }
    }
}
