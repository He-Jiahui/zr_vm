#include "semantic/lsp_semantic_import_chain.h"

#include "interface/lsp_interface_internal.h"

#include "zr_vm_library/file.h"

#include <ctype.h>
#include <string.h>

static TZrBool semantic_import_chain_uri_to_native_path(SZrString *uri, TZrChar *buffer, TZrSize bufferSize) {
    return ZrLanguageServer_Lsp_FileUriToNativePath(uri, buffer, bufferSize);
}

static TZrBool semantic_import_chain_try_get_analyzer_for_uri(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              SZrSemanticAnalyzer **outAnalyzer) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    TZrNativeString sourceBuffer = ZR_NULL;
    TZrSize sourceLength = 0;
    TZrBool loadedFromDisk = ZR_FALSE;
    TZrChar nativePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (outAnalyzer != ZR_NULL) {
        *outAnalyzer = ZR_NULL;
    }
    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || outAnalyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        *outAnalyzer = analyzer;
        return ZR_TRUE;
    }

    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        sourceBuffer = fileVersion->content;
        sourceLength = fileVersion->contentLength;
    } else if (state->global != ZR_NULL &&
               semantic_import_chain_uri_to_native_path(uri, nativePath, sizeof(nativePath))) {
        sourceBuffer = ZrLibrary_File_ReadAll(state->global, nativePath);
        sourceLength = sourceBuffer != ZR_NULL ? strlen(sourceBuffer) : 0;
        loadedFromDisk = sourceBuffer != ZR_NULL;
    }

    if (sourceBuffer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_Lsp_UpdateDocumentCore(state,
                                                 context,
                                                 uri,
                                                 sourceBuffer,
                                                 sourceLength,
                                                 fileVersion != ZR_NULL ? fileVersion->version : 0,
                                                 ZR_FALSE)) {
        if (loadedFromDisk) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          sourceBuffer,
                                          sourceLength + 1,
                                          ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        }
        return ZR_FALSE;
    }

    if (loadedFromDisk) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      sourceBuffer,
                                      sourceLength + 1,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer != ZR_NULL && analyzer->ast != ZR_NULL) {
        *outAnalyzer = analyzer;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool semantic_import_chain_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (!ZrLanguageServer_Lsp_StringsEqual(range.source, position.source)) {
        return ZR_FALSE;
    }

    if (range.start.offset > 0 && range.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset && position.end.offset <= range.end.offset;
    }

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line && range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line && position.end.column <= range.end.column));
}

static SZrString *semantic_import_chain_create_const_string(SZrState *state, const TZrChar *text) {
    return state != ZR_NULL && text != ZR_NULL
               ? ZrCore_String_Create(state, (TZrNativeString)text, strlen(text))
               : ZR_NULL;
}

static SZrString *semantic_import_chain_next_module_name(SZrState *state,
                                                         const SZrLspResolvedMetadataMember *resolvedMember) {
    if (state == ZR_NULL || resolvedMember == ZR_NULL ||
        resolvedMember->memberKind != ZR_LSP_METADATA_MEMBER_MODULE) {
        return ZR_NULL;
    }

    if (resolvedMember->moduleLinkDescriptor != ZR_NULL &&
        resolvedMember->moduleLinkDescriptor->moduleName != ZR_NULL) {
        return semantic_import_chain_create_const_string(state, resolvedMember->moduleLinkDescriptor->moduleName);
    }

    return resolvedMember->resolvedTypeText;
}

static TZrBool semantic_import_chain_resolve_linked_member_internal(
    SZrState *state,
    SZrLspMetadataProvider *provider,
    SZrSemanticAnalyzer *analyzer,
    SZrLspProjectIndex *projectIndex,
    SZrString *moduleName,
    SZrString *memberName,
    SZrLspResolvedMetadataMember *outResolvedMember,
    SZrString **outNextModuleName) {
    if (outResolvedMember != ZR_NULL) {
        memset(outResolvedMember, 0, sizeof(*outResolvedMember));
    }
    if (outNextModuleName != ZR_NULL) {
        *outNextModuleName = ZR_NULL;
    }
    if (state == ZR_NULL || provider == ZR_NULL || analyzer == ZR_NULL || moduleName == ZR_NULL ||
        memberName == ZR_NULL || outResolvedMember == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_LspMetadataProvider_ResolveImportedMember(provider,
                                                                    analyzer,
                                                                    projectIndex,
                                                                    moduleName,
                                                                    memberName,
                                                                    outResolvedMember)) {
        return ZR_FALSE;
    }

    if (outNextModuleName != ZR_NULL) {
        *outNextModuleName = semantic_import_chain_next_module_name(state, outResolvedMember);
    }
    return ZR_TRUE;
}

