#include "semantic_ir_flow_internal.h"

#include <string.h>

static const TZrBool *loan_fact_row(
        const TZrBool *matrix,
        TZrSize row,
        TZrSize width) {
    return matrix + row * width;
}

static TZrBool loan_id_array_contains(
        const SZrArray *loanIds,
        TZrLoanId loanId) {
    for (TZrSize index = 0U; index < loanIds->length; index++) {
        const TZrLoanId *candidate = (const TZrLoanId *)ZrCore_Array_Get(
                (SZrArray *)loanIds, index);
        if (candidate != ZR_NULL && *candidate == loanId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void loan_append_ids(
        SZrState *state,
        SZrArray *loanIds,
        const TZrBool *set,
        TZrSize count) {
    for (TZrSize index = 0U; index < count; index++) {
        if (set[index]) {
            TZrLoanId loanId = (TZrLoanId)(index + 1U);
            ZrCore_Array_Push(state, loanIds, &loanId);
        }
    }
}

TZrBool semantic_loan_publish_liveness(SSemanticLoanAnalysis *analysis) {
    for (TZrSize index = 0U;
         index < analysis->instructionCount;
         index++) {
        SZrSemanticInstructionLoanLiveness fact;
        memset(&fact, 0, sizeof(fact));
        fact.instructionId = (TZrSemanticInstructionId)(index + 1U);
        ZrCore_Array_Init(
                analysis->state,
                &fact.liveInLoanIds,
                sizeof(TZrLoanId),
                analysis->loanCount > 0U ? analysis->loanCount : 1U);
        ZrCore_Array_Init(
                analysis->state,
                &fact.liveOutLoanIds,
                sizeof(TZrLoanId),
                analysis->loanCount > 0U ? analysis->loanCount : 1U);
        ZrCore_Array_Init(
                analysis->state,
                &fact.activeInLoanIds,
                sizeof(TZrLoanId),
                analysis->loanCount > 0U ? analysis->loanCount : 1U);
        ZrCore_Array_Init(
                analysis->state,
                &fact.activeOutLoanIds,
                sizeof(TZrLoanId),
                analysis->loanCount > 0U ? analysis->loanCount : 1U);
        loan_append_ids(
                analysis->state,
                &fact.liveInLoanIds,
                loan_fact_row(
                        analysis->instructionLiveIn,
                        index,
                        analysis->loanCount),
                analysis->loanCount);
        loan_append_ids(
                analysis->state,
                &fact.liveOutLoanIds,
                loan_fact_row(
                        analysis->instructionLiveOut,
                        index,
                        analysis->loanCount),
                analysis->loanCount);
        loan_append_ids(
                analysis->state,
                &fact.activeInLoanIds,
                loan_fact_row(
                        analysis->instructionActiveIn,
                        index,
                        analysis->loanCount),
                analysis->loanCount);
        loan_append_ids(
                analysis->state,
                &fact.activeOutLoanIds,
                loan_fact_row(
                        analysis->instructionActiveOut,
                        index,
                        analysis->loanCount),
                analysis->loanCount);
        ZrCore_Array_Push(
                analysis->state, &analysis->result->loanLiveness, &fact);
    }

    for (TZrSize loanIndex = 0U;
         loanIndex < analysis->loanCount;
         loanIndex++) {
        const SZrSemanticIrLoanFact *loan = ZrParser_SemanticIr_Loan(
                analysis->function, (TZrLoanId)(loanIndex + 1U));
        SZrSemanticLoanRegionFact region;
        memset(&region, 0, sizeof(region));
        region.loanId = (TZrLoanId)(loanIndex + 1U);
        region.parentLoanId = analysis->parentLoanIds[loanIndex];
        region.phase = loan->phase;
        region.originRange = loan->originRange;
        region.lastUseRange = loan->originRange;
        for (TZrSize instructionIndex = 0U;
             instructionIndex < analysis->instructionCount;
             instructionIndex++) {
            const SZrSemanticIrInstruction *instruction =
                    ZrParser_SemanticIr_InstructionAt(
                            analysis->function, instructionIndex);
            if (instruction->loanId == region.loanId &&
                (instruction->opcode == ZR_SEMANTIC_IR_BORROW_SHARED ||
                 instruction->opcode == ZR_SEMANTIC_IR_BORROW_MUT ||
                 instruction->opcode == ZR_SEMANTIC_IR_RESERVE_BORROW_MUT ||
                 instruction->opcode == ZR_SEMANTIC_IR_REBORROW)) {
                region.firstLiveInstructionId = instruction->id;
            }
            if (instruction->loanId == region.loanId &&
                instruction->opcode == ZR_SEMANTIC_IR_ACTIVATE_LOAN) {
                region.activationInstructionId = instruction->id;
            }
            if (loan_fact_row(
                        analysis->instructionUses,
                        instructionIndex,
                        analysis->loanCount)[loanIndex]) {
                region.lastUseInstructionId = instruction->id;
                region.lastUseRange = instruction->sourceRange;
            }
        }
        if (region.lastUseInstructionId == ZR_SEMANTIC_INSTRUCTION_ID_INVALID) {
            region.lastUseInstructionId = region.firstLiveInstructionId;
        }
        ZrCore_Array_Push(
                analysis->state, &analysis->result->loanRegions, &region);
    }
    analysis->result->loanCount = analysis->loanCount;
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticFlow_LoanIsLiveAt(
        const SZrSemanticFlowResult *result,
        TZrSemanticInstructionId instructionId,
        TZrLoanId loanId,
        TZrBool beforeInstruction) {
    const SZrSemanticInstructionLoanLiveness *fact;
    const SZrArray *loanIds;
    if (result == ZR_NULL ||
        instructionId == ZR_SEMANTIC_INSTRUCTION_ID_INVALID ||
        instructionId > result->loanLiveness.length ||
        loanId == ZR_SEMANTIC_LOAN_ID_INVALID) {
        return ZR_FALSE;
    }
    fact = (const SZrSemanticInstructionLoanLiveness *)ZrCore_Array_Get(
            (SZrArray *)&result->loanLiveness,
            (TZrSize)instructionId - 1U);
    loanIds = beforeInstruction ? &fact->liveInLoanIds : &fact->liveOutLoanIds;
    return loan_id_array_contains(loanIds, loanId);
}

TZrBool ZrParser_SemanticFlow_LoanIsActiveAt(
        const SZrSemanticFlowResult *result,
        TZrSemanticInstructionId instructionId,
        TZrLoanId loanId,
        TZrBool beforeInstruction) {
    const SZrSemanticLoanRegionFact *region;
    const SZrSemanticInstructionLoanLiveness *fact;
    const SZrArray *loanIds;

    if (!ZrParser_SemanticFlow_LoanIsLiveAt(
                result, instructionId, loanId, beforeInstruction)) {
        return ZR_FALSE;
    }
    region = ZrParser_SemanticFlow_LoanRegion(result, loanId);
    if (region == ZR_NULL || region->phase == ZR_SEMANTIC_LOAN_IMMEDIATE) {
        return region != ZR_NULL;
    }
    if (instructionId > result->loanLiveness.length) {
        return ZR_FALSE;
    }
    fact = (const SZrSemanticInstructionLoanLiveness *)ZrCore_Array_Get(
            (SZrArray *)&result->loanLiveness,
            (TZrSize)instructionId - 1U);
    if (fact == ZR_NULL) {
        return ZR_FALSE;
    }
    loanIds = beforeInstruction
                      ? &fact->activeInLoanIds
                      : &fact->activeOutLoanIds;
    return loan_id_array_contains(loanIds, loanId);
}

const SZrSemanticLoanRegionFact *ZrParser_SemanticFlow_LoanRegion(
        const SZrSemanticFlowResult *result,
        TZrLoanId loanId) {
    if (result == ZR_NULL || loanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
        loanId > result->loanRegions.length) {
        return ZR_NULL;
    }
    return (const SZrSemanticLoanRegionFact *)ZrCore_Array_Get(
            (SZrArray *)&result->loanRegions, (TZrSize)loanId - 1U);
}
