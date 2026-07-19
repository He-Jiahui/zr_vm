#include "semantic_ir_flow_internal.h"

#include <stdlib.h>
#include <string.h>

static TZrBool *loan_row(TZrBool *matrix, TZrSize row, TZrSize width) {
    return matrix + row * width;
}

static const TZrBool *loan_const_row(
        const TZrBool *matrix,
        TZrSize row,
        TZrSize width) {
    return matrix + row * width;
}

static TZrBool loan_set_union(
        TZrBool *destination,
        const TZrBool *source,
        TZrSize count) {
    TZrBool changed = ZR_FALSE;
    for (TZrSize index = 0U; index < count; index++) {
        if (source[index] && !destination[index]) {
            destination[index] = ZR_TRUE;
            changed = ZR_TRUE;
        }
    }
    return changed;
}

static TZrBool loan_set_equal(
        const TZrBool *left,
        const TZrBool *right,
        TZrSize count) {
    return (TZrBool)(memcmp(left, right, count * sizeof(TZrBool)) == 0);
}

static void loan_collect_value(
        const SSemanticLoanAnalysis *analysis,
        TZrValueId valueId,
        TZrBool *destination) {
    if (valueId == ZR_VALUE_ID_INVALID || valueId > analysis->valueCount) {
        return;
    }
    (void)loan_set_union(
            destination,
            loan_const_row(
                    analysis->valueLoans,
                    (TZrSize)valueId - 1U,
                    analysis->loanCount),
            analysis->loanCount);
}

static void loan_collect_instruction_inputs(
        const SSemanticLoanAnalysis *analysis,
        const SZrSemanticIrInstruction *instruction,
        TZrBool *destination) {
    loan_collect_value(analysis, instruction->valueId, destination);
    loan_collect_value(analysis, instruction->auxiliaryValueId, destination);
    for (TZrSize index = 0U; index < instruction->operandCount; index++) {
        const TZrValueId *operand = (const TZrValueId *)ZrCore_Array_Get(
                (SZrArray *)&analysis->function->valueOperands,
                instruction->operandStart + index);
        if (operand != ZR_NULL) {
            loan_collect_value(analysis, *operand, destination);
        }
    }
}

static TZrBool loan_opcode_propagates_place_to_result(
        EZrSemanticIrOpcode opcode) {
    return (TZrBool)(opcode == ZR_SEMANTIC_IR_LOAD ||
                     opcode == ZR_SEMANTIC_IR_COPY ||
                     opcode == ZR_SEMANTIC_IR_MOVE ||
                     opcode == ZR_SEMANTIC_IR_DEREFERENCE ||
                     opcode == ZR_SEMANTIC_IR_PROPERTY_GET ||
                     opcode == ZR_SEMANTIC_IR_PROPERTY_REF_GET);
}

static TZrBool loan_opcode_propagates_input_to_result(
        EZrSemanticIrOpcode opcode) {
    return (TZrBool)(opcode == ZR_SEMANTIC_IR_CONVERT ||
                     opcode == ZR_SEMANTIC_IR_COPY ||
                     opcode == ZR_SEMANTIC_IR_MOVE);
}

static TZrBool loan_opcode_stores_value(EZrSemanticIrOpcode opcode) {
    return (TZrBool)(opcode == ZR_SEMANTIC_IR_STORE ||
                     opcode == ZR_SEMANTIC_IR_INITIALIZE ||
                     opcode == ZR_SEMANTIC_IR_PROPERTY_SET ||
                     opcode == ZR_SEMANTIC_IR_FIELD_INITIALIZE ||
                     opcode == ZR_SEMANTIC_IR_DESTRUCTURE_LEAF_ASSIGN ||
                     opcode == ZR_SEMANTIC_IR_DESTRUCTURE_LEAF_BIND);
}

