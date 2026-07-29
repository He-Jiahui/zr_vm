#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_language_server/incremental_parser.h"

static TZrBool semantic_change_range_is_within(
        const SZrFileRange *range,
        const SZrFileRange *container) {
    return range != ZR_NULL && container != ZR_NULL &&
           range->start.offset >= container->start.offset &&
           range->end.offset <= container->end.offset;
}

static TZrBool semantic_change_empty_range_touches_boundary(
        const SZrFileRange *range,
        const SZrFileRange *container) {
    return range != ZR_NULL && container != ZR_NULL &&
           range->start.offset == range->end.offset &&
           (range->start.offset <= container->start.offset ||
            range->start.offset >= container->end.offset);
}

static void semantic_change_reset_declaration(SZrFileChangeInfo *changeInfo) {
    SZrFilePosition origin = ZrParser_FilePosition_Create(0, 0, 0);

    changeInfo->hasDeclaration = ZR_FALSE;
    changeInfo->declarationType = (EZrAstNodeType)0;
    changeInfo->declarationRange = ZrParser_FileRange_Create(
            origin,
            origin,
            changeInfo->oldRange.source);
}

static SZrAstNode *semantic_change_scope_body(SZrAstNode *scopeRoot) {
    if (scopeRoot == ZR_NULL) {
        return ZR_NULL;
    }

    switch (scopeRoot->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return scopeRoot->data.functionDeclaration.body;
        case ZR_AST_CLASS_METHOD:
            return scopeRoot->data.classMethod.body;
        case ZR_AST_STRUCT_METHOD:
            return scopeRoot->data.structMethod.body;
        case ZR_AST_STRUCT_META_FUNCTION:
            return scopeRoot->data.structMetaFunction.body;
        case ZR_AST_CLASS_META_FUNCTION:
            return scopeRoot->data.classMetaFunction.body;
        case ZR_AST_LAMBDA_EXPRESSION:
            return scopeRoot->data.lambdaExpression.block;
        case ZR_AST_PROPERTY_GET:
            return scopeRoot->data.propertyGet.body;
        case ZR_AST_PROPERTY_SET:
            return scopeRoot->data.propertySet.body;
        default:
            return ZR_NULL;
    }
}

void ZrLanguageServer_SemanticAnalyzer_ClassifyFileChange(
        SZrAstNode *ast,
        SZrFileChangeInfo *changeInfo) {
    SZrAstNode *scopeRoot;
    SZrAstNode *body;

    if (changeInfo == ZR_NULL ||
        changeInfo->impact == ZR_FILE_CHANGE_IMPACT_NONE) {
        return;
    }

    changeInfo->impact = ZR_FILE_CHANGE_IMPACT_MODULE;
    semantic_change_reset_declaration(changeInfo);
    if (ast == ZR_NULL) {
        return;
    }

    scopeRoot = ZrLanguageServer_SemanticAnalyzer_FindAnalysisRootAtPosition(
            ast,
            changeInfo->oldRange);
    if (scopeRoot == ZR_NULL ||
        !semantic_change_range_is_within(
                &changeInfo->oldRange,
                &scopeRoot->location) ||
        semantic_change_empty_range_touches_boundary(
                &changeInfo->oldRange,
                &scopeRoot->location)) {
        return;
    }

    changeInfo->hasDeclaration = ZR_TRUE;
    changeInfo->declarationType = scopeRoot->type;
    changeInfo->declarationRange = scopeRoot->location;
    body = semantic_change_scope_body(scopeRoot);
    changeInfo->impact = body != ZR_NULL &&
                         semantic_change_range_is_within(
                                 &changeInfo->oldRange,
                                 &body->location) &&
                         !semantic_change_empty_range_touches_boundary(
                                 &changeInfo->oldRange,
                                 &body->location)
                             ? ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY
                             : ZR_FILE_CHANGE_IMPACT_DECLARATION_SIGNATURE;
}
