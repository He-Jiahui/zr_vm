#include "semantic_ir_flow_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct SLoanActivationBlocks {
    TZrBool *mayIn;
    TZrBool *mayOut;
    TZrBool *mustIn;
    TZrBool *mustOut;
    TZrBool *availableIn;
    TZrBool *availableOut;
    TZrBool *instructionAvailableIn;
} SLoanActivationBlocks;

static TZrBool *activation_row(
        TZrBool *matrix,
        TZrSize row,
        TZrSize width) {
    return matrix + row * width;
}

static const TZrBool *activation_const_row(
        const TZrBool *matrix,
        TZrSize row,
        TZrSize width) {
    return matrix + row * width;
}

static TZrBool activation_set_equal(
        const TZrBool *left,
        const TZrBool *right,
        TZrSize count) {
    return (TZrBool)(memcmp(left, right, count * sizeof(TZrBool)) == 0);
}

static TZrBool activation_block_is_reachable(
        const SSemanticLoanAnalysis *analysis,
        TZrSize blockIndex) {
    const SZrSemanticBlockFlowFacts *facts =
            ZrParser_SemanticFlow_BlockFacts(
                    analysis->result, (TZrUInt32)blockIndex);
    return (TZrBool)(facts != ZR_NULL && facts->isReachable);
}

