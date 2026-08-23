#include "compiler_internal.h"

#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

static TZrBool compiler_diagnostic_range_is_empty(const SZrFileRange *range) {
    return (TZrBool)(range == ZR_NULL ||
                     (range->source == ZR_NULL &&
                      range->start.line == 0 &&
                      range->start.column == 0 &&
                      range->start.offset == 0U &&
                      range->end.line == 0 &&
                      range->end.column == 0 &&
                      range->end.offset == 0U));
}

static const SZrSemanticOwnershipFact *compiler_unique_ownership_violation(
        const SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->ownershipFacts.length; index++) {
        const SZrSemanticOwnershipFact *fact =
                (const SZrSemanticOwnershipFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->ownershipFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_ERROR &&
            fact->qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE &&
            fact->isViolation) {
            return fact;
        }
    }
    return ZR_NULL;
}

TZrBool ZrParser_Compiler_PublishCurrentDiagnostic(SZrCompilerState *cs) {
    SZrSemanticDiagnosticFact fact;
    SZrStructuredDiagnostic diagnostic;
    SZrFileRange location;
    TZrBool result;

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        cs->semanticContext == ZR_NULL || !cs->hasError ||
        cs->errorMessage == ZR_NULL) {
        return ZR_FALSE;
    }
    location = cs->errorLocation;
    memset(&fact, 0, sizeof(fact));
    fact.node = cs->currentAst;
    if (cs->hasStructuredError &&
        cs->structuredError.code != ZR_NULL &&
        cs->structuredError.message != ZR_NULL) {
        fact.diagnostic = cs->structuredError;
        if (compiler_diagnostic_range_is_empty(&fact.diagnostic.location)) {
            fact.diagnostic.location = location;
        }
        return ZrParser_SemanticFacts_AppendDiagnostic(
                cs->semanticContext, &fact);
    }

    if (!ZrParser_DiagnosticBuilder_Build(
                cs->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                location,
                "compiler_error",
                cs->errorMessage,
                ZR_NULL,
                ZR_NULL)) {
        return ZR_FALSE;
    }
    fact.diagnostic = diagnostic;
    result = ZrParser_SemanticFacts_AppendDiagnostic(
            cs->semanticContext, &fact);
    ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
    return result;
}

TZrBool ZrParser_Compiler_PublishSemanticQueryDiagnostics(SZrCompilerState *cs) {
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;
    const SZrSemanticOwnershipFact *ownershipViolation;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments(cs->semanticContext)) {
        return ZR_FALSE;
    }
    if (!ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(cs->semanticContext)) {
        return ZR_FALSE;
    }
    if (cs->scriptAst != ZR_NULL &&
        !ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(cs->semanticContext, cs->scriptAst)) {
        return ZR_FALSE;
    }
    if (cs->scriptAst != ZR_NULL &&
        !ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(cs->semanticContext, cs->scriptAst)) {
        return ZR_FALSE;
    }
    if (cs->scriptAst != ZR_NULL &&
        !ZrParser_SemanticFacts_ResolveControlFlowOwnership(cs->semanticContext, cs->scriptAst)) {
        return ZR_FALSE;
    }

    ownershipViolation = compiler_unique_ownership_violation(cs->semanticContext);
    if (ownershipViolation != ZR_NULL) {
        ZrParser_Compiler_Error(cs,
                               "Unique value is used after it was moved",
                               ownershipViolation->range);
        return ZR_FALSE;
    }

    ZrParser_SemanticQueryScope_Module(&scope);
    if (!ZrParser_SemanticQuery_MaterializeDiagnostics(cs->semanticContext, &scope)) {
        return ZR_FALSE;
    }
    return ZrParser_SemanticQuery_Diagnostics(cs->semanticContext, &scope, &diagnostics);
}
