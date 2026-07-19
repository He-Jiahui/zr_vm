#include "zr_vm_parser/semantic_ir.h"

#include <string.h>

static TZrBool semantic_ir_function_is_valid(
        const SZrSemanticIrFunction *function) {
    return (TZrBool)(function != ZR_NULL &&
                     function->state != ZR_NULL &&
                     function->instructions.isValid);
}

static TZrBool semantic_ir_place_is_valid(
        const SZrSemanticIrFunction *function,
        TZrPlaceId placeId) {
    return (TZrBool)(placeId != ZR_PLACE_ID_INVALID &&
                     ZrParser_PlaceGraph_Get(&function->places, placeId) != ZR_NULL);
}

static TZrBool semantic_ir_opcode_requires_place(EZrSemanticIrOpcode opcode) {
    switch (opcode) {
        case ZR_SEMANTIC_IR_PLACE_BASE:
        case ZR_SEMANTIC_IR_PLACE_PROJECT:
        case ZR_SEMANTIC_IR_LOAD:
        case ZR_SEMANTIC_IR_STORE:
        case ZR_SEMANTIC_IR_INITIALIZE:
        case ZR_SEMANTIC_IR_MOVE:
        case ZR_SEMANTIC_IR_COPY:
        case ZR_SEMANTIC_IR_DROP:
        case ZR_SEMANTIC_IR_BORROW_SHARED:
        case ZR_SEMANTIC_IR_BORROW_MUT:
        case ZR_SEMANTIC_IR_REBORROW:
        case ZR_SEMANTIC_IR_END_LOAN:
        case ZR_SEMANTIC_IR_DEREFERENCE:
        case ZR_SEMANTIC_IR_PROPERTY_GET:
        case ZR_SEMANTIC_IR_PROPERTY_SET:
        case ZR_SEMANTIC_IR_PROPERTY_REF_GET:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool semantic_ir_opcode_requires_input_value(EZrSemanticIrOpcode opcode) {
    return (TZrBool)(opcode == ZR_SEMANTIC_IR_STORE ||
                     opcode == ZR_SEMANTIC_IR_INITIALIZE ||
                     opcode == ZR_SEMANTIC_IR_PROPERTY_SET);
}

static TZrBool semantic_ir_opcode_requires_result(EZrSemanticIrOpcode opcode) {
    switch (opcode) {
        case ZR_SEMANTIC_IR_CONSTANT:
        case ZR_SEMANTIC_IR_CONVERT:
        case ZR_SEMANTIC_IR_LOAD:
        case ZR_SEMANTIC_IR_MOVE:
        case ZR_SEMANTIC_IR_COPY:
        case ZR_SEMANTIC_IR_BORROW_SHARED:
        case ZR_SEMANTIC_IR_BORROW_MUT:
        case ZR_SEMANTIC_IR_REBORROW:
        case ZR_SEMANTIC_IR_DEREFERENCE:
        case ZR_SEMANTIC_IR_VALUE_CONSTRUCT:
        case ZR_SEMANTIC_IR_AGGREGATE_CONSTRUCT:
        case ZR_SEMANTIC_IR_UNION_CONSTRUCT:
        case ZR_SEMANTIC_IR_GC_NEW:
        case ZR_SEMANTIC_IR_OWN_CONSTRUCT:
        case ZR_SEMANTIC_IR_PROPERTY_GET:
        case ZR_SEMANTIC_IR_PROPERTY_REF_GET:
        case ZR_SEMANTIC_IR_DESTRUCTURE_EVALUATE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool semantic_ir_cfg_is_valid(
        const SZrSemanticIrFunction *function) {
    TZrSize blockIndex;

    if (!function->cfg.blocks.isValid) {
        return ZR_FALSE;
    }
    if (function->cfg.blocks.length == 0U) {
        return ZR_TRUE;
    }
    if (function->cfg.entryBlockId >= function->cfg.blocks.length ||
        function->cfg.exitBlockId >= function->cfg.blocks.length) {
        return ZR_FALSE;
    }
    for (blockIndex = 0; blockIndex < function->cfg.blocks.length; blockIndex++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&function->cfg.blocks, blockIndex);
        TZrSize edgeIndex;

        if (block == ZR_NULL || block->id != blockIndex ||
            block->firstInstructionIndex > function->instructions.length ||
            block->instructionCount >
                    function->instructions.length - block->firstInstructionIndex ||
            block->terminatorKind < ZR_PARSER_CFG_TERMINATOR_NONE ||
            block->terminatorKind > ZR_PARSER_CFG_TERMINATOR_EXIT) {
            return ZR_FALSE;
        }
        for (edgeIndex = 0; edgeIndex < block->outgoingEdges.length; edgeIndex++) {
            const SZrParserCfgEdge *edge =
                    ZrParser_Cfg_BlockEdgeAt(block, edgeIndex);
            if (edge == ZR_NULL || edge->fromBlockId != block->id ||
                edge->toBlockId >= function->cfg.blocks.length ||
                edge->kind < ZR_PARSER_CFG_EDGE_NORMAL ||
                edge->kind >= ZR_PARSER_CFG_EDGE_ENUM_MAX) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

void ZrParser_SemanticIrFunction_Init(SZrState *state,
                                      SZrSemanticIrFunction *function,
                                      TZrSymbolId symbolId,
                                      TZrTypeId callableTypeId) {
    if (state == ZR_NULL || function == ZR_NULL) {
        return;
    }

    memset(function, 0, sizeof(*function));
    function->state = state;
    function->symbolId = symbolId;
    function->callableTypeId = callableTypeId;
    ZrParser_PlaceGraph_Init(state, &function->places);
    ZrParser_Cfg_Init(state, &function->cfg);
    ZrCore_Array_Init(state, &function->locals, sizeof(SZrSemanticIrLocal), 8U);
    ZrCore_Array_Init(state, &function->values, sizeof(SZrSemanticIrValue), 16U);
    ZrCore_Array_Init(state, &function->instructions, sizeof(SZrSemanticIrInstruction), 32U);
    ZrCore_Array_Init(state, &function->valueOperands, sizeof(TZrValueId), 32U);
    ZrCore_Array_Init(state, &function->regions, sizeof(SZrSemanticIrRegion), 4U);
    ZrCore_Array_Init(state, &function->cleanupScopes, sizeof(SZrSemanticIrCleanupScope), 4U);
    ZrCore_Array_Init(state, &function->sourceMap, sizeof(SZrSemanticIrSourceMapEntry), 32U);
    ZrCore_Array_Init(state, &function->loanFacts, sizeof(SZrSemanticIrLoanFact), 8U);
}

void ZrParser_SemanticIrFunction_Free(SZrState *state,
                                      SZrSemanticIrFunction *function) {
    if (state == ZR_NULL || function == ZR_NULL) {
        return;
    }

    ZrParser_PlaceGraph_Free(state, &function->places);
    ZrParser_Cfg_Free(state, &function->cfg);
    ZrCore_Array_Free(state, &function->locals);
    ZrCore_Array_Free(state, &function->values);
    ZrCore_Array_Free(state, &function->instructions);
    ZrCore_Array_Free(state, &function->valueOperands);
    ZrCore_Array_Free(state, &function->regions);
    ZrCore_Array_Free(state, &function->cleanupScopes);
    ZrCore_Array_Free(state, &function->sourceMap);
    ZrCore_Array_Free(state, &function->loanFacts);
    memset(function, 0, sizeof(*function));
}

TZrPlaceId ZrParser_SemanticIr_AddLocal(SZrSemanticIrFunction *function,
                                        TZrSymbolId symbolId,
                                        const SZrParserPlaceBase *base,
                                        TZrTypeId typeId,
                                        SZrFileRange sourceRange,
                                        TZrBool isParameter) {
    SZrSemanticIrLocal local;
    TZrPlaceId placeId;

    if (!semantic_ir_function_is_valid(function) || base == ZR_NULL) {
        return ZR_PLACE_ID_INVALID;
    }

    placeId = ZrParser_PlaceGraph_AddBase(
            &function->places, base, typeId, sourceRange);
    if (placeId == ZR_PLACE_ID_INVALID) {
        return placeId;
    }

    local.symbolId = symbolId;
    local.placeId = placeId;
    local.typeId = typeId;
    local.isParameter = isParameter;
    ZrCore_Array_Push(function->state, &function->locals, &local);
    return placeId;
}

TZrValueId ZrParser_SemanticIr_AddValue(SZrSemanticIrFunction *function,
                                        TZrTypeId typeId,
                                        SZrFileRange sourceRange) {
    SZrSemanticIrValue value;

    if (!semantic_ir_function_is_valid(function)) {
        return ZR_VALUE_ID_INVALID;
    }

    memset(&value, 0, sizeof(value));
    value.id = (TZrValueId)(function->values.length + 1U);
    value.typeId = typeId;
    value.sourceRange = sourceRange;
    ZrCore_Array_Push(function->state, &function->values, &value);
    return value.id;
}

TZrRegionId ZrParser_SemanticIr_AddRegion(SZrSemanticIrFunction *function,
                                          TZrRegionId parentId,
                                          EZrSemanticEscapeState escapeBound,
                                          SZrFileRange sourceRange) {
    SZrSemanticIrRegion region;

    if (!semantic_ir_function_is_valid(function) ||
        escapeBound < ZR_SEMANTIC_ESCAPE_LOCAL ||
        escapeBound > ZR_SEMANTIC_ESCAPE_UNKNOWN) {
        return ZR_SEMANTIC_REGION_ID_INVALID;
    }

    region.id = (TZrRegionId)(function->regions.length + 1U);
    region.parentId = parentId;
    region.escapeBound = escapeBound;
    region.sourceRange = sourceRange;
    ZrCore_Array_Push(function->state, &function->regions, &region);
    return region.id;
}

TZrCleanupScopeId ZrParser_SemanticIr_AddCleanupScope(
        SZrSemanticIrFunction *function,
        TZrCleanupScopeId parentId,
        SZrFileRange sourceRange) {
    SZrSemanticIrCleanupScope scope;

    if (!semantic_ir_function_is_valid(function)) {
        return ZR_SEMANTIC_CLEANUP_SCOPE_ID_INVALID;
    }

    memset(&scope, 0, sizeof(scope));
    scope.id = (TZrCleanupScopeId)(function->cleanupScopes.length + 1U);
    scope.parentId = parentId;
    scope.sourceRange = sourceRange;
    ZrCore_Array_Push(function->state, &function->cleanupScopes, &scope);
    return scope.id;
}

TZrLoanId ZrParser_SemanticIr_AddLoan(SZrSemanticIrFunction *function,
                                      TZrPlaceId sourcePlaceId,
                                      EZrSemanticLoanAccess access,
                                      TZrRegionId regionId,
                                      SZrFileRange originRange,
                                      SZrFileRange lastUseRange,
                                      TZrValueId createdByValueId) {
    SZrSemanticIrLoanFact loan;

    if (!semantic_ir_function_is_valid(function) ||
        ZrParser_PlaceGraph_Get(&function->places, sourcePlaceId) == ZR_NULL ||
        (access != ZR_SEMANTIC_LOAN_SHARED &&
         access != ZR_SEMANTIC_LOAN_MUTABLE)) {
        return ZR_SEMANTIC_LOAN_ID_INVALID;
    }

    loan.loanId = (TZrLoanId)(function->loanFacts.length + 1U);
    loan.sourcePlaceId = sourcePlaceId;
    loan.access = access;
    loan.regionId = regionId;
    loan.originRange = originRange;
    loan.lastUseRange = lastUseRange;
    loan.createdByValueId = createdByValueId;
    ZrCore_Array_Push(function->state, &function->loanFacts, &loan);
    return loan.loanId;
}

const SZrSemanticIrValue *ZrParser_SemanticIr_Value(
        const SZrSemanticIrFunction *function,
        TZrValueId valueId) {
    if (function == ZR_NULL || !function->values.isValid ||
        valueId == ZR_VALUE_ID_INVALID || valueId > function->values.length) {
        return ZR_NULL;
    }
    return (const SZrSemanticIrValue *)ZrCore_Array_Get(
            (SZrArray *)&function->values, (TZrSize)valueId - 1U);
}

const SZrSemanticIrLoanFact *ZrParser_SemanticIr_Loan(
        const SZrSemanticIrFunction *function,
        TZrLoanId loanId) {
    if (function == ZR_NULL || !function->loanFacts.isValid ||
        loanId == ZR_SEMANTIC_LOAN_ID_INVALID ||
        loanId > function->loanFacts.length) {
        return ZR_NULL;
    }
    return (const SZrSemanticIrLoanFact *)ZrCore_Array_Get(
            (SZrArray *)&function->loanFacts, (TZrSize)loanId - 1U);
}

TZrSemanticInstructionId ZrParser_SemanticIr_Emit(
        SZrSemanticIrFunction *function,
        const SZrSemanticIrInstructionSpec *spec) {
    SZrSemanticIrInstruction instruction;
    SZrSemanticIrSourceMapEntry sourceMapEntry;
    TZrSize index;

    if (!semantic_ir_function_is_valid(function) || spec == ZR_NULL ||
        spec->opcode <= ZR_SEMANTIC_IR_INVALID ||
        spec->opcode >= ZR_SEMANTIC_IR_ENUM_MAX ||
        (spec->placeId != ZR_PLACE_ID_INVALID &&
         ZrParser_PlaceGraph_Get(&function->places, spec->placeId) == ZR_NULL) ||
        (spec->valueId != ZR_VALUE_ID_INVALID &&
         ZrParser_SemanticIr_Value(function, spec->valueId) == ZR_NULL) ||
        (spec->resultValueId != ZR_VALUE_ID_INVALID &&
         ZrParser_SemanticIr_Value(function, spec->resultValueId) == ZR_NULL) ||
        (spec->auxiliaryValueId != ZR_VALUE_ID_INVALID &&
         ZrParser_SemanticIr_Value(function, spec->auxiliaryValueId) == ZR_NULL) ||
        (spec->operandCount > 0U && spec->operands == ZR_NULL)) {
        return ZR_SEMANTIC_INSTRUCTION_ID_INVALID;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.id =
            (TZrSemanticInstructionId)(function->instructions.length + 1U);
    instruction.opcode = spec->opcode;
    instruction.typeId = spec->typeId;
    instruction.placeId = spec->placeId;
    instruction.valueId = spec->valueId;
    instruction.resultValueId = spec->resultValueId;
    instruction.auxiliaryValueId = spec->auxiliaryValueId;
    instruction.symbolId = spec->symbolId;
    instruction.ownershipOperation = spec->ownershipOperation;
    instruction.targetBlockId = spec->targetBlockId;
    instruction.loanId = spec->loanId;
    instruction.regionId = spec->regionId;
    instruction.cleanupScopeId = spec->cleanupScopeId;
    instruction.escape = spec->escape;
    instruction.operandStart = (TZrUInt32)function->valueOperands.length;
    instruction.operandCount = (TZrUInt32)spec->operandCount;
    instruction.sourceRange = spec->sourceRange;
    for (index = 0; index < spec->operandCount; index++) {
        if (ZrParser_SemanticIr_Value(function, spec->operands[index]) == ZR_NULL) {
            return ZR_SEMANTIC_INSTRUCTION_ID_INVALID;
        }
    }
    for (index = 0; index < spec->operandCount; index++) {
        ZrCore_Array_Push(
                function->state,
                &function->valueOperands,
                (TZrPtr)&spec->operands[index]);
    }
    ZrCore_Array_Push(function->state, &function->instructions, &instruction);

    if (instruction.resultValueId != ZR_VALUE_ID_INVALID) {
        SZrSemanticIrValue *result = (SZrSemanticIrValue *)ZrCore_Array_Get(
                &function->values,
                (TZrSize)instruction.resultValueId - 1U);
        result->definitionInstructionId = instruction.id;
    }
    sourceMapEntry.instructionId = instruction.id;
    sourceMapEntry.sourceRange = instruction.sourceRange;
    ZrCore_Array_Push(function->state, &function->sourceMap, &sourceMapEntry);
    return instruction.id;
}

TZrBool ZrParser_SemanticIr_Validate(
        const SZrSemanticIrFunction *function) {
    TZrSize index;

    if (!semantic_ir_function_is_valid(function) ||
        !semantic_ir_cfg_is_valid(function) ||
        !function->sourceMap.isValid ||
        function->sourceMap.length != function->instructions.length) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->locals.length; index++) {
        const SZrSemanticIrLocal *local =
                (const SZrSemanticIrLocal *)ZrCore_Array_Get(
                        (SZrArray *)&function->locals, index);
        const SZrParserPlace *place =
                local != ZR_NULL
                        ? ZrParser_PlaceGraph_Get(&function->places, local->placeId)
                        : ZR_NULL;
        if (local == ZR_NULL || place == ZR_NULL ||
            place->typeId != local->typeId) {
            return ZR_FALSE;
        }
    }
    for (index = 0; index < function->values.length; index++) {
        const SZrSemanticIrValue *value =
                (const SZrSemanticIrValue *)ZrCore_Array_Get(
                        (SZrArray *)&function->values, index);
        if (value == ZR_NULL || value->id != (TZrValueId)(index + 1U) ||
            value->definitionInstructionId > function->instructions.length) {
            return ZR_FALSE;
        }
    }
    for (index = 0; index < function->regions.length; index++) {
        const SZrSemanticIrRegion *region =
                (const SZrSemanticIrRegion *)ZrCore_Array_Get(
                        (SZrArray *)&function->regions, index);
        if (region == ZR_NULL || region->id != (TZrRegionId)(index + 1U) ||
            (region->parentId != ZR_SEMANTIC_REGION_ID_INVALID &&
             region->parentId >= region->id) ||
            region->escapeBound < ZR_SEMANTIC_ESCAPE_LOCAL ||
            region->escapeBound > ZR_SEMANTIC_ESCAPE_UNKNOWN) {
            return ZR_FALSE;
        }
    }
    for (index = 0; index < function->cleanupScopes.length; index++) {
        const SZrSemanticIrCleanupScope *scope =
                (const SZrSemanticIrCleanupScope *)ZrCore_Array_Get(
                        (SZrArray *)&function->cleanupScopes, index);
        if (scope == ZR_NULL ||
            scope->id != (TZrCleanupScopeId)(index + 1U) ||
            (scope->parentId != ZR_SEMANTIC_CLEANUP_SCOPE_ID_INVALID &&
             scope->parentId >= scope->id) ||
            scope->firstInstructionIndex > function->instructions.length ||
            scope->instructionCount >
                    function->instructions.length - scope->firstInstructionIndex) {
            return ZR_FALSE;
        }
    }
    for (index = 0; index < function->loanFacts.length; index++) {
        const SZrSemanticIrLoanFact *loan =
                (const SZrSemanticIrLoanFact *)ZrCore_Array_Get(
                        (SZrArray *)&function->loanFacts, index);
        if (loan == ZR_NULL || loan->loanId != (TZrLoanId)(index + 1U) ||
            !semantic_ir_place_is_valid(function, loan->sourcePlaceId) ||
            (loan->access != ZR_SEMANTIC_LOAN_SHARED &&
             loan->access != ZR_SEMANTIC_LOAN_MUTABLE) ||
            loan->regionId == ZR_SEMANTIC_REGION_ID_INVALID ||
            loan->regionId > function->regions.length ||
            (loan->createdByValueId != ZR_VALUE_ID_INVALID &&
             ZrParser_SemanticIr_Value(function, loan->createdByValueId) == ZR_NULL)) {
            return ZR_FALSE;
        }
    }

    for (index = 0; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        const SZrSemanticIrSourceMapEntry *sourceMapEntry =
                (const SZrSemanticIrSourceMapEntry *)ZrCore_Array_Get(
                        (SZrArray *)&function->sourceMap, index);
        TZrSize operandIndex;

        if (instruction == ZR_NULL || sourceMapEntry == ZR_NULL ||
            instruction->id != (TZrSemanticInstructionId)(index + 1U) ||
            instruction->opcode <= ZR_SEMANTIC_IR_INVALID ||
             instruction->opcode >= ZR_SEMANTIC_IR_ENUM_MAX ||
            instruction->ownershipOperation < ZR_SEMANTIC_OWNERSHIP_NONE ||
            instruction->ownershipOperation >= ZR_SEMANTIC_OWNERSHIP_ENUM_MAX ||
            (instruction->opcode == ZR_SEMANTIC_IR_OWN_CONSTRUCT &&
             instruction->ownershipOperation == ZR_SEMANTIC_OWNERSHIP_NONE) ||
            (instruction->opcode != ZR_SEMANTIC_IR_OWN_CONSTRUCT &&
             instruction->ownershipOperation != ZR_SEMANTIC_OWNERSHIP_NONE) ||
            sourceMapEntry->instructionId != instruction->id ||
            (semantic_ir_opcode_requires_place(instruction->opcode) &&
             !semantic_ir_place_is_valid(function, instruction->placeId)) ||
            (instruction->placeId != ZR_PLACE_ID_INVALID &&
             !semantic_ir_place_is_valid(function, instruction->placeId)) ||
            (semantic_ir_opcode_requires_input_value(instruction->opcode) &&
             instruction->valueId == ZR_VALUE_ID_INVALID) ||
            (semantic_ir_opcode_requires_result(instruction->opcode) &&
             instruction->resultValueId == ZR_VALUE_ID_INVALID) ||
            (instruction->valueId != ZR_VALUE_ID_INVALID &&
             ZrParser_SemanticIr_Value(function, instruction->valueId) == ZR_NULL) ||
            (instruction->resultValueId != ZR_VALUE_ID_INVALID &&
             ZrParser_SemanticIr_Value(function, instruction->resultValueId) == ZR_NULL) ||
            (instruction->auxiliaryValueId != ZR_VALUE_ID_INVALID &&
             ZrParser_SemanticIr_Value(function, instruction->auxiliaryValueId) == ZR_NULL) ||
            instruction->operandStart > function->valueOperands.length ||
            instruction->operandCount >
                    function->valueOperands.length - instruction->operandStart ||
            (instruction->loanId != ZR_SEMANTIC_LOAN_ID_INVALID &&
             ZrParser_SemanticIr_Loan(function, instruction->loanId) == ZR_NULL) ||
            (instruction->regionId != ZR_SEMANTIC_REGION_ID_INVALID &&
             instruction->regionId > function->regions.length) ||
            (instruction->cleanupScopeId != ZR_SEMANTIC_CLEANUP_SCOPE_ID_INVALID &&
             instruction->cleanupScopeId > function->cleanupScopes.length) ||
            (instruction->targetBlockId != ZR_PARSER_CFG_INVALID_BLOCK_ID &&
             function->cfg.blocks.length > 0U &&
             instruction->targetBlockId >= function->cfg.blocks.length)) {
            return ZR_FALSE;
        }

        if ((instruction->opcode == ZR_SEMANTIC_IR_BORROW_SHARED ||
             instruction->opcode == ZR_SEMANTIC_IR_BORROW_MUT ||
             instruction->opcode == ZR_SEMANTIC_IR_REBORROW ||
             instruction->opcode == ZR_SEMANTIC_IR_END_LOAN) &&
            instruction->loanId == ZR_SEMANTIC_LOAN_ID_INVALID) {
            return ZR_FALSE;
        }

        if (instruction->loanId != ZR_SEMANTIC_LOAN_ID_INVALID) {
            const SZrSemanticIrLoanFact *loan =
                    ZrParser_SemanticIr_Loan(function, instruction->loanId);
            if (loan == ZR_NULL ||
                (instruction->placeId != ZR_PLACE_ID_INVALID &&
                 loan->sourcePlaceId != instruction->placeId) ||
                (instruction->regionId != ZR_SEMANTIC_REGION_ID_INVALID &&
                 loan->regionId != instruction->regionId) ||
                (instruction->opcode == ZR_SEMANTIC_IR_BORROW_SHARED &&
                 loan->access != ZR_SEMANTIC_LOAN_SHARED) ||
                (instruction->opcode == ZR_SEMANTIC_IR_BORROW_MUT &&
                 loan->access != ZR_SEMANTIC_LOAN_MUTABLE)) {
                return ZR_FALSE;
            }
        }

        for (operandIndex = 0; operandIndex < instruction->operandCount; operandIndex++) {
            const TZrValueId *operand = (const TZrValueId *)ZrCore_Array_Get(
                    (SZrArray *)&function->valueOperands,
                    instruction->operandStart + operandIndex);
            if (operand == ZR_NULL ||
                ZrParser_SemanticIr_Value(function, *operand) == ZR_NULL) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

const SZrSemanticIrInstruction *ZrParser_SemanticIr_InstructionAt(
        const SZrSemanticIrFunction *function,
        TZrSize index) {
    if (function == ZR_NULL || !function->instructions.isValid ||
        index >= function->instructions.length) {
        return ZR_NULL;
    }
    return (const SZrSemanticIrInstruction *)ZrCore_Array_Get(
            (SZrArray *)&function->instructions, index);
}

TZrBool ZrParser_SemanticIr_BindBlockRange(
        const SZrSemanticIrFunction *function,
        SZrParserCfg *cfg,
        TZrUInt32 blockId,
        TZrUInt32 firstInstructionIndex,
        TZrUInt32 instructionCount,
        EZrParserCfgTerminatorKind terminatorKind) {
    SZrParserCfgBlock *block;

    if (!semantic_ir_function_is_valid(function) || cfg == ZR_NULL ||
        blockId >= cfg->blocks.length ||
        firstInstructionIndex > function->instructions.length ||
        instructionCount > function->instructions.length - firstInstructionIndex ||
        terminatorKind < ZR_PARSER_CFG_TERMINATOR_NONE ||
        terminatorKind > ZR_PARSER_CFG_TERMINATOR_EXIT) {
        return ZR_FALSE;
    }

    block = (SZrParserCfgBlock *)ZrCore_Array_Get(&cfg->blocks, blockId);
    block->firstInstructionIndex = firstInstructionIndex;
    block->instructionCount = instructionCount;
    block->terminatorKind = terminatorKind;
    return ZR_TRUE;
}
