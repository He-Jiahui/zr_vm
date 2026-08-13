#include "semantic_query_ownership_diagnostics.h"

static TZrBool semantic_query_ownership_is_use_after_move(
        const SZrSemanticOwnershipFact *fact) {
    return fact != ZR_NULL &&
           fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_ERROR &&
           fact->qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE &&
           fact->symbolId != ZR_SEMANTIC_ID_INVALID &&
           fact->relatedNode != ZR_NULL &&
           fact->isViolation;
}

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
