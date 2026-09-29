#include "semantic_query_ownership_diagnostics.h"

/**
 * @brief 筛出可定位的 UNIQUE 移动后使用错误。
 * @note 普通所有权不兼容不等同于 move；必须具备 symbol、来源节点和 violation 标记。
 */
static TZrBool semantic_query_ownership_is_use_after_move(
        const SZrSemanticOwnershipFact *fact) {
    return fact != ZR_NULL &&
           fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_ERROR &&
           fact->qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE &&
           fact->symbolId != ZR_SEMANTIC_ID_INVALID &&
           fact->relatedNode != ZR_NULL &&
           fact->isViolation;
}

/**
 * @brief 构造 move 后使用诊断，并把导致失效的 move 节点作为关联位置返回。
 * @note 添加关联信息失败时释放尚未发布的诊断，避免部分结果进入查询数组。
 */
static TZrBool semantic_query_append_use_after_move_diagnostic(
        SZrSemanticContext *context,
        const SZrSemanticOwnershipFact *fact) {
    SZrStructuredDiagnostic diagnostic;

    if (!ZrParser_DiagnosticBuilder_BuildUseAfterMove(context->state,
                                                       &diagnostic,
                                                       fact->range)) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_AddRelatedInformation(
            context->state,
            &diagnostic,
            fact->relatedNode->location,
            "Value was moved here")) {
        ZrParser_StructuredDiagnostic_Free(context->state, &diagnostic);
        return ZR_FALSE;
    }

    ZrCore_Array_Push(context->state, &context->queryDiagnostics, &diagnostic);
    return ZR_TRUE;
}

/**
 * @brief 筛出借用/loan 生命周期超过 owner 的可报告错误事实。
 * @note 生命周期 ID 与 relatedNode 是构建 owner-release 关联说明所需的证据。
 */
static TZrBool semantic_query_ownership_is_borrow_after_release(
        const SZrSemanticOwnershipFact *fact) {
    return fact != ZR_NULL &&
           fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_ERROR &&
           (fact->qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
            fact->qualifier == ZR_OWNERSHIP_QUALIFIER_LOANED) &&
           fact->symbolId != ZR_SEMANTIC_ID_INVALID &&
           fact->lifetimeRegionId != ZR_SEMANTIC_ID_INVALID &&
           fact->ownerLifetimeRegionId != ZR_SEMANTIC_ID_INVALID &&
           fact->relatedNode != ZR_NULL &&
           fact->isViolation;
}

/**
 * @brief 按 BORROWED 或 LOANED 区分构建逃逸诊断，并引用 owner 的释放位置。
 */
static TZrBool semantic_query_append_borrow_after_release_diagnostic(
        SZrSemanticContext *context,
        const SZrSemanticOwnershipFact *fact) {
    SZrStructuredDiagnostic diagnostic;
    TZrBool built;

    if (fact->qualifier == ZR_OWNERSHIP_QUALIFIER_LOANED) {
        built = ZrParser_DiagnosticBuilder_BuildLoanEscape(context->state,
                                                           &diagnostic,
                                                           fact->range);
    } else {
        built = ZrParser_DiagnosticBuilder_BuildBorrowEscape(context->state,
                                                             &diagnostic,
                                                             fact->range);
    }
    if (!built) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_AddRelatedInformation(
            context->state,
            &diagnostic,
            fact->relatedNode->location,
            "Owner was released here")) {
        ZrParser_StructuredDiagnostic_Free(context->state, &diagnostic);
        return ZR_FALSE;
    }

    ZrCore_Array_Push(context->state, &context->queryDiagnostics, &diagnostic);
    return ZR_TRUE;
}

/**
 * @brief 筛出 owner 释放后仍被使用的 weak 引用错误事实。
 * @note weak 事实必须携带两个生命周期和释放节点，避免把一般 weak 访问误报为失效。
 */
static TZrBool semantic_query_ownership_is_weak_after_release(
        const SZrSemanticOwnershipFact *fact) {
    return fact != ZR_NULL &&
           fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_ERROR &&
           fact->qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK &&
           fact->symbolId != ZR_SEMANTIC_ID_INVALID &&
           fact->lifetimeRegionId != ZR_SEMANTIC_ID_INVALID &&
           fact->ownerLifetimeRegionId != ZR_SEMANTIC_ID_INVALID &&
           fact->relatedNode != ZR_NULL &&
           fact->isViolation;
}

/**
 * @brief 构造 weak 唤醒诊断，并将释放 owner 的位置作为关联信息。
 */
static TZrBool semantic_query_append_weak_after_release_diagnostic(
        SZrSemanticContext *context,
        const SZrSemanticOwnershipFact *fact) {
    SZrStructuredDiagnostic diagnostic;

    if (!ZrParser_DiagnosticBuilder_BuildWeakWake(context->state,
                                                   &diagnostic,
                                                   fact->range)) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_AddRelatedInformation(
            context->state,
            &diagnostic,
            fact->relatedNode->location,
            "Owner was released here")) {
        ZrParser_StructuredDiagnostic_Free(context->state, &diagnostic);
        return ZR_FALSE;
    }

    ZrCore_Array_Push(context->state, &context->queryDiagnostics, &diagnostic);
    return ZR_TRUE;
}

/**
 * @brief 按 ownership fact 的错误类别分派到查询诊断投影器。
 * @note fact 由物化器按 scope 过滤后传入；未覆盖的所有权错误刻意留给其他诊断路径。
 * @return 仅在对应诊断成功追加时返回 true；false 不代表整个查询物化失败。
 */
TZrBool ZrParser_SemanticQueryOwnership_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticOwnershipFact *fact) {
    if (context == ZR_NULL || fact == ZR_NULL || !context->queryDiagnostics.isValid) {
        return ZR_FALSE;
    }
    if (semantic_query_ownership_is_use_after_move(fact)) {
        return semantic_query_append_use_after_move_diagnostic(context, fact);
    }
    if (semantic_query_ownership_is_borrow_after_release(fact)) {
        return semantic_query_append_borrow_after_release_diagnostic(context, fact);
    }
    if (semantic_query_ownership_is_weak_after_release(fact)) {
        return semantic_query_append_weak_after_release_diagnostic(context, fact);
    }
    return ZR_FALSE;
}