static TZrBool loan_seed_and_propagate_values(SSemanticLoanAnalysis *analysis) {
    TZrBool *inputs = (TZrBool *)calloc(
            analysis->loanCount, sizeof(TZrBool));
    TZrBool changed;
    TZrSize pass = 0U;
    TZrSize passLimit = analysis->valueCount + analysis->placeCount +
                        analysis->instructionCount + 1U;

    if (inputs == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < analysis->loanCount; index++) {
        const SZrSemanticIrLoanFact *loan = ZrParser_SemanticIr_Loan(
                analysis->function, (TZrLoanId)(index + 1U));
        if (loan != ZR_NULL && loan->createdByValueId != ZR_VALUE_ID_INVALID &&
            loan->createdByValueId <= analysis->valueCount) {
            loan_row(
                    analysis->valueLoans,
                    (TZrSize)loan->createdByValueId - 1U,
                    analysis->loanCount)[index] = ZR_TRUE;
        }
    }

    do {
        changed = ZR_FALSE;
        for (TZrSize index = 0U;
             index < analysis->instructionCount;
             index++) {
            const SZrSemanticIrInstruction *instruction =
                    ZrParser_SemanticIr_InstructionAt(
                            analysis->function, index);
            TZrBool *resultLoans = ZR_NULL;
            TZrBool *placeLoans = ZR_NULL;
            memset(inputs, 0, analysis->loanCount * sizeof(TZrBool));
            loan_collect_instruction_inputs(analysis, instruction, inputs);

            if (instruction->placeId != ZR_PLACE_ID_INVALID &&
                instruction->placeId <= analysis->placeCount) {
                placeLoans = loan_row(
                        analysis->placeLoans,
                        (TZrSize)instruction->placeId - 1U,
                        analysis->loanCount);
            }
            if (instruction->resultValueId != ZR_VALUE_ID_INVALID &&
                instruction->resultValueId <= analysis->valueCount) {
                resultLoans = loan_row(
                        analysis->valueLoans,
                        (TZrSize)instruction->resultValueId - 1U,
                        analysis->loanCount);
            }
            if (placeLoans != ZR_NULL &&
                loan_opcode_stores_value(instruction->opcode)) {
                changed = (TZrBool)(loan_set_union(
                                             placeLoans,
                                             inputs,
                                             analysis->loanCount) ||
                                     changed);
            }
            if (resultLoans != ZR_NULL && placeLoans != ZR_NULL &&
                loan_opcode_propagates_place_to_result(instruction->opcode)) {
                changed = (TZrBool)(loan_set_union(
                                             resultLoans,
                                             placeLoans,
                                             analysis->loanCount) ||
                                     changed);
            }
            if (resultLoans != ZR_NULL &&
                loan_opcode_propagates_input_to_result(instruction->opcode)) {
                changed = (TZrBool)(loan_set_union(
                                             resultLoans,
                                             inputs,
                                             analysis->loanCount) ||
                                     changed);
            }
        }
        pass++;
    } while (changed && pass < passLimit);

    free(inputs);
    return (TZrBool)!changed;
}

static void loan_seed_created_values(SSemanticLoanAnalysis *analysis) {
    memset(
            analysis->valueLoans,
            0,
            analysis->valueCount * analysis->loanCount * sizeof(TZrBool));
    for (TZrSize index = 0U; index < analysis->loanCount; index++) {
        const SZrSemanticIrLoanFact *loan = ZrParser_SemanticIr_Loan(
                analysis->function, (TZrLoanId)(index + 1U));
        if (loan != ZR_NULL && loan->createdByValueId != ZR_VALUE_ID_INVALID &&
            loan->createdByValueId <= analysis->valueCount) {
            loan_row(
                    analysis->valueLoans,
                    (TZrSize)loan->createdByValueId - 1U,
                    analysis->loanCount)[index] = ZR_TRUE;
        }
    }
}

static TZrBool loan_prepare_tracked_places(SSemanticLoanAnalysis *analysis) {
    TZrSize stateCells;

    analysis->trackedPlaceIndices = (TZrSize *)calloc(
            analysis->placeCount, sizeof(TZrSize));
    if (analysis->trackedPlaceIndices == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize placeIndex = 0U;
         placeIndex < analysis->placeCount;
         placeIndex++) {
        const TZrBool *candidateLoans = loan_const_row(
                analysis->placeLoans, placeIndex, analysis->loanCount);
        for (TZrSize loanIndex = 0U;
             loanIndex < analysis->loanCount;
             loanIndex++) {
            if (candidateLoans[loanIndex]) {
                analysis->trackedPlaceIndices[placeIndex] =
                        ++analysis->trackedPlaceCount;
                break;
            }
        }
    }
    stateCells = analysis->blockCount * analysis->trackedPlaceCount *
                 analysis->loanCount;
    analysis->blockPlaceIn = (TZrBool *)calloc(
            stateCells > 0U ? stateCells : 1U, sizeof(TZrBool));
    analysis->blockPlaceOut = (TZrBool *)calloc(
            stateCells > 0U ? stateCells : 1U, sizeof(TZrBool));
    return (TZrBool)(analysis->blockPlaceIn != ZR_NULL &&
                     analysis->blockPlaceOut != ZR_NULL);
}

