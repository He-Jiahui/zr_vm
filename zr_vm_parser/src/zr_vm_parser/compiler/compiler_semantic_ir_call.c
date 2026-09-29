#include "compiler_internal.h"

/*
 * 对已选择但缺少可用 CFG 输入的调用放弃本轮语义图，让调用方继续使用既有字节码路径。
 * CFG 未启动时无需回滚；回滚失败则必须向上返回失败，不能把残缺图当作成功。
 */
static TZrBool compiler_semantic_ir_call_fallback(SZrCompilerState *cs) {
    return cs != ZR_NULL && cs->preSemanticIrCfgActive
                   ? compiler_semantic_cfg_abandon(cs)
                   : ZR_TRUE;
}

/**
 * @brief 把已解析调用记录为带异常边界的 Semantic IR 调用，供后续数据流与 ExecIR 消费。
 * @pre 调用点所属的 Semantic IR 已初始化；slot、符号、结果类型和 opcode 由表达式编译器按解析结果提供。
 * @note receiverLoanId 有效时以 loan 创建值代表 callable；否则从 callableSlot 取值。参数区不含已作为
 *       receiver callable 的首个隐式参数。缺少可表达事实时放弃活动 CFG 并返回 true，保留旧编译路径；
 *       不可恢复的图构造/发射失败返回 false，由表达式调用方报错并终止当前编译。
 * @return 调用已记录、无需记录或已成功回退时返回 true；无效接口参数、回退失败或构造失败时返回 false。
 */
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
        resultSlot == ZR_PARSER_SLOT_NONE || argumentCount == UINT32_MAX ||
        (opcode != ZR_SEMANTIC_IR_CALL_TYPED &&
         opcode != ZR_SEMANTIC_IR_CALL_VIRTUAL &&
         opcode != ZR_SEMANTIC_IR_CALL_DYNAMIC &&
         opcode != ZR_SEMANTIC_IR_CALL_META)) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgTerminated) {
        return ZR_TRUE;
    }
    if (!cs->preSemanticIrCfgActive &&
        (cs->preSemanticIrCfgStartupSuppressed ||
         cs->preSemanticIrCfgStartupBlocked)) {
        return ZR_TRUE;
    }
    if (resultType == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID) {
        return compiler_semantic_ir_call_fallback(cs);
    }
    if (receiverLoanId != ZR_SEMANTIC_LOAN_ID_INVALID) {
        loan = ZrParser_SemanticIr_Loan(
                &cs->preSemanticIr, receiverLoanId);
    }
    callableValueId = loan != ZR_NULL
            ? loan->createdByValueId
            : compiler_semantic_ir_slot_value(cs, callableSlot);
    if (callableValueId == ZR_VALUE_ID_INVALID) {
        return compiler_semantic_ir_call_fallback(cs);
    }

    ZrCore_Array_Init(
            cs->state,
            &operands,
            sizeof(TZrValueId),
            (TZrSize)argumentCount + 1U);
    /* BUG: Array_Init 不返回分配状态；内存耗尽时 operands.head 仍为 NULL，首个 Push 在调试构建触发断言、
     * 在禁用断言时写空指针，无法以 false/CFG fallback 报告资源不足。证据见 array.h:36-42,73-88。 */
    ZrCore_Array_Push(cs->state, &operands, &callableValueId);
    for (index = 0U; index < argumentCount; index++) {
        TZrValueId argumentValueId = compiler_semantic_ir_slot_value(
                cs, firstArgumentSlot + index);
        if (argumentValueId == ZR_VALUE_ID_INVALID) {
            ZrCore_Array_Free(cs->state, &operands);
            return compiler_semantic_ir_call_fallback(cs);
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
        return compiler_semantic_ir_call_fallback(cs);
    }
    /* 调用指令必须独占 invoke block，随后才能把正常返回与异常传播接到不同 CFG 后继。 */
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
