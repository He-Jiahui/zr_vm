#include "semantic/lsp_semantic_type_hierarchy.h"

#include "interface/lsp_interface_internal.h"
#include "semantic/semantic_analyzer_query_source.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_query.h"

static TZrBool semantic_type_hierarchy_node_is_type(const SZrAstNode *node) {
    return node != ZR_NULL &&
           (node->type == ZR_AST_CLASS_DECLARATION ||
            node->type == ZR_AST_STRUCT_DECLARATION ||
            node->type == ZR_AST_INTERFACE_DECLARATION ||
            node->type == ZR_AST_ENUM_DECLARATION);
}

static TZrInt32 semantic_type_hierarchy_node_kind(const SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_LSP_SYMBOL_KIND_CLASS;
    }
    switch (node->type) {
        case ZR_AST_STRUCT_DECLARATION:
            return ZR_LSP_SYMBOL_KIND_STRUCT;
        case ZR_AST_INTERFACE_DECLARATION:
            return ZR_LSP_SYMBOL_KIND_INTERFACE;
        case ZR_AST_ENUM_DECLARATION:
            return ZR_LSP_SYMBOL_KIND_ENUM;
        case ZR_AST_CLASS_DECLARATION:
        default:
            return ZR_LSP_SYMBOL_KIND_CLASS;
    }
}

static TZrBool semantic_type_hierarchy_has_item(
        const SZrArray *result,
        TZrSymbolId semanticId) {
    TZrSize index;

    if (result == ZR_NULL || !result->isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < result->length; index++) {
        SZrLspHierarchyItem *const *slot =
                (SZrLspHierarchyItem *const *)ZrCore_Array_Get(
                        (SZrArray *)result, index);
        if (slot != ZR_NULL && *slot != ZR_NULL &&
            (*slot)->hasSemanticIdentity &&
            (*slot)->semanticId == semanticId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool semantic_type_hierarchy_append_item(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        const SZrSemanticSymbolRecord *semanticSymbol,
        SZrFileRange declarationRange,
        SZrFileRange selectionRange,
        SZrArray *result) {
    SZrFileRange boundDeclaration;
    SZrFileRange boundSelection;
    SZrFileVersion *fileVersion;
    SZrLspHierarchyItem *item;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        semanticSymbol == ZR_NULL ||
        semanticSymbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
        semanticSymbol->name == ZR_NULL ||
        !semantic_type_hierarchy_node_is_type(semanticSymbol->astNode) ||
        semanticSymbol->id == ZR_SEMANTIC_ID_INVALID ||
        semanticSymbol->typeId == ZR_SEMANTIC_ID_INVALID ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }
    boundDeclaration = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, declarationRange);
    boundSelection = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, selectionRange);
    if (boundDeclaration.source == ZR_NULL || boundSelection.source == ZR_NULL ||
        !ZrLanguageServer_Lsp_StringsEqual(
                boundDeclaration.source, boundSelection.source)) {
        return ZR_FALSE;
    }
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(
            context, boundDeclaration.source);
    if (fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }
    if (semantic_type_hierarchy_has_item(result, semanticSymbol->id)) {
        return ZR_TRUE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(
                state,
                result,
                sizeof(SZrLspHierarchyItem *),
                ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }
    item = (SZrLspHierarchyItem *)ZrCore_Memory_RawMalloc(
            state->global, sizeof(SZrLspHierarchyItem));
    if (item == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(item, 0, sizeof(SZrLspHierarchyItem));
    item->name = semanticSymbol->name;
    item->kind = semantic_type_hierarchy_node_kind(semanticSymbol->astNode);
    item->uri = boundDeclaration.source;
    item->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, item->uri, boundDeclaration);
    item->selectionRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, item->uri, boundSelection);
    item->hasSemanticIdentity = ZR_TRUE;
    item->semanticId = semanticSymbol->id;
    item->semanticTypeId = semanticSymbol->typeId;
    item->semanticVersion = fileVersion->version;
    ZrCore_Array_Push(state, result, &item);
    return ZR_TRUE;
}

