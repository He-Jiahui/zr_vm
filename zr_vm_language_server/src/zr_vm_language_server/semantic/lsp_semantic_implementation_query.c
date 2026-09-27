#include "semantic/lsp_semantic_implementation_query.h"
#include "semantic/semantic_analyzer_query_source.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_query.h"

/* 位置去重比较编辑器最终可见的范围，而不是未绑定文档的解析器偏移。 */
static TZrBool semantic_implementation_ranges_equal(
        SZrLspRange left,
        SZrLspRange right) {
    return left.start.line == right.start.line &&
           left.start.character == right.start.character &&
           left.end.line == right.end.line &&
           left.end.character == right.end.character;
}

/* 同一关系可由多个语义事实指向，先检查结果以免导航列表出现重复入口。 */
static TZrBool semantic_implementation_has_location(
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
            semantic_implementation_ranges_equal((*slot)->range, range)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 追加拥有独立 Location 对象的导航项；URI 借用分析快照中的字符串。 */
static TZrBool semantic_implementation_append_location(
        SZrState *state,
        SZrLspContext *context,
        SZrArray *result,
        SZrString *uri,
        SZrFileRange range) {
    SZrLspLocation *location;
    SZrLspRange lspRange;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL ||
        uri == ZR_NULL) {
        return ZR_FALSE;
    }
    lspRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context, uri, range);
    if (semantic_implementation_has_location(result, uri, lspRange)) {
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
    location->uri = uri;
    location->range = lspRange;
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

/* editor implementation 请求先规范化光标符号，再只展示本模块内可定位的实现/覆盖关系。 */
TZrBool ZrLanguageServer_LspSemanticImplementationQuery_Append(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        SZrLspPosition position,
        SZrArray *result) {
    SZrLspSemanticQuery query;
    SZrParserSemanticQueryScope scope;
    SZrArray relations = {0};
    TZrSize index;
    TZrBool appended = ZR_FALSE;
    TZrSymbolId symbolId;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        result == ZR_NULL ||
        ZrLanguageServer_LspContext_IsRequestCancellationRequested(context)) {
        return ZR_FALSE;
    }
    ZrLanguageServer_LspSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspSemanticQuery_ResolveAtPosition(
                state, context, uri, position, &query) ||
        query.kind != ZR_LSP_SEMANTIC_QUERY_TARGET_LOCAL_SYMBOL ||
        query.analyzer == ZR_NULL ||
        query.analyzer->semanticContext == ZR_NULL) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        return ZR_FALSE;
    }
    symbolId = query.hasCanonicalSymbol &&
                       (query.symbol == ZR_NULL ||
                        query.symbol->semanticId == query.canonicalSymbol.symbolId)
            ? query.canonicalSymbol.symbolId
            : query.symbol != ZR_NULL
                    ? query.symbol->semanticId
                    : ZR_SEMANTIC_ID_INVALID;
    if (symbolId == ZR_SEMANTIC_ID_INVALID) {
        ZrLanguageServer_LspSemanticQuery_Free(state, &query);
        return ZR_FALSE;
    }

    /* TODO: 此处仅查询当前 analyzer 的 module 事实；项目内其他文档的实现关系
     * 是否需并入 editor implementation，请沿项目索引及跨快照引用路径核查。 */
    ZrParser_SemanticQueryScope_Module(&scope);
    if (ZrParser_SemanticQuery_ImplementationsOf(
                query.analyzer->semanticContext,
                symbolId,
                &scope,
                &relations)) {
        for (index = 0U; index < relations.length; index++) {
            const SZrParserSemanticRelationQuery *relation =
                    (const SZrParserSemanticRelationQuery *)ZrCore_Array_Get(
                            &relations, index);
            SZrFileRange sourceRange;

            if (ZrLanguageServer_LspContext_IsRequestCancellationRequested(
                        context)) {
                appended = ZR_FALSE;
                break;
            }
            if (relation == ZR_NULL || !relation->hasSourceRange) {
                continue;
            }
            sourceRange = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
                    query.analyzer, relation->sourceRange);
            if (sourceRange.source == ZR_NULL) {
                continue;
            }
            appended = semantic_implementation_append_location(
                    state,
                    context,
                    result,
                    sourceRange.source,
                    sourceRange) || appended;
        }
    }
    if (relations.isValid) {
        ZrCore_Array_Free(state, &relations);
    }
    ZrLanguageServer_LspSemanticQuery_Free(state, &query);
    return appended;
}