static TZrBool semantic_import_chain_append_location(SZrState *state,
                                                     SZrArray *result,
                                                     SZrString *uri,
                                                     SZrFileRange range) {
    SZrLspLocation *location;

    if (state == ZR_NULL || result == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }

    location->uri = uri;
    location->range = ZrLanguageServer_LspRange_FromFileRange(range);
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

static TZrBool semantic_import_chain_resolve_primary_expression(SZrState *state,
                                                                SZrLspMetadataProvider *provider,
                                                                SZrSemanticAnalyzer *analyzer,
                                                                SZrLspProjectIndex *projectIndex,
                                                                SZrArray *bindings,
                                                                SZrAstNode *node,
                                                                SZrFileRange queryRange,
                                                                SZrLspSemanticImportChainHit *outHit) {
    SZrAstNode *receiverNode;
    SZrLspImportBinding *binding;
    SZrString *currentModuleName;

    if (outHit != ZR_NULL) {
        memset(outHit, 0, sizeof(*outHit));
    }
    if (state == ZR_NULL || provider == ZR_NULL || analyzer == ZR_NULL || bindings == ZR_NULL ||
        node == ZR_NULL || outHit == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    receiverNode = node->data.primaryExpression.property;
    if (receiverNode == ZR_NULL || receiverNode->type != ZR_AST_IDENTIFIER_LITERAL ||
        node->data.primaryExpression.members == ZR_NULL || node->data.primaryExpression.members->nodes == ZR_NULL ||
        node->data.primaryExpression.members->count == 0) {
        return ZR_FALSE;
    }

    binding = ZrLanguageServer_LspProject_FindImportBindingByAlias(bindings, receiverNode->data.identifier.name);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    currentModuleName = binding->moduleName;
    if (semantic_import_chain_range_contains_position(receiverNode->location, queryRange)) {
        outHit->moduleName = currentModuleName;
        outHit->memberName = ZR_NULL;
        outHit->location = receiverNode->location;
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < node->data.primaryExpression.members->count; index++) {
        SZrAstNode *memberNode = node->data.primaryExpression.members->nodes[index];
        SZrLspResolvedMetadataMember resolvedMember;
        SZrString *nextModuleName;

        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL) {
            continue;
        }

        if (memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
            memberNode->data.memberExpression.computed ||
            memberNode->data.memberExpression.property == ZR_NULL ||
            memberNode->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL) {
            break;
        }

        if (!semantic_import_chain_resolve_linked_member_internal(state,
                                                                  provider,
                                                                  analyzer,
                                                                  projectIndex,
                                                                  currentModuleName,
                                                                  memberNode->data.memberExpression.property
                                                                      ->data.identifier.name,
                                                                  &resolvedMember,
                                                                  &nextModuleName)) {
            break;
        }

        if (semantic_import_chain_range_contains_position(
                memberNode->data.memberExpression.property->location,
                queryRange)) {
            outHit->moduleName = currentModuleName;
            outHit->memberName = memberNode->data.memberExpression.property->data.identifier.name;
            outHit->location = memberNode->data.memberExpression.property->location;
            outHit->resolvedMember = resolvedMember;
            return ZR_TRUE;
        }

        if (nextModuleName == ZR_NULL) {
            break;
        }

        currentModuleName = nextModuleName;
    }

    return ZR_FALSE;
}

static TZrBool semantic_import_chain_append_primary_expression_matches(SZrState *state,
                                                                       SZrLspMetadataProvider *provider,
                                                                       SZrSemanticAnalyzer *analyzer,
                                                                       SZrLspProjectIndex *projectIndex,
                                                                       SZrArray *bindings,
                                                                       SZrAstNode *node,
                                                                       SZrString *uri,
                                                                       SZrString *targetModuleName,
                                                                       SZrString *targetMemberName,
                                                                       SZrArray *result) {
    SZrAstNode *receiverNode;
    SZrLspImportBinding *binding;
    SZrString *currentModuleName;
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || provider == ZR_NULL || analyzer == ZR_NULL || bindings == ZR_NULL ||
        node == ZR_NULL || uri == ZR_NULL || targetModuleName == ZR_NULL || targetMemberName == ZR_NULL ||
        result == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    receiverNode = node->data.primaryExpression.property;
    if (receiverNode == ZR_NULL || receiverNode->type != ZR_AST_IDENTIFIER_LITERAL ||
        node->data.primaryExpression.members == ZR_NULL || node->data.primaryExpression.members->nodes == ZR_NULL ||
        node->data.primaryExpression.members->count == 0) {
        return ZR_FALSE;
    }

    binding = ZrLanguageServer_LspProject_FindImportBindingByAlias(bindings, receiverNode->data.identifier.name);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    currentModuleName = binding->moduleName;
    for (TZrSize index = 0; index < node->data.primaryExpression.members->count; index++) {
        SZrAstNode *memberNode = node->data.primaryExpression.members->nodes[index];
        SZrLspResolvedMetadataMember resolvedMember;
        SZrString *memberName;
        SZrString *nextModuleName;

        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL) {
            continue;
        }

        if (memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
            memberNode->data.memberExpression.computed ||
            memberNode->data.memberExpression.property == ZR_NULL ||
            memberNode->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL) {
            break;
        }

        memberName = memberNode->data.memberExpression.property->data.identifier.name;
        if (!semantic_import_chain_resolve_linked_member_internal(state,
                                                                  provider,
                                                                  analyzer,
                                                                  projectIndex,
                                                                  currentModuleName,
                                                                  memberName,
                                                                  &resolvedMember,
                                                                  &nextModuleName)) {
            break;
        }

        if (ZrLanguageServer_Lsp_StringsEqual(currentModuleName, targetModuleName) &&
            ZrLanguageServer_Lsp_StringsEqual(memberName, targetMemberName) &&
            semantic_import_chain_append_location(state,
                                                  result,
                                                  uri,
                                                  memberNode->data.memberExpression.property->location)) {
            appended = ZR_TRUE;
        }

        if (nextModuleName == ZR_NULL) {
            break;
        }

        currentModuleName = nextModuleName;
    }

    return appended;
}

