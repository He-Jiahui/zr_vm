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

TZrBool ZrLanguageServer_SemanticAnalyzer_PublishCurrentCompilerQueryDiagnostic(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->compilerState == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrParser_Compiler_PublishCurrentDiagnostic(
            analyzer->compilerState);
}

void ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange location) {
    SZrCompilerState *compilerState;

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->compilerState == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    if (!compilerState->hasError &&
        !ZrParser_Compiler_ReportCannotInferExactType(
                compilerState,
                location)) {
        return;
    }
    ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(
            state,
            analyzer,
            location);
}

static SZrDiagnostic **query_diagnostic_find_reported_slot(
        SZrSemanticAnalyzer *analyzer,
        const SZrStructuredDiagnostic *structured) {
    if (analyzer == ZR_NULL || structured == ZR_NULL || !analyzer->diagnostics.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < analyzer->diagnostics.length; index++) {
        SZrDiagnostic **diagnosticPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, index);
        if (diagnosticPtr != ZR_NULL &&
            *diagnosticPtr != ZR_NULL &&
            query_diagnostic_same_range(&(*diagnosticPtr)->location, &structured->location) &&
            query_diagnostic_same_code((*diagnosticPtr)->code, structured->code)) {
            return diagnosticPtr;
        }
    }

    return ZR_NULL;
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
        ZrParser_SemanticFacts_ResolveControlFlowOwnership(analyzer->semanticContext,
                                                           analyzer->ast);
    } else {
        ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments(analyzer->semanticContext);
    }
    ZrParser_SemanticQueryScope_Module(&scope);
    if (!ZrParser_SemanticQuery_MaterializeDiagnostics(analyzer->semanticContext, &scope)) {
        return;
    }
    if (!ZrParser_SemanticQuery_Diagnostics(analyzer->semanticContext, &scope, &diagnostics)) {
        return;
    }

    for (TZrSize index = 0; index < diagnostics.count; index++) {
        const SZrStructuredDiagnostic *structured = &diagnostics.items[index];
        SZrDiagnostic *diagnostic;
        SZrDiagnostic **reportedSlot;

        reportedSlot = query_diagnostic_find_reported_slot(analyzer, structured);
        if (reportedSlot != ZR_NULL) {
            diagnostic = ZrLanguageServer_Diagnostic_FromStructured(state, structured);
            if (diagnostic != ZR_NULL) {
                ZrLanguageServer_Diagnostic_Free(state, *reportedSlot);
                *reportedSlot = diagnostic;
            }
            continue;
        }

        diagnostic = ZrLanguageServer_Diagnostic_FromStructured(state, structured);
        if (diagnostic != ZR_NULL) {
            ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);
        }
    }
}
