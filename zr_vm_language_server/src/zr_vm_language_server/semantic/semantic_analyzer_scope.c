#include "semantic/semantic_analyzer_internal.h"

static TZrBool semantic_analysis_scope_contains_position(
        const SZrAstNode *node,
        const SZrFileRange *position) {
    return node != ZR_NULL && position != ZR_NULL &&
           position->start.offset >= node->location.start.offset &&
           position->start.offset <= node->location.end.offset;
}

static SZrAstNode *semantic_analysis_scope_find_in_nodes(
        SZrAstNodeArray *nodes,
        SZrFileRange position);

static TZrBool semantic_analysis_scope_is_root_type(EZrAstNodeType type) {
    switch (type) {
        case ZR_AST_FUNCTION_DECLARATION:
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_STRUCT_METHOD:
        case ZR_AST_STRUCT_META_FUNCTION:
        case ZR_AST_CLASS_META_FUNCTION:
        case ZR_AST_TEST_DECLARATION:
        case ZR_AST_LAMBDA_EXPRESSION:
        case ZR_AST_PROPERTY_GET:
        case ZR_AST_PROPERTY_SET:
            return ZR_TRUE;

        default:
            return ZR_FALSE;
    }
}

static SZrAstNode *semantic_analysis_scope_find(
        SZrAstNode *node,
        SZrFileRange position) {
    if (!semantic_analysis_scope_contains_position(node, &position)) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return semantic_analysis_scope_find_in_nodes(
                    node->data.script.statements,
                    position);

        case ZR_AST_CLASS_DECLARATION:
            return semantic_analysis_scope_find_in_nodes(
                    node->data.classDeclaration.members,
                    position);

        case ZR_AST_STRUCT_DECLARATION:
            return semantic_analysis_scope_find_in_nodes(
                    node->data.structDeclaration.members,
                    position);

        case ZR_AST_CLASS_PROPERTY:
            return semantic_analysis_scope_find(
                    node->data.classProperty.modifier,
                    position);

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return semantic_analysis_scope_find(
                    node->data.compileTimeDeclaration.declaration,
                    position);

        case ZR_AST_FUNCTION_DECLARATION:
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_STRUCT_METHOD:
        case ZR_AST_STRUCT_META_FUNCTION:
        case ZR_AST_CLASS_META_FUNCTION:
        case ZR_AST_TEST_DECLARATION:
        case ZR_AST_LAMBDA_EXPRESSION:
        case ZR_AST_PROPERTY_GET:
        case ZR_AST_PROPERTY_SET:
            return node;

        default:
            return ZR_NULL;
    }
}

static SZrAstNode *semantic_analysis_scope_find_in_nodes(
        SZrAstNodeArray *nodes,
        SZrFileRange position) {
    TZrSize index;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < nodes->count; index++) {
        SZrAstNode *scopeRoot =
                semantic_analysis_scope_find(nodes->nodes[index], position);
        if (scopeRoot != ZR_NULL) {
            return scopeRoot;
        }
    }
    return ZR_NULL;
}

SZrAstNode *ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
        SZrAstNode *ast,
        SZrFileRange position) {
    return semantic_analysis_scope_find(ast, position);
}

static TZrBool semantic_analysis_scope_nodes_contain_root(
        SZrAstNodeArray *nodes,
        const SZrAstNode *candidate);

static TZrBool semantic_analysis_scope_contains_root(
        SZrAstNode *node,
        const SZrAstNode *candidate) {
    if (node == ZR_NULL || candidate == ZR_NULL) {
        return ZR_FALSE;
    }
    if (node == candidate) {
        return semantic_analysis_scope_is_root_type(node->type);
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return semantic_analysis_scope_nodes_contain_root(
                    node->data.script.statements,
                    candidate);

        case ZR_AST_CLASS_DECLARATION:
            return semantic_analysis_scope_nodes_contain_root(
                    node->data.classDeclaration.members,
                    candidate);

        case ZR_AST_STRUCT_DECLARATION:
            return semantic_analysis_scope_nodes_contain_root(
                    node->data.structDeclaration.members,
                    candidate);

        case ZR_AST_CLASS_PROPERTY:
            return semantic_analysis_scope_contains_root(
                    node->data.classProperty.modifier,
                    candidate);

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return semantic_analysis_scope_contains_root(
                    node->data.compileTimeDeclaration.declaration,
                    candidate);

        default:
            return ZR_FALSE;
    }
}

static TZrBool semantic_analysis_scope_nodes_contain_root(
        SZrAstNodeArray *nodes,
        const SZrAstNode *candidate) {
    TZrSize index;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < nodes->count; index++) {
        if (semantic_analysis_scope_contains_root(nodes->nodes[index], candidate)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_IsAnalysisRoot(
        SZrAstNode *ast,
        const SZrAstNode *candidate) {
    return ast == candidate || semantic_analysis_scope_contains_root(ast, candidate);
}