static TZrBool semantic_type_hierarchy_resolve_item(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrLspSemanticQuery *query) {
    SZrFileVersion *fileVersion;
    SZrSemanticAnalyzer *analyzer;
    const SZrSemanticSymbolRecord *semanticSymbol;
    const SZrSemanticReferenceFact *declaration;
    SZrFileRange boundDeclaration;
    SZrFileRange boundSelection;
    SZrLspRange declarationRange;
    SZrLspRange selectionRange;

    if (state == ZR_NULL || context == ZR_NULL || item == ZR_NULL ||
        query == ZR_NULL || item->uri == ZR_NULL ||
        !item->hasSemanticIdentity ||
        item->semanticId == ZR_SEMANTIC_ID_INVALID ||
        item->semanticTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(
            context, item->uri);
    if (fileVersion == ZR_NULL || fileVersion->version != item->semanticVersion) {
        return ZR_FALSE;
    }
    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(
            state, context, item->uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_FALSE;
    }
    semanticSymbol = ZrParser_Semantic_FindSymbolById(
            analyzer->semanticContext, item->semanticId);
    declaration = ZrParser_SemanticQuery_DeclarationOf(
            analyzer->semanticContext, item->semanticId, ZR_NULL);
    if (semanticSymbol == ZR_NULL || declaration == ZR_NULL ||
        semanticSymbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
        !semantic_type_hierarchy_node_is_type(semanticSymbol->astNode) ||
        semanticSymbol->id != item->semanticId ||
        semanticSymbol->typeId != item->semanticTypeId ||
        !declaration->isResolved ||
        declaration->kind != ZR_SEMANTIC_REFERENCE_DECLARATION ||
        declaration->symbolId != item->semanticId ||
        declaration->typeId != item->semanticTypeId ||
        declaration->node != semanticSymbol->astNode) {
        return ZR_FALSE;
    }
    boundDeclaration = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, semanticSymbol->location);
    boundSelection = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, declaration->range);
    if (boundDeclaration.source == ZR_NULL || boundSelection.source == ZR_NULL ||
        !ZrLanguageServer_Lsp_StringsEqual(
                boundDeclaration.source, item->uri) ||
        !ZrLanguageServer_Lsp_StringsEqual(
                boundSelection.source, item->uri)) {
        return ZR_FALSE;
    }
    declarationRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, item->uri, boundDeclaration);
    selectionRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, item->uri, boundSelection);
    if (item->range.start.line != declarationRange.start.line ||
        item->range.start.character != declarationRange.start.character ||
        item->range.end.line != declarationRange.end.line ||
        item->range.end.character != declarationRange.end.character ||
        item->selectionRange.start.line != selectionRange.start.line ||
        item->selectionRange.start.character != selectionRange.start.character ||
        item->selectionRange.end.line != selectionRange.end.line ||
        item->selectionRange.end.character != selectionRange.end.character) {
        return ZR_FALSE;
    }
    ZrLanguageServer_LspSemanticQuery_Init(query);
    query->uri = item->uri;
    query->analyzer = analyzer;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspSemanticTypeHierarchy_Prepare(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePosition;
    SZrFileRange queryRange;
    SZrParserSemanticSymbolQuery symbolQuery;
    TZrBool ok = ZR_TRUE;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        result == ZR_NULL ||
        ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(
                state,
                result,
                sizeof(SZrLspHierarchyItem *),
                ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }
    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(
            state, context, uri);
    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context, uri, position);
    queryRange = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer,
            ZrParser_FileRange_Create(filePosition, filePosition, uri));
    if (analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL &&
        ZrParser_SemanticQuery_SymbolAt(
                analyzer->semanticContext,
                queryRange,
                ZR_NULL,
                &symbolQuery) &&
        semantic_type_hierarchy_node_is_type(symbolQuery.declarationNode)) {
        const SZrSemanticSymbolRecord *semanticSymbol =
                ZrParser_Semantic_FindSymbolById(
                        analyzer->semanticContext,
                        symbolQuery.symbolId);
        const SZrSemanticReferenceFact *declaration =
                semanticSymbol != ZR_NULL
                        ? ZrParser_SemanticQuery_DeclarationOf(
                                  analyzer->semanticContext,
                                  semanticSymbol->id,
                                  ZR_NULL)
                        : ZR_NULL;

        ok = semanticSymbol != ZR_NULL && declaration != ZR_NULL &&
             semanticSymbol->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE &&
             semanticSymbol->astNode == symbolQuery.declarationNode &&
             semanticSymbol->typeId == symbolQuery.typeId &&
             declaration->isResolved &&
             declaration->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
             declaration->symbolId == semanticSymbol->id &&
             declaration->typeId == semanticSymbol->typeId &&
             declaration->node == semanticSymbol->astNode &&
             semantic_type_hierarchy_append_item(
                     state,
                     context,
                     analyzer,
                     semanticSymbol,
                     semanticSymbol->location,
                     declaration->range,
                     result);
    }
    return ok;
}

