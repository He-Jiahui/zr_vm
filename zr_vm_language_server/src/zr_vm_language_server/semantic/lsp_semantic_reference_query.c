#include "semantic/lsp_semantic_reference_query.h"
#include "semantic/lsp_external_target_identity.h"
#include "semantic/semantic_analyzer_query_source.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_query.h"

/* 以编辑器最终看到的范围去重，避免不同语义事实生成同一位置。 */
static TZrBool semantic_reference_query_ranges_equal(
        SZrLspRange left,
        SZrLspRange right) {
    return left.start.line == right.start.line &&
           left.start.character == right.start.character &&
           left.end.line == right.end.line &&
           left.end.character == right.end.character;
}

/* 本快照和跨快照调用方可重复提交同一引用，结果应保持 URI 与范围唯一。 */
static TZrBool semantic_reference_query_has_location(
        const SZrArray *result,
        SZrString *uri,
        SZrLspRange range) {
    TZrSize index;

    if (result == ZR_NULL || !result->isValid) {
        return ZR_FALSE;
    }
    for (index = 0U; index < result->length; index++) {
        SZrLspLocation *const *slot =
                (SZrLspLocation *const *)ZrCore_Array_Get(
                        (SZrArray *)result, index);
        if (slot != ZR_NULL && *slot != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*slot)->uri, uri) &&
            semantic_reference_query_ranges_equal((*slot)->range, range)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 将语义事实绑定其文档来源并追加 Location；跨快照收集器借此统一去重。 */
TZrBool ZrLanguageServer_LspSemanticReferenceQuery_AppendRange(
        SZrState *state,
        SZrLspContext *context,
        const SZrSemanticAnalyzer *analyzer,
        SZrArray *result,
        SZrFileRange range) {
    SZrLspLocation *location;
    SZrFileRange factRange;
    SZrLspRange lspRange;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL ||
        analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    factRange = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, range);
    if (factRange.source == ZR_NULL) {
        return ZR_FALSE;
    }
    lspRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, factRange.source, factRange);
    if (semantic_reference_query_has_location(
                result, factRange.source, lspRange)) {
        return ZR_TRUE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(
                state,
                result,
                sizeof(SZrLspLocation *),
                ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }
    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(
            state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }
    location->uri = factRange.source;
    location->range = lspRange;
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

/* 解析器引用事实可没有可用来源，统一经 AppendRange 执行来源绑定。 */
static TZrBool semantic_reference_query_append_location(
        SZrState *state,
        SZrLspContext *context,
        const SZrSemanticAnalyzer *analyzer,
        SZrArray *result,
        const SZrSemanticReferenceFact *fact) {
    return fact != ZR_NULL &&
           ZrLanguageServer_LspSemanticReferenceQuery_AppendRange(
                   state, context, analyzer, result, fact->range);
}

/* 声明和写入在编辑器中按写类型高亮，其余已解析引用按读类型展示。 */
static TZrInt32 semantic_reference_query_highlight_kind(
        EZrSemanticReferenceKind kind) {
    return kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
                   kind == ZR_SEMANTIC_REFERENCE_WRITE ||
                   kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE
            ? 3
            : 2;
}

/* 本地引用必须沿用规范符号身份；AST 侧 ID 与规范 ID 冲突时不猜测目标。 */
static TZrSymbolId semantic_reference_query_symbol_id(
        const SZrLspSemanticQuery *query) {
    if (query == ZR_NULL || !query->hasCanonicalSymbol ||
        query->canonicalSymbol.symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    if (query->symbol != ZR_NULL &&
        query->symbol->semanticId != query->canonicalSymbol.symbolId) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return query->canonicalSymbol.symbolId;
}

/* 同一范围可能同时带读写事实；复用高亮对象以保留最终写优先级。 */
static SZrLspDocumentHighlight *semantic_reference_query_find_highlight(
        const SZrArray *result,
        SZrLspRange range) {
    TZrSize index;

    if (result == ZR_NULL || !result->isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < result->length; index++) {
        SZrLspDocumentHighlight *const *slot =
                (SZrLspDocumentHighlight *const *)ZrCore_Array_Get(
                        (SZrArray *)result, index);
        if (slot != ZR_NULL && *slot != ZR_NULL &&
            semantic_reference_query_ranges_equal((*slot)->range, range)) {
            return *slot;
        }
    }
    return ZR_NULL;
}

/* 文档高亮只接收当前 URI 的事实，并在同范围冲突时提升为写高亮。 */
static TZrBool semantic_reference_query_append_highlight_range(
        SZrState *state,
        SZrLspContext *context,
        const SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrArray *result,
        SZrFileRange referenceRange,
        EZrSemanticReferenceKind role) {
    SZrLspDocumentHighlight *highlight;
    SZrFileRange factRange;
    SZrLspRange range;
    TZrInt32 kind;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    factRange = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, referenceRange);
    if (factRange.source == ZR_NULL ||
        !ZrLanguageServer_Lsp_StringsEqual(factRange.source, uri)) {
        return ZR_FALSE;
    }
    range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, uri, factRange);
    kind = semantic_reference_query_highlight_kind(role);
    highlight = semantic_reference_query_find_highlight(result, range);
    if (highlight != ZR_NULL) {
        if (kind == 3) {
            highlight->kind = kind;
        }
        return ZR_TRUE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(
                state,
                result,
                sizeof(SZrLspDocumentHighlight *),
                ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }
    highlight = (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(
            state->global, sizeof(SZrLspDocumentHighlight));
    if (highlight == ZR_NULL) {
        return ZR_FALSE;
    }
    highlight->range = range;
    highlight->kind = kind;
    ZrCore_Array_Push(state, result, &highlight);
    return ZR_TRUE;
}

/* 将一般引用事实送入仅当前文档的高亮投影。 */
static TZrBool semantic_reference_query_append_highlight(
        SZrState *state,
        SZrLspContext *context,
        const SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrArray *result,
        const SZrSemanticReferenceFact *fact) {
    return fact != ZR_NULL &&
           semantic_reference_query_append_highlight_range(
                   state, context, analyzer, uri, result, fact->range, fact->kind);
}

/* 外部目标高亮依赖 provider 世代和完整身份，防止刷新后沿用旧目标的同名引用。 */
static TZrBool semantic_reference_query_append_external_highlights(
        SZrState *state,
        SZrLspContext *context,
        const SZrLspSemanticQuery *query,
        SZrArray *result) {
    SZrArray references = {0};
    const SZrParserSemanticSymbolQuery *target = &query->canonicalSymbol;
    TZrBool appended = ZR_FALSE;

    if (!ZrLanguageServer_LspExternalTargetIdentity_IsAvailable(target) ||
        (target->externalProviderGeneration != 0U &&
         target->externalProviderGeneration !=
                 context->semanticSnapshotProviderGeneration)) {
        return ZR_FALSE;
    }
    if (ZrParser_SemanticQuery_ExternalReferences(
                query->analyzer->semanticContext, ZR_NULL, &references)) {
        for (TZrSize index = 0U; index < references.length; index++) {
            const SZrParserSemanticExternalReferenceQuery *reference =
                    (const SZrParserSemanticExternalReferenceQuery *)ZrCore_Array_Get(
                            &references, index);
            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
                ZrCore_Array_Free(state, &references);
                return ZR_FALSE;
            }
            if (ZrLanguageServer_LspExternalTargetIdentity_MatchesReference(
                        target, reference)) {
                appended = semantic_reference_query_append_highlight_range(
                        state, context, query->analyzer, query->uri, result,
                        reference->referenceRange, reference->role) || appended;
            }
        }
    }
    if (references.isValid) {
        ZrCore_Array_Free(state, &references);
    }
    return appended;
}

/* 收集本语义上下文内的声明及已解析使用点；跨快照关系由上层合并。 */
static TZrBool semantic_reference_query_append_references_for_symbol_id(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        TZrSymbolId symbolId,
        TZrBool includeDeclaration,
        SZrArray *result,
        TZrBool *outAppended) {
    SZrArray references = {0};
    const SZrSemanticReferenceFact *declaration;
    TZrSize index;
    TZrBool appended = ZR_FALSE;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        result == ZR_NULL || outAppended == ZR_NULL) {
        return ZR_FALSE;
    }
    *outAppended = ZR_FALSE;
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }
    declaration = ZrParser_SemanticQuery_DeclarationOf(
            analyzer->semanticContext,
            symbolId,
            ZR_NULL);
    if (includeDeclaration && declaration != ZR_NULL) {
        appended = semantic_reference_query_append_location(
                state, context, analyzer, result, declaration);
    }
    if (ZrParser_SemanticQuery_ReferencesOf(
                analyzer->semanticContext,
                symbolId,
                ZR_NULL,
                &references)) {
        for (index = 0U; index < references.length; index++) {
            const SZrSemanticReferenceFact *const *slot =
                    (const SZrSemanticReferenceFact *const *)ZrCore_Array_Get(
                            &references, index);
            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(
                        context)) {
                ZrCore_Array_Free(state, &references);
                return ZR_FALSE;
            }
            if (slot == ZR_NULL || *slot == ZR_NULL ||
                (*slot)->kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
                !(*slot)->isResolved) {
                continue;
            }
            appended = semantic_reference_query_append_location(
                    state, context, analyzer, result, *slot) || appended;
        }
    }
    if (references.isValid) {
        ZrCore_Array_Free(state, &references);
    }
    *outAppended = appended;
    return ZR_TRUE;
}

