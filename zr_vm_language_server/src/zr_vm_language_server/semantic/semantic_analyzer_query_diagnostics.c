#include "semantic_analyzer_internal.h"

#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

static TZrBool query_diagnostic_same_source(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL || left == right) {
        return ZR_TRUE;
    }

    return ZrCore_String_Equal(left, right);
}

static TZrBool query_diagnostic_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

static TZrBool query_diagnostic_same_range(const SZrFileRange *left, const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL || !query_diagnostic_same_source(left->source, right->source)) {
        return ZR_FALSE;
    }

    if (query_diagnostic_has_offset(&left->start) ||
        query_diagnostic_has_offset(&left->end) ||
        query_diagnostic_has_offset(&right->start) ||
        query_diagnostic_has_offset(&right->end)) {
        return left->start.offset == right->start.offset &&
               left->end.offset == right->end.offset;
    }

    return left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

static TZrBool query_diagnostic_same_code(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return left == right;
    }

    return ZrCore_String_Equal(left, right);
}

static TZrBool query_diagnostic_range_already_reported(SZrSemanticAnalyzer *analyzer,
                                                       const SZrStructuredDiagnostic *structured) {
    if (analyzer == ZR_NULL || structured == ZR_NULL || !analyzer->diagnostics.isValid) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < analyzer->diagnostics.length; index++) {
        SZrDiagnostic **diagnosticPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, index);
        if (diagnosticPtr != ZR_NULL &&
            *diagnosticPtr != ZR_NULL &&
            query_diagnostic_same_range(&(*diagnosticPtr)->location, &structured->location) &&
            query_diagnostic_same_code((*diagnosticPtr)->code, structured->code)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

void ZrLanguageServer_SemanticAnalyzer_AppendSemanticQueryDiagnostics(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return;
    }

    if (analyzer->ast != ZR_NULL) {
        ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(analyzer->semanticContext,
                                                                     analyzer->ast);
    } else {
        ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments(analyzer->semanticContext);
    }
    ZrParser_SemanticQueryScope_Module(&scope);
    if (!ZrParser_SemanticQuery_Diagnostics(analyzer->semanticContext, &scope, &diagnostics)) {
        return;
    }

    for (TZrSize index = 0; index < diagnostics.count; index++) {
        const SZrStructuredDiagnostic *structured = &diagnostics.items[index];
        SZrDiagnostic *diagnostic;

        if (query_diagnostic_range_already_reported(analyzer, structured)) {
            continue;
        }

        diagnostic = ZrLanguageServer_Diagnostic_FromStructured(state, structured);
        if (diagnostic != ZR_NULL) {
            ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);
        }
    }
}
