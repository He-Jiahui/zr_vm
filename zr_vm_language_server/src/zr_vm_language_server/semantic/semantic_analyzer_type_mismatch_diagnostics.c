#include "semantic/semantic_analyzer_type_mismatch_diagnostics.h"

static TZrBool semantic_type_mismatch_can_offer_numeric_cast(
        const SZrInferredType *expectedType,
        const SZrInferredType *actualType) {
    return expectedType != ZR_NULL &&
           actualType != ZR_NULL &&
           ZR_VALUE_IS_TYPE_NUMBER(expectedType->baseType) &&
           ZR_VALUE_IS_TYPE_NUMBER(actualType->baseType) &&
           expectedType->baseType != actualType->baseType;
}

static TZrBool semantic_type_mismatch_range_contains(
        SZrFileRange outer,
        SZrFileRange inner) {
    if (outer.start.offset > 0 && outer.end.offset > 0 &&
        inner.start.offset > 0 && inner.end.offset > 0) {
        return outer.start.offset <= inner.start.offset &&
               inner.end.offset <= outer.end.offset;
    }
    return (outer.start.line < inner.start.line ||
            (outer.start.line == inner.start.line &&
             outer.start.column <= inner.start.column)) &&
           (inner.end.line < outer.end.line ||
            (inner.end.line == outer.end.line &&
             inner.end.column <= outer.end.column));
}

static TZrBool semantic_type_mismatch_replace_covering_diagnostic(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrDiagnostic *replacement) {
    TZrSize index;

    if (state == ZR_NULL || analyzer == ZR_NULL || replacement == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < analyzer->diagnostics.length; index++) {
        SZrDiagnostic **existingPtr =
                (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, index);
        SZrDiagnostic *existing = existingPtr != ZR_NULL ? *existingPtr : ZR_NULL;
        const TZrChar *code = existing != ZR_NULL ? semantic_string_native(existing->code) : ZR_NULL;

        if (code != ZR_NULL &&
            strcmp(code, "type_mismatch") == 0 &&
            semantic_type_mismatch_range_contains(existing->location, replacement->location)) {
            ZrLanguageServer_Diagnostic_Free(state, existing);
            *existingPtr = replacement;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

SZrFileRange ZrLanguageServer_SemanticAnalyzer_AssignmentExpectedTypeLocation(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *leftExpression) {
    SZrSymbol *symbol;

    if (leftExpression == ZR_NULL) {
        return ZrParser_FileRange_Create(
                ZrParser_FilePosition_Create(0, 0, 0),
                ZrParser_FilePosition_Create(0, 0, 0),
                ZR_NULL);
    }
    if (analyzer == ZR_NULL ||
        leftExpression->type != ZR_AST_IDENTIFIER_LITERAL ||
        leftExpression->data.identifier.name == ZR_NULL) {
        return leftExpression->location;
    }

    symbol = ZrLanguageServer_SymbolTable_LookupAtPosition(
            analyzer->symbolTable,
            leftExpression->data.identifier.name,
            leftExpression->location);
    if (symbol == ZR_NULL) {
        return leftExpression->location;
    }
    if (symbol->astNode != ZR_NULL &&
        symbol->astNode->type == ZR_AST_VARIABLE_DECLARATION &&
        symbol->astNode->data.variableDeclaration.typeInfo != ZR_NULL &&
        symbol->astNode->data.variableDeclaration.typeInfo->name != ZR_NULL) {
        return symbol->astNode->data.variableDeclaration.typeInfo->name->location;
    }
    return symbol->selectionRange;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_ReportTypeMismatch(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange location,
        const SZrFileRange *expectedTypeLocation,
        const SZrInferredType *expectedType,
        const SZrInferredType *actualType) {
    TZrChar actualBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrChar expectedBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    const TZrChar *actualText;
    const TZrChar *conversionHint = ZR_NULL;
    const TZrChar *expectedText;
    SZrStructuredDiagnostic structured;
    SZrDiagnostic *diagnostic;

    if (state == ZR_NULL || analyzer == ZR_NULL || expectedType == ZR_NULL || actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    expectedText = ZrParser_TypeNameString_Get(
            state, expectedType, expectedBuffer, sizeof(expectedBuffer));
    actualText = ZrParser_TypeNameString_Get(
            state, actualType, actualBuffer, sizeof(actualBuffer));
    if (semantic_type_mismatch_can_offer_numeric_cast(expectedType, actualType)) {
        conversionHint = expectedText;
    }
    if (!ZrParser_DiagnosticBuilder_BuildTypeMismatchDetailed(
                state,
                &structured,
                location,
                expectedText != ZR_NULL ? expectedText : "unknown",
                actualText != ZR_NULL ? actualText : "unknown",
                expectedTypeLocation,
                conversionHint)) {
        return ZR_FALSE;
    }

    diagnostic = ZrLanguageServer_Diagnostic_FromStructured(state, &structured);
    ZrParser_StructuredDiagnostic_Free(state, &structured);
    if (diagnostic == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!semantic_type_mismatch_replace_covering_diagnostic(state, analyzer, diagnostic)) {
        ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);
    }
    if (analyzer->compilerState != ZR_NULL) {
        analyzer->compilerState->hasError = ZR_FALSE;
        ZrParser_Compiler_ClearStructuredError(analyzer->compilerState);
    }
    return ZR_TRUE;
}