static void loan_record_reborrow_parents(
        SSemanticLoanAnalysis *analysis,
        const SZrSemanticIrInstruction *instruction,
        const TZrBool *inputs) {
    TZrSize childIndex;
    TZrBool *directParents;

    if (instruction->opcode != ZR_SEMANTIC_IR_REBORROW ||
        instruction->loanId == ZR_SEMANTIC_LOAN_ID_INVALID) {
        return;
    }
    childIndex = (TZrSize)instruction->loanId - 1U;
    directParents = loan_row(
            analysis->directParentLoans, childIndex, analysis->loanCount);
    for (TZrSize loanIndex = 0U;
         loanIndex < analysis->loanCount;
         loanIndex++) {
        TZrLoanId parentLoanId = (TZrLoanId)(loanIndex + 1U);
        if (!inputs[loanIndex] || loanIndex == childIndex) {
            continue;
        }
        directParents[loanIndex] = ZR_TRUE;
        if (analysis->parentLoanIds[childIndex] ==
            ZR_SEMANTIC_LOAN_ID_INVALID) {
            analysis->parentLoanIds[childIndex] = parentLoanId;
        } else if (analysis->parentLoanIds[childIndex] != parentLoanId) {
            analysis->parentLoanIds[childIndex] =
                    ZR_SEMANTIC_LOAN_ID_MULTIPLE;
        }
    }
}

static TZrBool loan_finalize_reborrow_graph(
        SSemanticLoanAnalysis *analysis) {
    TZrBool changed;
    TZrSize pass = 0U;

    memcpy(
            analysis->ancestorLoans,
            analysis->directParentLoans,
            analysis->loanCount * analysis->loanCount * sizeof(TZrBool));
    do {
        changed = ZR_FALSE;
        for (TZrSize childIndex = 0U;
             childIndex < analysis->loanCount;
             childIndex++) {
            TZrBool *ancestors = loan_row(
                    analysis->ancestorLoans,
                    childIndex,
                    analysis->loanCount);
            const TZrBool *directParents = loan_const_row(
                    analysis->directParentLoans,
                    childIndex,
                    analysis->loanCount);
            for (TZrSize parentIndex = 0U;
                 parentIndex < analysis->loanCount;
                 parentIndex++) {
                if (directParents[parentIndex]) {
                    changed = (TZrBool)(loan_set_union(
                                                 ancestors,
                                                 loan_const_row(
                                                         analysis->ancestorLoans,
                                                         parentIndex,
                                                         analysis->loanCount),
                                                 analysis->loanCount) ||
                                         changed);
                }
            }
            if (ancestors[childIndex]) {
                return ZR_FALSE;
            }
        }
        pass++;
    } while (changed && pass <= analysis->loanCount);
    return (TZrBool)!changed;
}

static TZrBool loan_place_is_same_or_narrower(
        const SZrParserPlaceGraph *graph,
        TZrPlaceId childId,
        TZrPlaceId parentId) {
    const EZrParserPlaceOverlap overlap =
            ZrParser_PlaceGraph_Overlap(graph, childId, parentId);
    const SZrParserPlace *child;

    if (overlap == ZR_PARSER_PLACE_EQUAL ||
        overlap == ZR_PARSER_PLACE_UNKNOWN) {
        return ZR_TRUE;
    }
    if (overlap != ZR_PARSER_PLACE_OVERLAP) {
        return ZR_FALSE;
    }
    child = ZrParser_PlaceGraph_Get(graph, childId);
    while (child != ZR_NULL && child->parentId != ZR_PLACE_ID_INVALID) {
        if (child->parentId == parentId) {
            return ZR_TRUE;
        }
        child = ZrParser_PlaceGraph_Get(graph, child->parentId);
    }
    return ZR_FALSE;
}