static TZrBool semantic_import_chain_resolve_in_node_array(SZrState *state,
                                                           SZrLspMetadataProvider *provider,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrLspProjectIndex *projectIndex,
                                                           SZrArray *bindings,
                                                           SZrAstNodeArray *nodes,
                                                           SZrFileRange queryRange,
                                                           SZrLspSemanticImportChainHit *outHit);
static TZrBool semantic_import_chain_resolve_recursive(SZrState *state,
                                                       SZrLspMetadataProvider *provider,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrLspProjectIndex *projectIndex,
                                                       SZrArray *bindings,
                                                       SZrAstNode *node,
                                                       SZrFileRange queryRange,
                                                       SZrLspSemanticImportChainHit *outHit);
static TZrBool semantic_import_chain_append_in_node_array(SZrState *state,
                                                          SZrLspMetadataProvider *provider,
                                                          SZrSemanticAnalyzer *analyzer,
                                                          SZrLspProjectIndex *projectIndex,
                                                          SZrArray *bindings,
                                                          SZrAstNodeArray *nodes,
                                                          SZrString *uri,
                                                          SZrString *targetModuleName,
                                                          SZrString *targetMemberName,
                                                          SZrArray *result);
static TZrBool semantic_import_chain_append_recursive(SZrState *state,
                                                      SZrLspMetadataProvider *provider,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrLspProjectIndex *projectIndex,
                                                      SZrArray *bindings,
                                                      SZrAstNode *node,
                                                      SZrString *uri,
                                                      SZrString *targetModuleName,
                                                      SZrString *targetMemberName,
                                                      SZrArray *result);