static TZrBool semantic_type_hierarchy_append_relations(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        TZrBool derived,
        SZrArray *result) {
    SZrLspSemanticQuery query;
    SZrArray relations = {0};
    TZrSize index;
    TZrBool ok = ZR_TRUE;
    TZrBool queried;

    if (state == ZR_NULL || context == ZR_NULL || item == ZR_NULL ||
        result == ZR_NULL ||
        ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(
                state,
                result,
                sizeof(SZrLspHierarchyItem *),
                ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }
    if (!semantic_type_hierarchy_resolve_item(
                state, context, item, &query)) {
        return ZR_FALSE;
    }
    queried = derived
                      ? ZrParser_SemanticQuery_DerivedTypesOf(
                                query.analyzer->semanticContext,
                                item->semanticTypeId,
                                &relations)
                      : ZrParser_SemanticQuery_BaseTypesOf(
                                query.analyzer->semanticContext,
                                item->semanticTypeId,
                                &relations);
    if (queried) {
        for (index = 0U; index < relations.length; index++) {
            const SZrParserSemanticRelationQuery *relation =
                    (const SZrParserSemanticRelationQuery *)ZrCore_Array_Get(
                            &relations, index);
            TZrSymbolId targetId;
            TZrTypeId targetTypeId;
            SZrFileRange targetRange;
            TZrBool hasTargetRange;
            const SZrSemanticSymbolRecord *targetSymbol;
            const SZrSemanticReferenceFact *targetDeclaration;

            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(
                        context)) {
                ok = ZR_FALSE;
                break;
            }
            if (relation == ZR_NULL) {
                continue;
            }
            targetId = derived
                               ? relation->sourceSymbolId
                               : relation->targetSymbolId;
            targetTypeId = derived
                                   ? relation->sourceTypeId
                                   : relation->targetTypeId;
            targetRange = derived
                                  ? relation->sourceRange
                                  : relation->targetRange;
            hasTargetRange = derived
                                     ? relation->hasSourceRange
                                     : relation->hasTargetRange;
            targetSymbol = ZrParser_Semantic_FindSymbolById(
                    query.analyzer->semanticContext, targetId);
            targetDeclaration = targetSymbol != ZR_NULL
                    ? ZrParser_SemanticQuery_DeclarationOf(
                              query.analyzer->semanticContext,
                              targetId,
                              ZR_NULL)
                    : ZR_NULL;
            if (!hasTargetRange || targetSymbol == ZR_NULL ||
                targetDeclaration == ZR_NULL ||
                targetSymbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
                !semantic_type_hierarchy_node_is_type(targetSymbol->astNode) ||
                targetSymbol->typeId != targetTypeId ||
                !targetDeclaration->isResolved ||
                targetDeclaration->kind != ZR_SEMANTIC_REFERENCE_DECLARATION ||
                targetDeclaration->symbolId != targetId ||
                targetDeclaration->typeId != targetTypeId ||
                targetDeclaration->node != targetSymbol->astNode) {
                continue;
            }
            if (!semantic_type_hierarchy_append_item(
                        state,
                        context,
                        query.analyzer,
                        targetSymbol,
                        targetRange,
                        targetDeclaration->range,
                        result)) {
                ok = ZR_FALSE;
                break;
            }
        }
    }
    if (relations.isValid) {
        ZrCore_Array_Free(state, &relations);
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    return ok;
}

TZrBool ZrLanguageServer_LspSemanticTypeHierarchy_AppendSupertypes(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return semantic_type_hierarchy_append_relations(
            state, context, item, ZR_FALSE, result);
}

TZrBool ZrLanguageServer_LspSemanticTypeHierarchy_AppendSubtypes(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspHierarchyItem *item,
        SZrArray *result) {
    return semantic_type_hierarchy_append_relations(
            state, context, item, ZR_TRUE, result);
}
