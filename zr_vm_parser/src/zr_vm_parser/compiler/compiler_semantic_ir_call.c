#include "compiler_internal.h"

TZrBool compiler_semantic_ir_lower_call(
        SZrCompilerState *cs,
        EZrSemanticIrOpcode opcode,
        TZrUInt32 callableSlot,
        TZrLoanId receiverLoanId,
        TZrUInt32 firstArgumentSlot,
        TZrUInt32 argumentCount,
        TZrUInt32 resultSlot,
        const SZrInferredType *resultType,
        TZrSymbolId symbolId,
        SZrAstNode *callNode,
        SZrFileRange sourceRange) {
    SZrArray operands;
    SZrSemanticIrInstructionSpec spec;
    const SZrSemanticIrLoanFact *loan = ZR_NULL;
    TZrTypeId resultTypeId;
    TZrValueId resultValueId;
    TZrValueId callableValueId;
    TZrUInt32 index;
    TZrBool emitted;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        !cs->preSemanticIrCfgActive ||
        resultSlot == ZR_PARSER_SLOT_NONE || argumentCount == UINT32_MAX ||
        (opcode != ZR_SEMANTIC_IR_CALL_TYPED &&
         opcode != ZR_SEMANTIC_IR_CALL_VIRTUAL &&
         opcode != ZR_SEMANTIC_IR_CALL_DYNAMIC &&
         opcode != ZR_SEMANTIC_IR_CALL_META)) {
        return ZR_FALSE;
    }
    if (resultType == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID) {
        return compiler_semantic_cfg_abandon(cs);
    }
    if (receiverLoanId != ZR_SEMANTIC_LOAN_ID_INVALID) {
        loan = ZrParser_SemanticIr_Loan(
                &cs->preSemanticIr, receiverLoanId);
    }
    callableValueId = loan != ZR_NULL
            ? loan->createdByValueId
            : compiler_semantic_ir_slot_value(cs, callableSlot);
    if (callableValueId == ZR_VALUE_ID_INVALID) {
        return compiler_semantic_cfg_abandon(cs);
    }

    ZrCore_Array_Init(
            cs->state,
            &operands,
            sizeof(TZrValueId),
            (TZrSize)argumentCount + 1U);
    ZrCore_Array_Push(cs->state, &operands, &callableValueId);
    for (index = 0U; index < argumentCount; index++) {
        TZrValueId argumentValueId = compiler_semantic_ir_slot_value(
                cs, firstArgumentSlot + index);
        if (argumentValueId == ZR_VALUE_ID_INVALID) {
            ZrCore_Array_Free(cs->state, &operands);
            return compiler_semantic_cfg_abandon(cs);
        }
        ZrCore_Array_Push(cs->state, &operands, &argumentValueId);
    }

    resultTypeId = ZrParser_Semantic_RegisterInferredType(
            cs->semanticContext,
            resultType,
            ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
            ZR_NULL,
            ZR_NULL);
    if (resultTypeId == ZR_SEMANTIC_ID_INVALID) {
        ZrCore_Array_Free(cs->state, &operands);
        return compiler_semantic_cfg_abandon(cs);
    }
    if (!compiler_semantic_cfg_begin_invoke(cs, callNode, sourceRange)) {
        ZrCore_Array_Free(cs->state, &operands);
        return ZR_FALSE;
    }
    resultValueId = ZrParser_SemanticIr_AddValue(
            &cs->preSemanticIr, resultTypeId, sourceRange);
    if (resultValueId == ZR_VALUE_ID_INVALID ||
        !compiler_semantic_ir_bind_result_value(
                cs,
                resultSlot,
                resultTypeId,
                resultValueId,
                sourceRange)) {
        ZrCore_Array_Free(cs->state, &operands);
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = opcode;
    spec.typeId = resultTypeId;
    spec.resultValueId = resultValueId;
    spec.symbolId = symbolId;
    spec.operands = (const TZrValueId *)operands.head;
    spec.operandCount = operands.length;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    emitted = compiler_semantic_ir_emit(cs, &spec);
    ZrCore_Array_Free(cs->state, &operands);
    return (TZrBool)(emitted && compiler_semantic_cfg_split_invoke(
            cs, callNode, sourceRange));
}
