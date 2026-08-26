#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_parser/const_assignment.h"
#include "zr_vm_parser/semantic_query.h"

static TZrBool semantic_const_assignment_target_range(
        const SZrAstNode *assignment,
        SZrFileRange *outRange) {
    const SZrAstNode *left;

    if (assignment == ZR_NULL || outRange == ZR_NULL ||
        assignment->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        assignment->data.assignmentExpression.left == ZR_NULL) {
        return ZR_FALSE;
    }
    left = assignment->data.assignmentExpression.left;
    if (left->type == ZR_AST_IDENTIFIER_LITERAL) {
        *outRange = left->location;
        return ZR_TRUE;
    }
    if (left->type == ZR_AST_PRIMARY_EXPRESSION &&
        left->data.primaryExpression.members != ZR_NULL &&
        left->data.primaryExpression.members->count > 0U) {
        const SZrAstNode *lastMember =
                left->data.primaryExpression.members->nodes[
                        left->data.primaryExpression.members->count - 1U];
        if (lastMember != ZR_NULL &&
            lastMember->type == ZR_AST_MEMBER_EXPRESSION &&
            lastMember->data.memberExpression.property != ZR_NULL) {
            *outRange = lastMember->data.memberExpression.property->location;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static SZrSymbol *semantic_const_assignment_find_symbol(
        SZrSemanticAnalyzer *analyzer,
        TZrSymbolId symbolId) {
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }

    for (TZrSize scopeIndex = 0U;
         scopeIndex < analyzer->symbolTable->allScopes.length;
         scopeIndex++) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(
                &analyzer->symbolTable->allScopes,
                scopeIndex);
        SZrSymbolScope *scope = scopePtr != ZR_NULL ? *scopePtr : ZR_NULL;
        for (TZrSize symbolIndex = 0U;
             scope != ZR_NULL && symbolIndex < scope->symbols.length;
             symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(
                    &scope->symbols,
                    symbolIndex);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
                (*symbolPtr)->semanticId == symbolId) {
                return *symbolPtr;
            }
        }
    }
    return ZR_NULL;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_ProjectConstAssignment(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *assignment) {
    SZrFileRange targetRange;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticSymbolQuery query;
    SZrSymbol *symbol;
    SZrAstNode *targetDeclaration;
    SZrConstAssignmentResult result;
    SZrStructuredDiagnostic diagnostic;
    SZrSemanticDiagnosticFact fact;
    TZrBool appended;

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || analyzer->ast == ZR_NULL ||
        !semantic_const_assignment_target_range(assignment, &targetRange)) {
        return ZR_FALSE;
    }

    ZrParser_SemanticQueryScope_Module(&scope);
    memset(&query, 0, sizeof(query));
    symbol = ZR_NULL;
    if (ZrParser_SemanticQuery_SymbolAt(
            analyzer->semanticContext,
            targetRange,
            &scope,
            &query)) {
        symbol = semantic_const_assignment_find_symbol(analyzer, query.symbolId);
    }
    targetDeclaration = symbol != ZR_NULL ? symbol->astNode : ZR_NULL;
    if (!ZrParser_ConstAssignment_EvaluateContext(
                analyzer->compilerState,
                analyzer->ast,
                assignment,
                targetDeclaration,
                &result) ||
        !result.isConstTarget || !result.isViolation) {
        return ZR_FALSE;
    }

    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_ConstAssignment_BuildDiagnostic(
                state, &result, &diagnostic)) {
        return ZR_FALSE;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = assignment;
    fact.diagnostic = diagnostic;
    appended = ZrParser_SemanticFacts_AppendDiagnostic(
            analyzer->semanticContext, &fact);
    ZrParser_StructuredDiagnostic_Free(state, &diagnostic);
    return appended;
}