static TZrBool semantic_import_chain_resolve_recursive(SZrState *state,
                                                       SZrLspMetadataProvider *provider,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrLspProjectIndex *projectIndex,
                                                       SZrArray *bindings,
                                                       SZrAstNode *node,
                                                       SZrFileRange queryRange,
                                                       SZrLspSemanticImportChainHit *outHit) {
    if (node == ZR_NULL || outHit == ZR_NULL) {
        return ZR_FALSE;
    }

    if (semantic_import_chain_resolve_primary_expression(state,
                                                         provider,
                                                         analyzer,
                                                         projectIndex,
                                                         bindings,
                                                         node,
                                                         queryRange,
                                                         outHit)) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return semantic_import_chain_resolve_in_node_array(state,
                                                               provider,
                                                               analyzer,
                                                               projectIndex,
                                                               bindings,
                                                               node->data.script.statements,
                                                               queryRange,
                                                               outHit);
        case ZR_AST_BLOCK:
            return semantic_import_chain_resolve_in_node_array(state,
                                                               provider,
                                                               analyzer,
                                                               projectIndex,
                                                               bindings,
                                                               node->data.block.body,
                                                               queryRange,
                                                               outHit);
        case ZR_AST_FUNCTION_DECLARATION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.functionDeclaration.body,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_TEST_DECLARATION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.testDeclaration.body,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_STRUCT_METHOD:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.structMethod.body,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_STRUCT_META_FUNCTION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.structMetaFunction.body,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_CLASS_METHOD:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.classMethod.body,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_CLASS_META_FUNCTION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.classMetaFunction.body,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_PROPERTY_GET:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.propertyGet.body,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_PROPERTY_SET:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.propertySet.body,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_CLASS_PROPERTY:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.classProperty.modifier,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_VARIABLE_DECLARATION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.variableDeclaration.value,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_STRUCT_FIELD:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.structField.init,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_CLASS_FIELD:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.classField.init,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_ENUM_MEMBER:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.enumMember.value,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_RETURN_STATEMENT:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.returnStatement.expr,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_EXPRESSION_STATEMENT:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.expressionStatement.expr,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_USING_STATEMENT:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.usingStatement.resource,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.usingStatement.body,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.breakContinueStatement.expr,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_THROW_STATEMENT:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.throwStatement.expr,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_OUT_STATEMENT:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.outStatement.expr,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.tryCatchFinallyStatement.block,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_in_node_array(state,
                                                               provider,
                                                               analyzer,
                                                               projectIndex,
                                                               bindings,
                                                               node->data.tryCatchFinallyStatement.catchClauses,
                                                               queryRange,
                                                               outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.tryCatchFinallyStatement.finallyBlock,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_CATCH_CLAUSE:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.catchClause.block,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.assignmentExpression.left,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.assignmentExpression.right,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_BINARY_EXPRESSION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.binaryExpression.left,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.binaryExpression.right,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_LOGICAL_EXPRESSION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.logicalExpression.left,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.logicalExpression.right,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.conditionalExpression.test,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.conditionalExpression.consequent,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.conditionalExpression.alternate,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_UNARY_EXPRESSION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.unaryExpression.argument,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.typeCastExpression.expression,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_LAMBDA_EXPRESSION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.lambdaExpression.block,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_IF_EXPRESSION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.ifExpression.condition,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.ifExpression.thenExpr,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.ifExpression.elseExpr,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_SWITCH_EXPRESSION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.switchExpression.expr,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_in_node_array(state,
                                                               provider,
                                                               analyzer,
                                                               projectIndex,
                                                               bindings,
                                                               node->data.switchExpression.cases,
                                                               queryRange,
                                                               outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.switchExpression.defaultCase,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_SWITCH_CASE:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.switchCase.value,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.switchCase.block,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_SWITCH_DEFAULT:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.switchDefault.block,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_FUNCTION_CALL:
            return semantic_import_chain_resolve_in_node_array(state,
                                                               provider,
                                                               analyzer,
                                                               projectIndex,
                                                               bindings,
                                                               node->data.functionCall.args,
                                                               queryRange,
                                                               outHit);
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.constructExpression.target,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_in_node_array(state,
                                                               provider,
                                                               analyzer,
                                                               projectIndex,
                                                               bindings,
                                                               node->data.constructExpression.args,
                                                               queryRange,
                                                               outHit);
        case ZR_AST_PRIMARY_EXPRESSION:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.primaryExpression.property,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_in_node_array(state,
                                                               provider,
                                                               analyzer,
                                                               projectIndex,
                                                               bindings,
                                                               node->data.primaryExpression.members,
                                                               queryRange,
                                                               outHit);
        case ZR_AST_MEMBER_EXPRESSION:
            return node->data.memberExpression.computed
                       ? semantic_import_chain_resolve_recursive(state,
                                                                 provider,
                                                                 analyzer,
                                                                 projectIndex,
                                                                 bindings,
                                                                 node->data.memberExpression.property,
                                                                 queryRange,
                                                                 outHit)
                       : ZR_FALSE;
        case ZR_AST_ARRAY_LITERAL:
            return semantic_import_chain_resolve_in_node_array(state,
                                                               provider,
                                                               analyzer,
                                                               projectIndex,
                                                               bindings,
                                                               node->data.arrayLiteral.elements,
                                                               queryRange,
                                                               outHit);
        case ZR_AST_OBJECT_LITERAL:
            return semantic_import_chain_resolve_in_node_array(state,
                                                               provider,
                                                               analyzer,
                                                               projectIndex,
                                                               bindings,
                                                               node->data.objectLiteral.properties,
                                                               queryRange,
                                                               outHit);
        case ZR_AST_KEY_VALUE_PAIR:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.keyValuePair.key,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.keyValuePair.value,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_WHILE_LOOP:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.whileLoop.cond,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.whileLoop.block,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_FOR_LOOP:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.forLoop.init,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.forLoop.cond,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.forLoop.step,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.forLoop.block,
                                                           queryRange,
                                                           outHit);
        case ZR_AST_FOREACH_LOOP:
            return semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.foreachLoop.expr,
                                                           queryRange,
                                                           outHit) ||
                   semantic_import_chain_resolve_recursive(state,
                                                           provider,
                                                           analyzer,
                                                           projectIndex,
                                                           bindings,
                                                           node->data.foreachLoop.block,
                                                           queryRange,
                                                           outHit);
        default:
            break;
    }

    return ZR_FALSE;
}

