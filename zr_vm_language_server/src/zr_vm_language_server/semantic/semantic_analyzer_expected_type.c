#include "semantic/semantic_analyzer_expected_type.h"

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
