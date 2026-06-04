#include "semantic/semantic_analyzer_internal.h"

static TZrBool constant_condition_sources_match(SZrString *left, SZrString *right) {
    const TZrChar *leftText;
    const TZrChar *rightText;

    if (left == right) {
        return ZR_TRUE;
    }
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_TRUE;
    }

    leftText = semantic_string_native(left);
    rightText = semantic_string_native(right);
    return leftText != ZR_NULL && rightText != ZR_NULL && strcmp(leftText, rightText) == 0;
}

static int constant_condition_compare_position(SZrFilePosition left, SZrFilePosition right) {
    if (left.offset > 0 || right.offset > 0) {
        if (left.offset < right.offset) {
            return -1;
        }
        if (left.offset > right.offset) {
            return 1;
        }
        return 0;
    }

    if (left.line < right.line) {
        return -1;
    }
    if (left.line > right.line) {
        return 1;
    }
    if (left.column < right.column) {
        return -1;
    }
    if (left.column > right.column) {
        return 1;
    }
    return 0;
}

static TZrBool constant_condition_symbol_is_before_use(SZrSymbol *symbol, SZrAstNode *useNode) {
    SZrFileRange declarationRange;

    if (symbol == ZR_NULL || useNode == ZR_NULL) {
        return ZR_FALSE;
    }

    declarationRange = symbol->selectionRange;
    if (declarationRange.start.line == 0 &&
        declarationRange.start.column == 0 &&
        declarationRange.start.offset == 0) {
        declarationRange = symbol->location;
    }

    return constant_condition_sources_match(declarationRange.source, useNode->location.source) &&
           constant_condition_compare_position(declarationRange.start, useNode->location.start) <= 0;
}

static SZrString *constant_condition_identifier_name(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return node->data.identifier.name;
    }

    if (node->type == ZR_AST_PRIMARY_EXPRESSION &&
        node->data.primaryExpression.property != ZR_NULL &&
        node->data.primaryExpression.property->type == ZR_AST_IDENTIFIER_LITERAL &&
        (node->data.primaryExpression.members == ZR_NULL ||
         node->data.primaryExpression.members->count == 0)) {
        return node->data.primaryExpression.property->data.identifier.name;
    }

    return ZR_NULL;
}

static TZrBool constant_condition_boolean_literal(SZrAstNode *node, TZrBool *outValue) {
    if (node == ZR_NULL || node->type != ZR_AST_BOOLEAN_LITERAL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    *outValue = node->data.booleanLiteral.value;
    return ZR_TRUE;
}

static TZrBool constant_condition_try_variable_initializer(SZrSemanticAnalyzer *analyzer,
                                                           SZrAstNode *node,
                                                           TZrBool *outValue,
                                                           SZrAstNode **outEvidenceNode) {
    SZrString *name;
    SZrSymbol *symbol;
    SZrVariableDeclaration *declaration;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || node == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    name = constant_condition_identifier_name(node);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    symbol = ZrLanguageServer_SymbolTable_LookupAtPosition(analyzer->symbolTable, name, node->location);
    if (symbol == ZR_NULL ||
        symbol->type != ZR_SYMBOL_VARIABLE ||
        !symbol->isConst ||
        symbol->astNode == ZR_NULL ||
        symbol->astNode->type != ZR_AST_VARIABLE_DECLARATION ||
        !constant_condition_symbol_is_before_use(symbol, node)) {
        return ZR_FALSE;
    }

    declaration = &symbol->astNode->data.variableDeclaration;
    if (!constant_condition_boolean_literal(declaration->value, outValue)) {
        return ZR_FALSE;
    }

    if (outEvidenceNode != ZR_NULL) {
        *outEvidenceNode = declaration->value;
    }
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_TryEvaluateConstantBooleanCondition(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *node,
        TZrBool *outValue,
        SZrAstNode **outEvidenceNode) {
    if (outValue != ZR_NULL) {
        *outValue = ZR_FALSE;
    }
    if (outEvidenceNode != ZR_NULL) {
        *outEvidenceNode = ZR_NULL;
    }

    if (node == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (constant_condition_boolean_literal(node, outValue)) {
        if (outEvidenceNode != ZR_NULL) {
            *outEvidenceNode = node;
        }
        return ZR_TRUE;
    }

    return constant_condition_try_variable_initializer(analyzer, node, outValue, outEvidenceNode);
}