static TZrBool activation_block_targets(
        const SZrParserCfgBlock *block,
        TZrSize successorIndex) {
    for (TZrSize edgeIndex = 0U;
         edgeIndex < block->successorCount;
         edgeIndex++) {
        if (ZrParser_Cfg_BlockSuccessorIdAt(block, edgeIndex) ==
            successorIndex) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool activation_instruction_is_origin(
        const SZrSemanticIrInstruction *instruction) {
    return (TZrBool)(instruction->opcode == ZR_SEMANTIC_IR_BORROW_SHARED ||
                     instruction->opcode == ZR_SEMANTIC_IR_BORROW_MUT ||
                     instruction->opcode == ZR_SEMANTIC_IR_RESERVE_BORROW_MUT ||
                     instruction->opcode == ZR_SEMANTIC_IR_REBORROW);
}

static void activation_transfer_instruction(
        const SSemanticLoanAnalysis *analysis,
        const SZrSemanticIrInstruction *instruction,
        TZrBool *mayActive,
        TZrBool *mustActive,
        TZrBool *available) {
    const SZrSemanticIrLoanFact *loan;
    TZrSize loanIndex;

    if (instruction->loanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
        instruction->loanId > analysis->loanCount) {
        return;
    }
    loanIndex = (TZrSize)instruction->loanId - 1U;
    loan = ZrParser_SemanticIr_Loan(
            analysis->function, instruction->loanId);
    if (loan == ZR_NULL) {
        return;
    }
    if (activation_instruction_is_origin(instruction)) {
        available[loanIndex] = ZR_TRUE;
        mayActive[loanIndex] =
                loan->phase == ZR_SEMANTIC_LOAN_IMMEDIATE;
        mustActive[loanIndex] = mayActive[loanIndex];
    } else if (instruction->opcode == ZR_SEMANTIC_IR_ACTIVATE_LOAN) {
        mayActive[loanIndex] = ZR_TRUE;
        mustActive[loanIndex] = ZR_TRUE;
    } else if (instruction->opcode == ZR_SEMANTIC_IR_END_LOAN) {
        available[loanIndex] = ZR_FALSE;
        mayActive[loanIndex] = ZR_FALSE;
        mustActive[loanIndex] = ZR_FALSE;
    }
}

static void activation_merge_predecessors(
        const SSemanticLoanAnalysis *analysis,
        const SLoanActivationBlocks *blocks,
        TZrSize blockIndex,
        TZrBool *mayIn,
        TZrBool *mustIn,
        TZrBool *availableIn) {
    TZrBool foundPredecessor = ZR_FALSE;

    memset(mayIn, 0, analysis->loanCount * sizeof(TZrBool));
    memset(mustIn, 0, analysis->loanCount * sizeof(TZrBool));
    memset(availableIn, 0, analysis->loanCount * sizeof(TZrBool));
    if (blockIndex == analysis->cfg->entryBlockId) {
        return;
    }
    for (TZrSize predecessorIndex = 0U;
         predecessorIndex < analysis->blockCount;
         predecessorIndex++) {
        const SZrParserCfgBlock *predecessor;
        const TZrBool *predecessorMay;
        const TZrBool *predecessorMust;
        const TZrBool *predecessorAvailable;

        if (!activation_block_is_reachable(analysis, predecessorIndex)) {
            continue;
        }
        predecessor = (const SZrParserCfgBlock *)ZrCore_Array_Get(
                (SZrArray *)&analysis->cfg->blocks, predecessorIndex);
        if (predecessor == ZR_NULL ||
            !activation_block_targets(predecessor, blockIndex)) {
            continue;
        }
        predecessorMay = activation_const_row(
                blocks->mayOut, predecessorIndex, analysis->loanCount);
        predecessorMust = activation_const_row(
                blocks->mustOut, predecessorIndex, analysis->loanCount);
        predecessorAvailable = activation_const_row(
                blocks->availableOut,
                predecessorIndex,
                analysis->loanCount);
        if (!foundPredecessor) {
            memcpy(mayIn, predecessorMay,
                   analysis->loanCount * sizeof(TZrBool));
            memcpy(mustIn, predecessorMust,
                   analysis->loanCount * sizeof(TZrBool));
            memcpy(availableIn, predecessorAvailable,
                   analysis->loanCount * sizeof(TZrBool));
            foundPredecessor = ZR_TRUE;
            continue;
        }
        for (TZrSize loanIndex = 0U;
             loanIndex < analysis->loanCount;
             loanIndex++) {
            mayIn[loanIndex] =
                    (TZrBool)(mayIn[loanIndex] || predecessorMay[loanIndex]);
            mustIn[loanIndex] =
                    (TZrBool)(mustIn[loanIndex] && predecessorMust[loanIndex]);
            availableIn[loanIndex] =
                    (TZrBool)(availableIn[loanIndex] &&
                              predecessorAvailable[loanIndex]);
        }
    }
}

static TZrBool activation_update_row(
        TZrBool *destination,
        const TZrBool *source,
        TZrSize count) {
    if (activation_set_equal(destination, source, count)) {
        return ZR_FALSE;
    }
    memcpy(destination, source, count * sizeof(TZrBool));
    return ZR_TRUE;
}

static void activation_blocks_free(SLoanActivationBlocks *blocks) {
    free(blocks->mayIn);
    free(blocks->mayOut);
    free(blocks->mustIn);
    free(blocks->mustOut);
    free(blocks->availableIn);
    free(blocks->availableOut);
    free(blocks->instructionAvailableIn);
    memset(blocks, 0, sizeof(*blocks));
}

static TZrBool activation_blocks_allocate(
        const SSemanticLoanAnalysis *analysis,
        SLoanActivationBlocks *blocks) {
    TZrSize blockCells = analysis->blockCount * analysis->loanCount;
    TZrSize instructionCells =
            analysis->instructionCount * analysis->loanCount;

    memset(blocks, 0, sizeof(*blocks));
    blocks->mayIn = (TZrBool *)calloc(blockCells, sizeof(TZrBool));
    blocks->mayOut = (TZrBool *)calloc(blockCells, sizeof(TZrBool));
    blocks->mustIn = (TZrBool *)malloc(blockCells * sizeof(TZrBool));
    blocks->mustOut = (TZrBool *)malloc(blockCells * sizeof(TZrBool));
    blocks->availableIn = (TZrBool *)malloc(blockCells * sizeof(TZrBool));
    blocks->availableOut = (TZrBool *)malloc(blockCells * sizeof(TZrBool));
    blocks->instructionAvailableIn =
            (TZrBool *)calloc(instructionCells, sizeof(TZrBool));
    if (blocks->mayIn == ZR_NULL || blocks->mayOut == ZR_NULL ||
        blocks->mustIn == ZR_NULL || blocks->mustOut == ZR_NULL ||
        blocks->availableIn == ZR_NULL ||
        blocks->availableOut == ZR_NULL ||
        blocks->instructionAvailableIn == ZR_NULL) {
        activation_blocks_free(blocks);
        return ZR_FALSE;
    }
    for (TZrSize cell = 0U; cell < blockCells; cell++) {
        blocks->mustIn[cell] = ZR_TRUE;
        blocks->mustOut[cell] = ZR_TRUE;
        blocks->availableIn[cell] = ZR_TRUE;
        blocks->availableOut[cell] = ZR_TRUE;
    }
    return ZR_TRUE;
}

static TZrBool activation_validate_origins(
        const SSemanticLoanAnalysis *analysis,
        const SLoanActivationBlocks *blocks) {
    for (TZrSize instructionIndex = 0U;
         instructionIndex < analysis->instructionCount;
         instructionIndex++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(
                        analysis->function, instructionIndex);
        TZrSize loanIndex;

        if (instruction->opcode != ZR_SEMANTIC_IR_ACTIVATE_LOAN) {
            continue;
        }
        if (analysis->instructionBlockIds[instructionIndex] ==
                    ZR_PARSER_CFG_INVALID_BLOCK_ID ||
            instruction->loanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
            instruction->loanId > analysis->loanCount) {
            return ZR_FALSE;
        }
        loanIndex = (TZrSize)instruction->loanId - 1U;
        if (!activation_const_row(
                    blocks->instructionAvailableIn,
                    instructionIndex,
                    analysis->loanCount)[loanIndex]) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool semantic_loan_activation_analyze(SSemanticLoanAnalysis *analysis) {
    SLoanActivationBlocks blocks;
    TZrBool *newMayIn;
    TZrBool *newMustIn;
    TZrBool *newAvailableIn;
    TZrBool *workMay;
    TZrBool *workMust;
    TZrBool *workAvailable;
    TZrBool changed;
    TZrSize pass = 0U;
    TZrSize passLimit =
            analysis->blockCount * (analysis->loanCount + 1U) * 4U + 1U;

    if (!activation_blocks_allocate(analysis, &blocks)) {
        return ZR_FALSE;
    }
    newMayIn = (TZrBool *)calloc(analysis->loanCount, sizeof(TZrBool));
    newMustIn = (TZrBool *)calloc(analysis->loanCount, sizeof(TZrBool));
    newAvailableIn =
            (TZrBool *)calloc(analysis->loanCount, sizeof(TZrBool));
    workMay = (TZrBool *)calloc(analysis->loanCount, sizeof(TZrBool));
    workMust = (TZrBool *)calloc(analysis->loanCount, sizeof(TZrBool));
    workAvailable =
            (TZrBool *)calloc(analysis->loanCount, sizeof(TZrBool));
    if (newMayIn == ZR_NULL || newMustIn == ZR_NULL ||
        newAvailableIn == ZR_NULL || workMay == ZR_NULL ||
        workMust == ZR_NULL || workAvailable == ZR_NULL) {
        free(newMayIn);
        free(newMustIn);
        free(newAvailableIn);
        free(workMay);
        free(workMust);
        free(workAvailable);
        activation_blocks_free(&blocks);
        return ZR_FALSE;
    }

    do {
        changed = ZR_FALSE;
        for (TZrSize blockIndex = 0U;
             blockIndex < analysis->blockCount;
             blockIndex++) {
            const SZrParserCfgBlock *block;

            if (!activation_block_is_reachable(analysis, blockIndex)) {
                continue;
            }
            block = (const SZrParserCfgBlock *)ZrCore_Array_Get(
                    (SZrArray *)&analysis->cfg->blocks, blockIndex);
            activation_merge_predecessors(
                    analysis,
                    &blocks,
                    blockIndex,
                    newMayIn,
                    newMustIn,
                    newAvailableIn);
            changed = (TZrBool)(activation_update_row(
                                         activation_row(
                                                 blocks.mayIn,
                                                 blockIndex,
                                                 analysis->loanCount),
                                         newMayIn,
                                         analysis->loanCount) ||
                                 changed);
            changed = (TZrBool)(activation_update_row(
                                         activation_row(
                                                 blocks.mustIn,
                                                 blockIndex,
                                                 analysis->loanCount),
                                         newMustIn,
                                         analysis->loanCount) ||
                                 changed);
            changed = (TZrBool)(activation_update_row(
                                         activation_row(
                                                 blocks.availableIn,
                                                 blockIndex,
                                                 analysis->loanCount),
                                         newAvailableIn,
                                         analysis->loanCount) ||
                                 changed);
            memcpy(workMay, newMayIn,
                   analysis->loanCount * sizeof(TZrBool));
            memcpy(workMust, newMustIn,
                   analysis->loanCount * sizeof(TZrBool));
            memcpy(workAvailable, newAvailableIn,
                   analysis->loanCount * sizeof(TZrBool));
            for (TZrSize offset = 0U;
                 offset < block->instructionCount;
                 offset++) {
                TZrSize instructionIndex =
                        block->firstInstructionIndex + offset;
                const SZrSemanticIrInstruction *instruction =
                        ZrParser_SemanticIr_InstructionAt(
                                analysis->function, instructionIndex);
                memcpy(activation_row(
                               analysis->instructionMayActiveIn,
                               instructionIndex,
                               analysis->loanCount),
                       workMay,
                       analysis->loanCount * sizeof(TZrBool));
                memcpy(activation_row(
                               analysis->instructionActiveIn,
                               instructionIndex,
                               analysis->loanCount),
                       workMust,
                       analysis->loanCount * sizeof(TZrBool));
                memcpy(activation_row(
                               blocks.instructionAvailableIn,
                               instructionIndex,
                               analysis->loanCount),
                       workAvailable,
                       analysis->loanCount * sizeof(TZrBool));
                activation_transfer_instruction(
                        analysis,
                        instruction,
                        workMay,
                        workMust,
                        workAvailable);
                memcpy(activation_row(
                               analysis->instructionMayActiveOut,
                               instructionIndex,
                               analysis->loanCount),
                       workMay,
                       analysis->loanCount * sizeof(TZrBool));
                memcpy(activation_row(
                               analysis->instructionActiveOut,
                               instructionIndex,
                               analysis->loanCount),
                       workMust,
                       analysis->loanCount * sizeof(TZrBool));
            }
            changed = (TZrBool)(activation_update_row(
                                         activation_row(
                                                 blocks.mayOut,
                                                 blockIndex,
                                                 analysis->loanCount),
                                         workMay,
                                         analysis->loanCount) ||
                                 changed);
            changed = (TZrBool)(activation_update_row(
                                         activation_row(
                                                 blocks.mustOut,
                                                 blockIndex,
                                                 analysis->loanCount),
                                         workMust,
                                         analysis->loanCount) ||
                                 changed);
            changed = (TZrBool)(activation_update_row(
                                         activation_row(
                                                 blocks.availableOut,
                                                 blockIndex,
                                                 analysis->loanCount),
                                         workAvailable,
                                         analysis->loanCount) ||
                                 changed);
        }
        pass++;
    } while (changed && pass < passLimit);

    free(newMayIn);
    free(newMustIn);
    free(newAvailableIn);
    free(workMay);
    free(workMust);
    free(workAvailable);
    if (changed || !activation_validate_origins(analysis, &blocks)) {
        activation_blocks_free(&blocks);
        return ZR_FALSE;
    }
    activation_blocks_free(&blocks);
    return ZR_TRUE;
}