/* 本地或规范化导入符号走同一引用事实查询，避免 AST 局部符号和规范身份混用。 */
TZrBool ZrLanguageServer_LspSemanticReferenceQuery_AppendReferences(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        TZrBool includeDeclaration,
        SZrArray *result) {
    TZrBool appended = ZR_FALSE;
    TZrSymbolId symbolId;

    if (query == ZR_NULL ||
        query->kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL) {
        return ZR_FALSE;
    }
    symbolId = semantic_reference_query_symbol_id(query);
    if (!semantic_reference_query_append_references_for_symbol_id(
                state,
                context,
                query->analyzer,
                symbolId,
                includeDeclaration,
                result,
                &appended)) {
        return ZR_FALSE;
    }
    return appended;
}

/* 高亮只返回请求文档中的引用；外部目标另按 provider 身份筛选。 */
TZrBool ZrLanguageServer_LspSemanticReferenceQuery_AppendHighlights(
        SZrState *state,
        SZrLspContext *context,
        SZrLspSemanticQuery *query,
        SZrArray *result) {
    SZrArray references = {0};
    const SZrSemanticReferenceFact *declaration;
    TZrSize index;
    TZrBool appended = ZR_FALSE;
    TZrSymbolId symbolId;

    if (state == ZR_NULL || context == ZR_NULL || query == ZR_NULL ||
        query->kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
        query->analyzer == ZR_NULL || query->analyzer->semanticContext == ZR_NULL ||
        query->uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    symbolId = semantic_reference_query_symbol_id(query);
    if (symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }
    if (query->canonicalSymbol.hasExternalTarget) {
        return semantic_reference_query_append_external_highlights(
                state, context, query, result);
    }
    declaration = ZrParser_SemanticQuery_DeclarationOf(
            query->analyzer->semanticContext,
            symbolId,
            ZR_NULL);
    if (declaration != ZR_NULL) {
        appended = semantic_reference_query_append_highlight(
                state,
                context,
                query->analyzer,
                query->uri,
                result,
                declaration);
    }
    if (ZrParser_SemanticQuery_ReferencesOf(
                query->analyzer->semanticContext,
                symbolId,
                ZR_NULL,
                &references)) {
        for (index = 0U; index < references.length; index++) {
            const SZrSemanticReferenceFact *const *slot =
                    (const SZrSemanticReferenceFact *const *)ZrCore_Array_Get(
                            &references, index);
            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(
                        context)) {
                ZrCore_Array_Free(state, &references);
                return ZR_FALSE;
            }
            if (slot == ZR_NULL || *slot == ZR_NULL ||
                (*slot)->kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
                !(*slot)->isResolved) {
                continue;
            }
            appended = semantic_reference_query_append_highlight(
                    state,
                    context,
                    query->analyzer,
                    query->uri,
                    result,
                    *slot) || appended;
        }
    }
    if (references.isValid) {
        ZrCore_Array_Free(state, &references);
    }
    return appended;
}