static TZrBool semantic_import_chain_append_recursive(SZrState *state,
                                                      SZrLspMetadataProvider *provider,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrLspProjectIndex *projectIndex,
                                                      SZrArray *bindings,
                                                      SZrAstNode *node,
                                                      SZrString *uri,
                                                      SZrString *targetModuleName,
                                                      SZrString *targetMemberName,
                                                      SZrArray *result) {
    TZrBool appended = ZR_FALSE;

    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    appended = semantic_import_chain_append_primary_expression_matches(state,
                                                                       provider,
                                                                       analyzer,
                                                                       projectIndex,
                                                                       bindings,
                                                                       node,
                                                                       uri,
                                                                       targetModuleName,
                                                                       targetMemberName,
                                                                       result) ||
               appended;

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return semantic_import_chain_append_in_node_array(state,
                                                              provider,
                                                              analyzer,
                                                              projectIndex,
                                                              bindings,
                                                              node->data.script.statements,
                                                              uri,
                                                              targetModuleName,
                                                              targetMemberName,
                                                              result) || appended;
        case ZR_AST_BLOCK:
            return semantic_import_chain_append_in_node_array(state,
                                                              provider,
                                                              analyzer,
                                                              projectIndex,
                                                              bindings,
                                                              node->data.block.body,
                                                              uri,
                                                              targetModuleName,
                                                              targetMemberName,
                                                              result) || appended;
        case ZR_AST_FUNCTION_DECLARATION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.functionDeclaration.body,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_TEST_DECLARATION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.testDeclaration.body,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_STRUCT_METHOD:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.structMethod.body,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_STRUCT_META_FUNCTION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.structMetaFunction.body,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_CLASS_METHOD:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.classMethod.body,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_CLASS_META_FUNCTION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.classMetaFunction.body,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_PROPERTY_GET:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.propertyGet.body,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_PROPERTY_SET:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.propertySet.body,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_CLASS_PROPERTY:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.classProperty.modifier,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_VARIABLE_DECLARATION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.variableDeclaration.value,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_STRUCT_FIELD:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.structField.init,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_CLASS_FIELD:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.classField.init,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_ENUM_MEMBER:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.enumMember.value,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_RETURN_STATEMENT:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.returnStatement.expr,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_EXPRESSION_STATEMENT:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.expressionStatement.expr,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_USING_STATEMENT:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.usingStatement.resource,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.usingStatement.body,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.breakContinueStatement.expr,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_THROW_STATEMENT:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.throwStatement.expr,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_OUT_STATEMENT:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.outStatement.expr,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.tryCatchFinallyStatement.block,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_in_node_array(state,
                                                              provider,
                                                              analyzer,
                                                              projectIndex,
                                                              bindings,
                                                              node->data.tryCatchFinallyStatement.catchClauses,
                                                              uri,
                                                              targetModuleName,
                                                              targetMemberName,
                                                              result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.tryCatchFinallyStatement.finallyBlock,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_CATCH_CLAUSE:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.catchClause.block,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.assignmentExpression.left,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.assignmentExpression.right,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_BINARY_EXPRESSION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.binaryExpression.left,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.binaryExpression.right,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_LOGICAL_EXPRESSION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.logicalExpression.left,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.logicalExpression.right,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.conditionalExpression.test,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.conditionalExpression.consequent,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.conditionalExpression.alternate,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_UNARY_EXPRESSION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.unaryExpression.argument,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.typeCastExpression.expression,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_LAMBDA_EXPRESSION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.lambdaExpression.block,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_IF_EXPRESSION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.ifExpression.condition,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.ifExpression.thenExpr,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.ifExpression.elseExpr,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_SWITCH_EXPRESSION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.switchExpression.expr,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_in_node_array(state,
                                                              provider,
                                                              analyzer,
                                                              projectIndex,
                                                              bindings,
                                                              node->data.switchExpression.cases,
                                                              uri,
                                                              targetModuleName,
                                                              targetMemberName,
                                                              result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.switchExpression.defaultCase,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_SWITCH_CASE:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.switchCase.value,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.switchCase.block,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_SWITCH_DEFAULT:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.switchDefault.block,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) || appended;
        case ZR_AST_FUNCTION_CALL:
            return semantic_import_chain_append_in_node_array(state,
                                                              provider,
                                                              analyzer,
                                                              projectIndex,
                                                              bindings,
                                                              node->data.functionCall.args,
                                                              uri,
                                                              targetModuleName,
                                                              targetMemberName,
                                                              result) || appended;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.constructExpression.target,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_in_node_array(state,
                                                              provider,
                                                              analyzer,
                                                              projectIndex,
                                                              bindings,
                                                              node->data.constructExpression.args,
                                                              uri,
                                                              targetModuleName,
                                                              targetMemberName,
                                                              result) ||
                   appended;
        case ZR_AST_PRIMARY_EXPRESSION:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.primaryExpression.property,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_in_node_array(state,
                                                              provider,
                                                              analyzer,
                                                              projectIndex,
                                                              bindings,
                                                              node->data.primaryExpression.members,
                                                              uri,
                                                              targetModuleName,
                                                              targetMemberName,
                                                              result) ||
                   appended;
        case ZR_AST_MEMBER_EXPRESSION:
            return node->data.memberExpression.computed
                       ? semantic_import_chain_append_recursive(state,
                                                                 provider,
                                                                 analyzer,
                                                                 projectIndex,
                                                                 bindings,
                                                                 node->data.memberExpression.property,
                                                                 uri,
                                                                 targetModuleName,
                                                                 targetMemberName,
                                                                 result) ||
                             appended
                       : appended;
        case ZR_AST_ARRAY_LITERAL:
            return semantic_import_chain_append_in_node_array(state,
                                                              provider,
                                                              analyzer,
                                                              projectIndex,
                                                              bindings,
                                                              node->data.arrayLiteral.elements,
                                                              uri,
                                                              targetModuleName,
                                                              targetMemberName,
                                                              result) || appended;
        case ZR_AST_OBJECT_LITERAL:
            return semantic_import_chain_append_in_node_array(state,
                                                              provider,
                                                              analyzer,
                                                              projectIndex,
                                                              bindings,
                                                              node->data.objectLiteral.properties,
                                                              uri,
                                                              targetModuleName,
                                                              targetMemberName,
                                                              result) || appended;
        case ZR_AST_KEY_VALUE_PAIR:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.keyValuePair.key,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.keyValuePair.value,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_WHILE_LOOP:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.whileLoop.cond,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.whileLoop.block,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_FOR_LOOP:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.forLoop.init,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.forLoop.cond,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.forLoop.step,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.forLoop.block,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        case ZR_AST_FOREACH_LOOP:
            return semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.foreachLoop.expr,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          node->data.foreachLoop.block,
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
        default:
            break;
    }

    return appended;
}