static TZrBool loan_validate_reborrow_provenance(
        const SSemanticLoanAnalysis *analysis) {
    for (TZrSize instructionIndex = 0U;
         instructionIndex < analysis->instructionCount;
         instructionIndex++) {
        const SZrSemanticIrInstruction *instruction;
        const SZrSemanticIrLoanFact *childLoan;
        const TZrBool *directParents;
        TZrBool hasParent = ZR_FALSE;

        if (analysis->instructionBlockIds[instructionIndex] ==
            ZR_PARSER_CFG_INVALID_BLOCK_ID) {
            continue;
        }
        instruction = ZrParser_SemanticIr_InstructionAt(
                analysis->function, instructionIndex);
        if (instruction->opcode != ZR_SEMANTIC_IR_REBORROW ||
            instruction->loanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
            instruction->loanId > analysis->loanCount) {
            continue;
        }
        childLoan = ZrParser_SemanticIr_Loan(
                analysis->function, instruction->loanId);
        directParents = loan_const_row(
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
            hasParent = ZR_TRUE;
            parentLoan = ZrParser_SemanticIr_Loan(
                    analysis->function, (TZrLoanId)(parentIndex + 1U));
            if (parentLoan == ZR_NULL || childLoan == ZR_NULL ||
                !loan_place_is_same_or_narrower(
                        &analysis->function->places,
                        instruction->placeId,
                        parentLoan->sourcePlaceId) ||
                !loan_place_is_same_or_narrower(
                        &analysis->function->places,
                        childLoan->sourcePlaceId,
                        parentLoan->sourcePlaceId)) {
                return ZR_FALSE;
            }
        }
        if (!hasParent) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool loan_block_is_reachable(
        const SSemanticLoanAnalysis *analysis,
        TZrSize blockIndex) {
    const SZrSemanticBlockFlowFacts *facts =
            ZrParser_SemanticFlow_BlockFacts(
                    analysis->result, (TZrUInt32)blockIndex);
    return (TZrBool)(facts != ZR_NULL && facts->isReachable);
}

static void loan_map_reachable_instruction_blocks(
        SSemanticLoanAnalysis *analysis) {
    for (TZrSize index = 0U;
         index < analysis->instructionCount;
         index++) {
        analysis->instructionBlockIds[index] =
                ZR_PARSER_CFG_INVALID_BLOCK_ID;
    }
    for (TZrSize blockIndex = 0U;
         blockIndex < analysis->blockCount;
         blockIndex++) {
        const SZrParserCfgBlock *block;
        if (!loan_block_is_reachable(analysis, blockIndex)) {
            continue;
        }
        block = (const SZrParserCfgBlock *)ZrCore_Array_Get(
                (SZrArray *)&analysis->cfg->blocks, blockIndex);
        for (TZrSize offset = 0U;
             offset < block->instructionCount;
             offset++) {
            analysis->instructionBlockIds[
                    block->firstInstructionIndex + offset] =
                    (TZrUInt32)blockIndex;
        }
    }
}

static TZrBool loan_block_is_predecessor(
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

static TZrBool loan_propagate_reaching_place_values(
        SSemanticLoanAnalysis *analysis) {
    TZrSize placeStateWidth =
            analysis->trackedPlaceCount * analysis->loanCount;
    TZrSize factLimit = analysis->valueCount * analysis->loanCount +
                        analysis->blockCount * placeStateWidth + 1U;
    TZrBool *newIn = (TZrBool *)calloc(
            placeStateWidth > 0U ? placeStateWidth : 1U,
            sizeof(TZrBool));
    TZrBool *work = (TZrBool *)calloc(
            placeStateWidth > 0U ? placeStateWidth : 1U,
            sizeof(TZrBool));
    TZrBool *inputs = (TZrBool *)calloc(
            analysis->loanCount, sizeof(TZrBool));
    TZrBool changed;
    TZrSize pass = 0U;

    if (newIn == ZR_NULL || work == ZR_NULL || inputs == ZR_NULL) {
        free(newIn);
        free(work);
        free(inputs);
        return ZR_FALSE;
    }
    loan_seed_created_values(analysis);
    memset(
            analysis->parentLoanIds,
            0,
            analysis->loanCount * sizeof(TZrLoanId));
    do {
        changed = ZR_FALSE;
        for (TZrSize blockIndex = 0U;
             blockIndex < analysis->blockCount;
             blockIndex++) {
            const SZrParserCfgBlock *block =
                    (const SZrParserCfgBlock *)ZrCore_Array_Get(
                            (SZrArray *)&analysis->cfg->blocks, blockIndex);
            TZrBool *blockIn = analysis->blockPlaceIn +
                               blockIndex * placeStateWidth;
            TZrBool *blockOut = analysis->blockPlaceOut +
                                blockIndex * placeStateWidth;

            if (!loan_block_is_reachable(analysis, blockIndex)) {
                continue;
            }

            memset(newIn, 0, placeStateWidth * sizeof(TZrBool));
            for (TZrSize predecessorIndex = 0U;
                 predecessorIndex < analysis->blockCount;
                 predecessorIndex++) {
                const SZrParserCfgBlock *predecessor =
                        (const SZrParserCfgBlock *)ZrCore_Array_Get(
                                (SZrArray *)&analysis->cfg->blocks,
                                predecessorIndex);
                if (loan_block_is_reachable(
                            analysis, predecessorIndex) &&
                    loan_block_is_predecessor(predecessor, blockIndex)) {
                    (void)loan_set_union(
                            newIn,
                            analysis->blockPlaceOut +
                                    predecessorIndex * placeStateWidth,
                            placeStateWidth);
                }
            }
            if (!loan_set_equal(blockIn, newIn, placeStateWidth)) {
                memcpy(blockIn, newIn, placeStateWidth * sizeof(TZrBool));
                changed = ZR_TRUE;
            }
            memcpy(work, newIn, placeStateWidth * sizeof(TZrBool));
            for (TZrSize offset = 0U;
                 offset < block->instructionCount;
                 offset++) {
                TZrSize instructionIndex =
                        block->firstInstructionIndex + offset;
                const SZrSemanticIrInstruction *instruction =
                        ZrParser_SemanticIr_InstructionAt(
                                analysis->function, instructionIndex);
                TZrBool *resultLoans = ZR_NULL;
                TZrBool *currentPlaceLoans = ZR_NULL;
                TZrSize trackedPlaceIndex = 0U;

                memset(inputs, 0, analysis->loanCount * sizeof(TZrBool));
                loan_collect_instruction_inputs(analysis, instruction, inputs);
                loan_record_reborrow_parents(analysis, instruction, inputs);
                if (instruction->placeId != ZR_PLACE_ID_INVALID &&
                    instruction->placeId <= analysis->placeCount) {
                    trackedPlaceIndex = analysis->trackedPlaceIndices[
                            (TZrSize)instruction->placeId - 1U];
                    if (trackedPlaceIndex > 0U) {
                        currentPlaceLoans = work +
                                (trackedPlaceIndex - 1U) * analysis->loanCount;
                    }
                }
                if (instruction->resultValueId != ZR_VALUE_ID_INVALID &&
                    instruction->resultValueId <= analysis->valueCount) {
                    resultLoans = loan_row(
                            analysis->valueLoans,
                            (TZrSize)instruction->resultValueId - 1U,
                            analysis->loanCount);
                }
                if (currentPlaceLoans != ZR_NULL &&
                    loan_opcode_stores_value(instruction->opcode)) {
                    memcpy(
                            currentPlaceLoans,
                            inputs,
                            analysis->loanCount * sizeof(TZrBool));
                }
                if (resultLoans != ZR_NULL && currentPlaceLoans != ZR_NULL &&
                    loan_opcode_propagates_place_to_result(
                            instruction->opcode)) {
                    changed = (TZrBool)(loan_set_union(
                                                 resultLoans,
                                                 currentPlaceLoans,
                                                 analysis->loanCount) ||
                                         changed);
                }
                if (resultLoans != ZR_NULL &&
                    loan_opcode_propagates_input_to_result(
                            instruction->opcode)) {
                    changed = (TZrBool)(loan_set_union(
                                                 resultLoans,
                                                 inputs,
                                                 analysis->loanCount) ||
                                         changed);
                }
            }
            if (!loan_set_equal(blockOut, work, placeStateWidth)) {
                memcpy(blockOut, work, placeStateWidth * sizeof(TZrBool));
                changed = ZR_TRUE;
            }
        }
        pass++;
    } while (changed && pass < factLimit);

    free(newIn);
    free(work);
    free(inputs);
    return (TZrBool)!changed;
}

static void loan_add_parent_closure(
        const SSemanticLoanAnalysis *analysis,
        TZrBool *loans) {
    for (TZrSize index = 0U; index < analysis->loanCount; index++) {
        if (loans[index]) {
            (void)loan_set_union(
                    loans,
                    loan_const_row(
                            analysis->ancestorLoans,
                            index,
                            analysis->loanCount),
                    analysis->loanCount);
        }
    }
}

static void loan_build_instruction_uses(SSemanticLoanAnalysis *analysis) {
    for (TZrSize index = 0U; index < analysis->instructionCount; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(analysis->function, index);
        TZrBool *uses = loan_row(
                analysis->instructionUses, index, analysis->loanCount);
        if (analysis->instructionBlockIds[index] ==
            ZR_PARSER_CFG_INVALID_BLOCK_ID) {
            continue;
        }
        loan_collect_instruction_inputs(analysis, instruction, uses);
        if (instruction->opcode == ZR_SEMANTIC_IR_END_LOAN &&
            instruction->loanId != ZR_SEMANTIC_LOAN_ID_INVALID) {
            uses[(TZrSize)instruction->loanId - 1U] = ZR_TRUE;
        } else if (instruction->loanId != ZR_SEMANTIC_LOAN_ID_INVALID &&
                   instruction->opcode != ZR_SEMANTIC_IR_BORROW_SHARED &&
                   instruction->opcode != ZR_SEMANTIC_IR_BORROW_MUT &&
                   instruction->opcode != ZR_SEMANTIC_IR_REBORROW) {
            uses[(TZrSize)instruction->loanId - 1U] = ZR_TRUE;
        }
        loan_add_parent_closure(analysis, uses);
    }
}

static void loan_backward_transfer(
        const SSemanticLoanAnalysis *analysis,
        const SZrSemanticIrInstruction *instruction,
        const TZrBool *uses,
        TZrBool *live) {
    if ((instruction->opcode == ZR_SEMANTIC_IR_BORROW_SHARED ||
         instruction->opcode == ZR_SEMANTIC_IR_BORROW_MUT ||
         instruction->opcode == ZR_SEMANTIC_IR_REBORROW) &&
        instruction->loanId != ZR_SEMANTIC_LOAN_ID_INVALID) {
        live[(TZrSize)instruction->loanId - 1U] = ZR_FALSE;
    }
    (void)loan_set_union(live, uses, analysis->loanCount);
    loan_add_parent_closure(analysis, live);
}

static TZrBool loan_compute_liveness(SSemanticLoanAnalysis *analysis) {
    TZrBool *newOut = (TZrBool *)calloc(
            analysis->loanCount, sizeof(TZrBool));
    TZrBool *work = (TZrBool *)calloc(
            analysis->loanCount, sizeof(TZrBool));
    TZrBool changed;
    TZrSize pass = 0U;
    TZrSize passLimit = analysis->blockCount *
                        (analysis->loanCount + 1U) + 1U;

    if (newOut == ZR_NULL || work == ZR_NULL) {
        free(newOut);
        free(work);
        return ZR_FALSE;
    }
    do {
        changed = ZR_FALSE;
        for (TZrSize reverse = analysis->blockCount; reverse > 0U; reverse--) {
            TZrSize blockIndex = reverse - 1U;
            const SZrParserCfgBlock *block =
                    (const SZrParserCfgBlock *)ZrCore_Array_Get(
                            (SZrArray *)&analysis->cfg->blocks, blockIndex);
            TZrBool *blockIn = loan_row(
                    analysis->blockLiveIn,
                    blockIndex,
                    analysis->loanCount);
            TZrBool *blockOut = loan_row(
                    analysis->blockLiveOut,
                    blockIndex,
                    analysis->loanCount);
            if (!loan_block_is_reachable(analysis, blockIndex)) {
                continue;
            }
            memset(newOut, 0, analysis->loanCount * sizeof(TZrBool));
            for (TZrSize edgeIndex = 0U;
                 edgeIndex < block->successorCount;
                 edgeIndex++) {
                TZrUInt32 successorId =
                        ZrParser_Cfg_BlockSuccessorIdAt(block, edgeIndex);
                if (successorId < analysis->blockCount &&
                    loan_block_is_reachable(analysis, successorId)) {
                    (void)loan_set_union(
                            newOut,
                            loan_const_row(
                                    analysis->blockLiveIn,
                                    successorId,
                                    analysis->loanCount),
                            analysis->loanCount);
                }
            }
            memcpy(work, newOut, analysis->loanCount * sizeof(TZrBool));
            for (TZrSize offset = block->instructionCount; offset > 0U; offset--) {
                TZrSize instructionIndex =
                        block->firstInstructionIndex + offset - 1U;
                const SZrSemanticIrInstruction *instruction =
                        ZrParser_SemanticIr_InstructionAt(
                                analysis->function, instructionIndex);
                loan_backward_transfer(
                        analysis,
                        instruction,
                        loan_const_row(
                                analysis->instructionUses,
                                instructionIndex,
                                analysis->loanCount),
                        work);
            }
            if (!loan_set_equal(blockOut, newOut, analysis->loanCount)) {
                memcpy(blockOut, newOut, analysis->loanCount * sizeof(TZrBool));
                changed = ZR_TRUE;
            }
            if (!loan_set_equal(blockIn, work, analysis->loanCount)) {
                memcpy(blockIn, work, analysis->loanCount * sizeof(TZrBool));
                changed = ZR_TRUE;
            }
        }
        pass++;
    } while (changed && pass < passLimit);

    if (changed) {
        free(newOut);
        free(work);
        return ZR_FALSE;
    }

    for (TZrSize blockIndex = 0U;
         blockIndex < analysis->blockCount;
         blockIndex++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&analysis->cfg->blocks, blockIndex);
        if (!loan_block_is_reachable(analysis, blockIndex)) {
            continue;
        }
        memcpy(
                work,
                loan_const_row(
                        analysis->blockLiveOut,
                        blockIndex,
                        analysis->loanCount),
                analysis->loanCount * sizeof(TZrBool));
        for (TZrSize offset = block->instructionCount; offset > 0U; offset--) {
            TZrSize instructionIndex =
                    block->firstInstructionIndex + offset - 1U;
            const SZrSemanticIrInstruction *instruction =
                    ZrParser_SemanticIr_InstructionAt(
                            analysis->function, instructionIndex);
            memcpy(
                    loan_row(
                            analysis->instructionLiveOut,
                            instructionIndex,
                            analysis->loanCount),
                    work,
                    analysis->loanCount * sizeof(TZrBool));
            loan_backward_transfer(
                    analysis,
                    instruction,
                    loan_const_row(
                            analysis->instructionUses,
                            instructionIndex,
                            analysis->loanCount),
                    work);
            memcpy(
                    loan_row(
                            analysis->instructionLiveIn,
                            instructionIndex,
                            analysis->loanCount),
                    work,
                    analysis->loanCount * sizeof(TZrBool));
        }
    }
    free(newOut);
    free(work);
    return ZR_TRUE;
}

static void loan_borrow_state_clear(SZrSemanticBorrowState *borrowing) {
    borrowing->sharedLoanIds.length = 0U;
    borrowing->mutableLoanId = ZR_SEMANTIC_LOAN_ID_INVALID;
}

static TZrBool loan_borrow_state_contains(
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

static void loan_borrow_state_add(
        SZrState *state,
        SZrSemanticBorrowState *borrowing,
        const SZrSemanticIrLoanFact *loan) {
    if (loan->access == ZR_SEMANTIC_LOAN_SHARED) {
        if (!loan_borrow_state_contains(
                    &borrowing->sharedLoanIds, loan->loanId)) {
            TZrLoanId loanId = loan->loanId;
            ZrCore_Array_Push(state, &borrowing->sharedLoanIds, &loanId);
        }
    } else if (borrowing->mutableLoanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
               borrowing->mutableLoanId == loan->loanId) {
        borrowing->mutableLoanId = loan->loanId;
    } else {
        borrowing->mutableLoanId = ZR_SEMANTIC_LOAN_ID_MULTIPLE;
    }
}

static void loan_apply_set_to_place_states(
        const SSemanticLoanAnalysis *analysis,
        SZrArray *states,
        const TZrBool *liveLoans) {
    for (TZrSize placeIndex = 0U; placeIndex < states->length; placeIndex++) {
        SZrSemanticPlaceFlowState *placeState =
                (SZrSemanticPlaceFlowState *)ZrCore_Array_Get(
                        states, placeIndex);
        loan_borrow_state_clear(&placeState->borrowing);
    }
    for (TZrSize loanIndex = 0U;
         loanIndex < analysis->loanCount;
         loanIndex++) {
        const SZrSemanticIrLoanFact *loan;
        SZrSemanticPlaceFlowState *placeState;
        if (!liveLoans[loanIndex]) {
            continue;
        }
        loan = ZrParser_SemanticIr_Loan(
                analysis->function, (TZrLoanId)(loanIndex + 1U));
        if (loan == ZR_NULL || loan->sourcePlaceId == ZR_PLACE_ID_INVALID ||
            loan->sourcePlaceId > states->length) {
            continue;
        }
        placeState = (SZrSemanticPlaceFlowState *)ZrCore_Array_Get(
                states, (TZrSize)loan->sourcePlaceId - 1U);
        loan_borrow_state_add(analysis->state, &placeState->borrowing, loan);
    }
}

static void loan_apply_block_facts(const SSemanticLoanAnalysis *analysis) {
    for (TZrSize blockIndex = 0U;
         blockIndex < analysis->blockCount;
         blockIndex++) {
        SZrSemanticBlockFlowFacts *facts =
                (SZrSemanticBlockFlowFacts *)ZrCore_Array_Get(
                        &analysis->result->blockFacts, blockIndex);
        loan_apply_set_to_place_states(
                analysis,
                &facts->entryStates,
                loan_const_row(
                        analysis->blockLiveIn,
                        blockIndex,
                        analysis->loanCount));
        loan_apply_set_to_place_states(
                analysis,
                &facts->exitStates,
                loan_const_row(
                        analysis->blockLiveOut,
                        blockIndex,
                        analysis->loanCount));
    }
}

static void loan_analysis_free(SSemanticLoanAnalysis *analysis) {
    free(analysis->valueLoans);
    free(analysis->placeLoans);
    free(analysis->trackedPlaceIndices);
    free(analysis->blockPlaceIn);
    free(analysis->blockPlaceOut);
    free(analysis->instructionUses);
    free(analysis->instructionLiveIn);
    free(analysis->instructionLiveOut);
    free(analysis->blockLiveIn);
    free(analysis->blockLiveOut);
    free(analysis->directParentLoans);
    free(analysis->ancestorLoans);
    free(analysis->parentLoanIds);
    free(analysis->instructionBlockIds);
}

static TZrBool loan_analysis_allocate(SSemanticLoanAnalysis *analysis) {
    TZrSize valueCells = analysis->valueCount * analysis->loanCount;
    TZrSize placeCells = analysis->placeCount * analysis->loanCount;
    TZrSize instructionCells =
            analysis->instructionCount * analysis->loanCount;
    TZrSize blockCells = analysis->blockCount * analysis->loanCount;
    TZrSize parentCells = analysis->loanCount * analysis->loanCount;
    analysis->valueLoans = (TZrBool *)calloc(valueCells, sizeof(TZrBool));
    analysis->placeLoans = (TZrBool *)calloc(placeCells, sizeof(TZrBool));
    analysis->instructionUses =
            (TZrBool *)calloc(instructionCells, sizeof(TZrBool));
    analysis->instructionLiveIn =
            (TZrBool *)calloc(instructionCells, sizeof(TZrBool));
    analysis->instructionLiveOut =
            (TZrBool *)calloc(instructionCells, sizeof(TZrBool));
    analysis->blockLiveIn =
            (TZrBool *)calloc(blockCells, sizeof(TZrBool));
    analysis->blockLiveOut =
            (TZrBool *)calloc(blockCells, sizeof(TZrBool));
    analysis->directParentLoans =
            (TZrBool *)calloc(parentCells, sizeof(TZrBool));
    analysis->ancestorLoans =
            (TZrBool *)calloc(parentCells, sizeof(TZrBool));
    analysis->parentLoanIds =
            (TZrLoanId *)calloc(analysis->loanCount, sizeof(TZrLoanId));
    analysis->instructionBlockIds = (TZrUInt32 *)calloc(
            analysis->instructionCount, sizeof(TZrUInt32));
    return (TZrBool)(analysis->valueLoans != ZR_NULL &&
                     analysis->placeLoans != ZR_NULL &&
                     analysis->instructionUses != ZR_NULL &&
                     analysis->instructionLiveIn != ZR_NULL &&
                     analysis->instructionLiveOut != ZR_NULL &&
                     analysis->blockLiveIn != ZR_NULL &&
                     analysis->blockLiveOut != ZR_NULL &&
                     analysis->directParentLoans != ZR_NULL &&
                     analysis->ancestorLoans != ZR_NULL &&
                     analysis->parentLoanIds != ZR_NULL &&
                     analysis->instructionBlockIds != ZR_NULL);
}

TZrBool semantic_loan_liveness_analyze(
        SZrState *state,
        const SZrSemanticIrFunction *function,
        const SZrParserCfg *cfg,
        SZrSemanticFlowResult *result) {
    SSemanticLoanAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));
    analysis.state = state;
    analysis.function = function;
    analysis.cfg = cfg;
    analysis.result = result;
    analysis.loanCount = function->loanFacts.length;
    analysis.valueCount = function->values.length;
    analysis.placeCount = function->places.places.length;
    analysis.instructionCount = function->instructions.length;
    analysis.blockCount = cfg->blocks.length;
    result->loanCount = analysis.loanCount;
    if (analysis.loanCount == 0U || analysis.instructionCount == 0U) {
        return ZR_TRUE;
    }
    if (!loan_analysis_allocate(&analysis)) {
        loan_analysis_free(&analysis);
        return ZR_FALSE;
    }
    loan_map_reachable_instruction_blocks(&analysis);
    if (!loan_seed_and_propagate_values(&analysis) ||
        !loan_prepare_tracked_places(&analysis) ||
        !loan_propagate_reaching_place_values(&analysis) ||
        !loan_finalize_reborrow_graph(&analysis) ||
        !loan_validate_reborrow_provenance(&analysis)) {
        loan_analysis_free(&analysis);
        return ZR_FALSE;
    }
    loan_build_instruction_uses(&analysis);
    if (!loan_compute_liveness(&analysis) ||
        !semantic_loan_publish_liveness(&analysis)) {
        loan_analysis_free(&analysis);
        return ZR_FALSE;
    }
    loan_apply_block_facts(&analysis);
    semantic_loan_check_conflicts(&analysis);
    loan_analysis_free(&analysis);
    return ZR_TRUE;
}
