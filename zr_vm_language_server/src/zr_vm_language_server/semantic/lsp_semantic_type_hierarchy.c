#include "semantic/lsp_semantic_type_hierarchy.h"

#include "interface/lsp_interface_internal.h"
#include "semantic/semantic_analyzer_query_source.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_query.h"

static TZrBool semantic_type_hierarchy_symbol_is_type(
        const SZrSymbol *symbol) {
    return symbol != ZR_NULL &&
           (symbol->type == ZR_SYMBOL_CLASS ||
            symbol->type == ZR_SYMBOL_STRUCT ||
            symbol->type == ZR_SYMBOL_INTERFACE ||
            symbol->type == ZR_SYMBOL_ENUM);
}

static TZrInt32 semantic_type_hierarchy_symbol_kind(const SZrSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return ZR_LSP_SYMBOL_KIND_CLASS;
    }
    switch (symbol->type) {
        case ZR_SYMBOL_STRUCT:
            return ZR_LSP_SYMBOL_KIND_STRUCT;
        case ZR_SYMBOL_INTERFACE:
            return ZR_LSP_SYMBOL_KIND_INTERFACE;
        case ZR_SYMBOL_ENUM:
            return ZR_LSP_SYMBOL_KIND_ENUM;
        case ZR_SYMBOL_CLASS:
        default:
            return ZR_LSP_SYMBOL_KIND_CLASS;
    }
}

static SZrSymbol *semantic_type_hierarchy_find_symbol(
        SZrSemanticAnalyzer *analyzer,
        TZrSymbolId semanticId) {
    TZrSize scopeIndex;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL ||
        semanticId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    for (scopeIndex = 0U;
         scopeIndex < analyzer->symbolTable->allScopes.length;
         scopeIndex++) {
        SZrSymbolScope **scopeSlot =
                (SZrSymbolScope **)ZrCore_Array_Get(
                        &analyzer->symbolTable->allScopes, scopeIndex);
        SZrSymbolScope *scope =
                scopeSlot != ZR_NULL ? *scopeSlot : ZR_NULL;
        TZrSize symbolIndex;

        for (symbolIndex = 0U;
             scope != ZR_NULL && symbolIndex < scope->symbols.length;
             symbolIndex++) {
            SZrSymbol **symbolSlot =
                    (SZrSymbol **)ZrCore_Array_Get(
                            &scope->symbols, symbolIndex);
            if (symbolSlot != ZR_NULL && *symbolSlot != ZR_NULL &&
                (*symbolSlot)->semanticId == semanticId) {
                return *symbolSlot;
            }
        }
    }
    return ZR_NULL;
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
        SZrSymbol *symbol,
        SZrFileRange declarationRange,
        SZrArray *result) {
    SZrFileRange boundDeclaration;
    SZrFileRange boundSelection;
    SZrFileVersion *fileVersion;
    SZrLspHierarchyItem *item;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        !semantic_type_hierarchy_symbol_is_type(symbol) ||
        symbol->semanticId == ZR_SEMANTIC_ID_INVALID ||
        symbol->semanticTypeId == ZR_SEMANTIC_ID_INVALID ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }
    boundDeclaration = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, declarationRange);
    boundSelection = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, symbol->selectionRange);
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
    if (semantic_type_hierarchy_has_item(result, symbol->semanticId)) {
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
    item->name = symbol->name;
    item->kind = semantic_type_hierarchy_symbol_kind(symbol);
    item->uri = boundDeclaration.source;
    item->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, item->uri, boundDeclaration);
    item->selectionRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, item->uri, boundSelection);
    item->hasSemanticIdentity = ZR_TRUE;
    item->semanticId = symbol->semanticId;
    item->semanticTypeId = symbol->semanticTypeId;
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
    ZrLanguageServer_LspSemanticQuery_Init(query);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state,
                context,
                item->uri,
                item->selectionRange.start,
                query) ||
        query->kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
        query->analyzer == ZR_NULL ||
        query->analyzer->semanticContext == ZR_NULL ||
        query->symbol == ZR_NULL ||
        !semantic_type_hierarchy_symbol_is_type(query->symbol) ||
        query->symbol->semanticId != item->semanticId ||
        query->symbol->semanticTypeId != item->semanticTypeId) {
        ZrLanguageServer_LspSemanticQuery_Free(state, query);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspSemanticTypeHierarchy_Prepare(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result) {
    SZrLspSemanticQuery query;
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
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, position, &query) &&
        query.kind == ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL &&
        query.analyzer != ZR_NULL && query.symbol != ZR_NULL &&
        semantic_type_hierarchy_symbol_is_type(query.symbol)) {
        ok = semantic_type_hierarchy_append_item(
                state,
                context,
                query.analyzer,
                query.symbol,
                query.symbol->location,
                result);
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
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
            SZrSymbol *targetSymbol;

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
            targetSymbol = semantic_type_hierarchy_find_symbol(
                    query.analyzer, targetId);
            if (!hasTargetRange || targetSymbol == ZR_NULL ||
                targetSymbol->semanticTypeId != targetTypeId) {
                continue;
            }
            if (!semantic_type_hierarchy_append_item(
                        state,
                        context,
                        query.analyzer,
                        targetSymbol,
                        targetRange,
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