static TZrBool semantic_import_chain_append_in_node_array(SZrState *state,
                                                          SZrLspMetadataProvider *provider,
                                                          SZrSemanticAnalyzer *analyzer,
                                                          SZrLspProjectIndex *projectIndex,
                                                          SZrArray *bindings,
                                                          SZrAstNodeArray *nodes,
                                                          SZrString *uri,
                                                          SZrString *targetModuleName,
                                                          SZrString *targetMemberName,
                                                          SZrArray *result) {
    TZrBool appended = ZR_FALSE;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        appended = semantic_import_chain_append_recursive(state,
                                                          provider,
                                                          analyzer,
                                                          projectIndex,
                                                          bindings,
                                                          nodes->nodes[index],
                                                          uri,
                                                          targetModuleName,
                                                          targetMemberName,
                                                          result) ||
                   appended;
    }

    return appended;
}

static TZrBool semantic_import_chain_resolve_completion_module_internal(SZrState *state,
                                                                        SZrLspMetadataProvider *provider,
                                                                        SZrSemanticAnalyzer *analyzer,
                                                                        SZrLspProjectIndex *projectIndex,
                                                                        SZrArray *bindings,
                                                                        const TZrChar *content,
                                                                        TZrSize contentLength,
                                                                        TZrSize cursorOffset,
                                                                        SZrLspResolvedImportedModule *outResolvedModule) {
    TZrSize chainEnd;
    TZrSize chainStart;
    TZrSize parseIndex;
    SZrString *aliasName;
    SZrLspImportBinding *binding;
    SZrString *currentModuleName;

    if (outResolvedModule != ZR_NULL) {
        memset(outResolvedModule, 0, sizeof(*outResolvedModule));
    }
    if (state == ZR_NULL || provider == ZR_NULL || analyzer == ZR_NULL || bindings == ZR_NULL ||
        content == ZR_NULL || cursorOffset == 0 || cursorOffset > contentLength || outResolvedModule == ZR_NULL) {
        return ZR_FALSE;
    }

    chainEnd = cursorOffset;
    while (chainEnd > 0 &&
           isspace((unsigned char)content[chainEnd - 1]) &&
           content[chainEnd - 1] != '\n' &&
           content[chainEnd - 1] != '\r') {
        chainEnd--;
    }
    if (chainEnd == 0 || content[chainEnd - 1] != '.') {
        return ZR_FALSE;
    }

    chainEnd--;
    while (chainEnd > 0 &&
           isspace((unsigned char)content[chainEnd - 1]) &&
           content[chainEnd - 1] != '\n' &&
           content[chainEnd - 1] != '\r') {
        chainEnd--;
    }
    if (chainEnd == 0) {
        return ZR_FALSE;
    }

    chainStart = chainEnd;
    while (chainStart > 0) {
        TZrChar current = content[chainStart - 1];
        if (!isalnum((unsigned char)current) && current != '_' && current != '.') {
            break;
        }
        chainStart--;
    }
    if (chainStart >= chainEnd ||
        (!isalpha((unsigned char)content[chainStart]) && content[chainStart] != '_')) {
        return ZR_FALSE;
    }

    parseIndex = chainStart;
    while (parseIndex < chainEnd &&
           (isalnum((unsigned char)content[parseIndex]) || content[parseIndex] == '_')) {
        parseIndex++;
    }
    if (parseIndex == chainStart) {
        return ZR_FALSE;
    }

    aliasName = ZrCore_String_Create(state,
                                     (TZrNativeString)(content + chainStart),
                                     parseIndex - chainStart);
    if (aliasName == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = ZrLanguageServer_LspProject_FindImportBindingByAlias(bindings, aliasName);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    currentModuleName = binding->moduleName;
    while (parseIndex < chainEnd) {
        TZrSize memberStart;
        SZrString *memberName;
        SZrLspResolvedMetadataMember resolvedMember;
        SZrString *nextModuleName = ZR_NULL;

        if (content[parseIndex] != '.') {
            return ZR_FALSE;
        }
        parseIndex++;
        memberStart = parseIndex;
        while (parseIndex < chainEnd &&
               (isalnum((unsigned char)content[parseIndex]) || content[parseIndex] == '_')) {
            parseIndex++;
        }
        if (parseIndex == memberStart) {
            return ZR_FALSE;
        }

        memberName = ZrCore_String_Create(state,
                                          (TZrNativeString)(content + memberStart),
                                          parseIndex - memberStart);
        if (memberName == ZR_NULL) {
            return ZR_FALSE;
        }

        if (!semantic_import_chain_resolve_linked_member_internal(state,
                                                                  provider,
                                                                  analyzer,
                                                                  projectIndex,
                                                                  currentModuleName,
                                                                  memberName,
                                                                  &resolvedMember,
                                                                  &nextModuleName) ||
            nextModuleName == ZR_NULL) {
            return ZR_FALSE;
        }

        currentModuleName = nextModuleName;
    }

    return ZrLanguageServer_LspMetadataProvider_ResolveImportedModule(provider,
                                                                      analyzer,
                                                                      projectIndex,
                                                                      currentModuleName,
                                                                      outResolvedModule);
}

TZrBool ZrLanguageServer_LspSemanticImportChain_ResolveLinkedMember(
    SZrState *state,
    SZrLspContext *context,
    SZrSemanticAnalyzer *analyzer,
    SZrLspProjectIndex *projectIndex,
    SZrString *moduleName,
    SZrString *memberName,
    SZrLspResolvedMetadataMember *outResolvedMember,
    SZrString **outNextModuleName) {
    SZrLspMetadataProvider provider;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL || outResolvedMember == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    return semantic_import_chain_resolve_linked_member_internal(state,
                                                                &provider,
                                                                analyzer,
                                                                projectIndex,
                                                                moduleName,
                                                                memberName,
                                                                outResolvedMember,
                                                                outNextModuleName);
}

TZrBool ZrLanguageServer_LspSemanticImportChain_ResolveCompletionModuleAtOffset(
    SZrState *state,
    SZrLspContext *context,
    SZrLspProjectIndex *projectIndex,
    SZrSemanticAnalyzer *analyzer,
    SZrArray *bindings,
    const TZrChar *content,
    TZrSize contentLength,
    TZrSize cursorOffset,
    SZrLspResolvedImportedModule *outResolvedModule) {
    SZrLspMetadataProvider provider;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL || bindings == ZR_NULL ||
        outResolvedModule == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    return semantic_import_chain_resolve_completion_module_internal(state,
                                                                    &provider,
                                                                    analyzer,
                                                                    projectIndex,
                                                                    bindings,
                                                                    content,
                                                                    contentLength,
                                                                    cursorOffset,
                                                                    outResolvedModule);
}

TZrBool ZrLanguageServer_LspSemanticImportChain_ResolveAtRange(SZrState *state,
                                                               SZrLspContext *context,
                                                               SZrLspProjectIndex *projectIndex,
                                                               SZrSemanticAnalyzer *analyzer,
                                                               SZrArray *bindings,
                                                               SZrFileRange queryRange,
                                                               SZrLspSemanticImportChainHit *outHit) {
    SZrLspMetadataProvider provider;

    if (outHit != ZR_NULL) {
        memset(outHit, 0, sizeof(*outHit));
    }
    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL ||
        bindings == ZR_NULL || outHit == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    return semantic_import_chain_resolve_recursive(state,
                                                   &provider,
                                                   analyzer,
                                                   projectIndex,
                                                   bindings,
                                                   analyzer->ast,
                                                   queryRange,
                                                   outHit);
}

TZrBool ZrLanguageServer_LspSemanticImportChain_AppendMatchingLocationsForUri(SZrState *state,
                                                                              SZrLspContext *context,
                                                                              SZrLspProjectIndex *projectIndex,
                                                                              SZrString *uri,
                                                                              SZrString *moduleName,
                                                                              SZrString *memberName,
                                                                              SZrArray *result) {
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrArray bindings;
    SZrLspMetadataProvider provider;
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || moduleName == ZR_NULL ||
        memberName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!semantic_import_chain_try_get_analyzer_for_uri(state, context, uri, &analyzer) ||
        analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    if (projectIndex == ZR_NULL) {
        projectIndex = ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(state, context, uri);
    }

    ZrCore_Array_Init(state, &bindings, sizeof(SZrLspImportBinding *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrLanguageServer_LspProject_CollectImportBindings(state, analyzer->ast, &bindings);
    ZrLanguageServer_LspMetadataProvider_Init(&provider, state, context);
    appended = semantic_import_chain_append_recursive(state,
                                                      &provider,
                                                      analyzer,
                                                      projectIndex,
                                                      &bindings,
                                                      analyzer->ast,
                                                      uri,
                                                      moduleName,
                                                      memberName,
                                                      result);
    ZrLanguageServer_LspProject_FreeImportBindings(state, &bindings);
    return appended;
}

static TZrBool semantic_import_chain_resolve_in_node_array(SZrState *state,
                                                           SZrLspMetadataProvider *provider,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrLspProjectIndex *projectIndex,
                                                           SZrArray *bindings,
                                                           SZrAstNodeArray *nodes,
                                                           SZrFileRange queryRange,
                                                           SZrLspSemanticImportChainHit *outHit) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (semantic_import_chain_resolve_recursive(state,
                                                    provider,
                                                    analyzer,
                                                    projectIndex,
                                                    bindings,
                                                    nodes->nodes[index],
                                                    queryRange,
                                                    outHit)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}
