#include "semantic_query_unresolved_diagnostics.h"

#include "zr_vm_parser/diagnostic_builder.h"

#include <stdio.h>

static TZrBool semantic_query_unresolved_same_source(SZrString *left,
                                                      SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }
    return left == right || ZrCore_String_Equal(left, right);
}

static TZrBool semantic_query_unresolved_has_offset(
        const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0U;
}

static TZrBool semantic_query_unresolved_ranges_equal(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !semantic_query_unresolved_same_source(left->source, right->source)) {
        return ZR_FALSE;
    }
    if (semantic_query_unresolved_has_offset(&left->start) ||
        semantic_query_unresolved_has_offset(&left->end) ||
        semantic_query_unresolved_has_offset(&right->start) ||
        semantic_query_unresolved_has_offset(&right->end)) {
        return left->start.offset == right->start.offset &&
               left->end.offset == right->end.offset;
    }
    return left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

static TZrBool semantic_query_unresolved_names_equal(
        SZrString *left,
        SZrString *right) {
    return left != ZR_NULL && right != ZR_NULL &&
           (left == right || ZrCore_String_Equal(left, right));
}

static TZrBool semantic_query_unresolved_kind_is_reportable(
        EZrSemanticReferenceKind kind) {
    return kind == ZR_SEMANTIC_REFERENCE_READ ||
           kind == ZR_SEMANTIC_REFERENCE_WRITE ||
           kind == ZR_SEMANTIC_REFERENCE_CALL ||
           kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
           kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE ||
           kind == ZR_SEMANTIC_REFERENCE_TYPE;
}

static TZrBool semantic_query_unresolved_kind_is_member(
        EZrSemanticReferenceKind kind) {
    return kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
           kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE;
}

static TZrBool semantic_query_unresolved_has_canonical_target(
        const SZrSemanticReferenceFact *fact) {
    return fact != ZR_NULL &&
           (fact->symbolId != ZR_SEMANTIC_ID_INVALID ||
            fact->typeId != ZR_SEMANTIC_ID_INVALID ||
            fact->signatureDisplay != ZR_NULL ||
            fact->contractRole != 0U);
}

static TZrBool semantic_query_unresolved_is_effective(
        const SZrSemanticContext *context,
        const SZrSemanticReferenceFact *fact) {
    TZrSize index;

    if (context == ZR_NULL || fact == ZR_NULL || fact->isResolved ||
        fact->name == ZR_NULL ||
        semantic_query_unresolved_has_canonical_target(fact) ||
        !semantic_query_unresolved_kind_is_reportable(fact->kind) ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *candidate =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        if (candidate != ZR_NULL &&
            candidate->isResolved &&
            semantic_query_unresolved_kind_is_reportable(candidate->kind) &&
            semantic_query_unresolved_names_equal(candidate->name, fact->name) &&
            semantic_query_unresolved_ranges_equal(
                    &candidate->range, &fact->range)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_SemanticQueryUnresolved_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticReferenceFact *fact) {
    const TZrChar *code;
    const TZrChar *name;
    const TZrChar *cause;
    const TZrChar *suggestion;
    TZrChar message[256];
    SZrStructuredDiagnostic diagnostic;

    if (!semantic_query_unresolved_is_effective(context, fact) ||
        !context->queryDiagnostics.isValid) {
        return ZR_FALSE;
    }

    name = fact->name != ZR_NULL
                   ? ZrCore_String_GetNativeString(fact->name)
                   : "reference";
    if (semantic_query_unresolved_kind_is_member(fact->kind)) {
        code = "member_not_found";
        cause = "Canonical member binding found no field, property, or method for this reference.";
        suggestion = "Declare the member or use a member exposed by the receiver's canonical type.";
        (void)snprintf(message,
                       sizeof(message),
                       "Member '%s' could not be resolved",
                       name);
    } else {
        code = "unresolved_reference";
        cause = "Canonical semantic binding found no declaration for this reference.";
        suggestion = "Declare or import the referenced symbol and verify that it is visible in this scope.";
        (void)snprintf(message,
                       sizeof(message),
                       "Reference '%s' could not be resolved",
                       name);
    }

    if (!ZrParser_DiagnosticBuilder_Build(
                context->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                fact->range,
                code,
                message,
                cause,
                suggestion)) {
        return ZR_FALSE;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(context->state, &diagnostic);
        return ZR_FALSE;
    }
    ZrCore_Array_Push(context->state, &context->queryDiagnostics, &diagnostic);
    return ZR_TRUE;
}
