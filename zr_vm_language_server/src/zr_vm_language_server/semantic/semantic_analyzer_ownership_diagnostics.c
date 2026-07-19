#include "semantic/semantic_analyzer_ownership_diagnostics.h"

static SZrAstNode *semantic_ownership_callable_body(SZrAstNode *callable) {
    if (callable == ZR_NULL) {
        return ZR_NULL;
    }

    if (callable->type == ZR_AST_FUNCTION_DECLARATION) {
        return callable->data.functionDeclaration.body;
    }
    if (callable->type == ZR_AST_CLASS_METHOD) {
        return callable->data.classMethod.body;
    }

    return ZR_NULL;
}

static SZrFileRange semantic_ownership_point_at_range_end(SZrFileRange range) {
    range.start = range.end;
    return range;
}

static TZrBool semantic_ownership_source_lifetime_end(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *sourceNode,
        SZrAstNode *enclosingCallable,
        SZrFileRange *outRange) {
    SZrString *sourceName;
    SZrSymbol *sourceSymbol;
    SZrAstNode *body;

    if (analyzer == ZR_NULL || sourceNode == ZR_NULL || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceName = ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(analyzer->state, sourceNode);
    sourceSymbol = sourceName != ZR_NULL && analyzer->symbolTable != ZR_NULL
                   ? ZrLanguageServer_SymbolTable_LookupAtPosition(
                           analyzer->symbolTable,
                           sourceName,
                           sourceNode->location)
                   : ZR_NULL;
    if (sourceSymbol != ZR_NULL &&
        sourceSymbol->scope != ZR_NULL &&
        sourceSymbol->scope->range.end.line > 0) {
        *outRange = semantic_ownership_point_at_range_end(sourceSymbol->scope->range);
        return ZR_TRUE;
    }

    body = semantic_ownership_callable_body(enclosingCallable);
    if (body != ZR_NULL && body->location.end.line > 0) {
        *outRange = semantic_ownership_point_at_range_end(body->location);
        return ZR_TRUE;
    }
    if (enclosingCallable != ZR_NULL && enclosingCallable->location.end.line > 0) {
        *outRange = semantic_ownership_point_at_range_end(enclosingCallable->location);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool ZrLanguageServer_SemanticOwnership_AddEscapeRelatedInformation(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrStructuredDiagnostic *diagnostic,
        SZrAstNode *ownershipNode,
        SZrAstNode *enclosingCallable,
        EZrOwnershipQualifier qualifier) {
    SZrAstNode *sourceNode;
    SZrFileRange lifetimeEnd;
    const TZrChar *sourceMessage;

    if (state == ZR_NULL || analyzer == ZR_NULL || diagnostic == ZR_NULL ||
        ownershipNode == ZR_NULL || ownershipNode->type != ZR_AST_CONSTRUCT_EXPRESSION) {
        return ZR_FALSE;
    }

    sourceNode = ownershipNode->data.constructExpression.target;
    if (sourceNode == ZR_NULL ||
        !semantic_ownership_source_lifetime_end(analyzer, sourceNode, enclosingCallable, &lifetimeEnd)) {
        return ZR_FALSE;
    }

    sourceMessage = qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED
                    ? "Borrow source is here"
                    : "Loan source is here";
    return ZrParser_StructuredDiagnostic_AddRelatedInformation(
                   state,
                   diagnostic,
                   sourceNode->location,
                   sourceMessage) &&
           ZrParser_StructuredDiagnostic_AddRelatedInformation(
                   state,
                   diagnostic,
                   lifetimeEnd,
                   "Source lifetime ends here");
}
