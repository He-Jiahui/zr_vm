#include "semantic/lsp_canonical_completion.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_display.h"
#include "zr_vm_parser/semantic_query.h"

#include <stdio.h>
#include <string.h>

/* 补全项的类别与 hover 的展示词汇保持同一套语义，而不暴露 parser 枚举。 */
static const TZrChar *canonical_completion_kind_text(
        EZrSemanticSymbolKind kind) {
    switch (kind) {
        case ZR_SEMANTIC_SYMBOL_KIND_FUNCTION:
            return "function";
        case ZR_SEMANTIC_SYMBOL_KIND_TYPE:
            return "class";
        case ZR_SEMANTIC_SYMBOL_KIND_FIELD:
            return "field";
        case ZR_SEMANTIC_SYMBOL_KIND_PROPERTY:
            return "property";
        case ZR_SEMANTIC_SYMBOL_KIND_PARAMETER:
        case ZR_SEMANTIC_SYMBOL_KIND_VARIABLE:
        case ZR_SEMANTIC_SYMBOL_KIND_UNKNOWN:
        default:
            return "variable";
    }
}

/* 可见性查询可能带回非声明位置；这里只向编辑器推荐有稳定声明身份的候选。 */
static TZrBool canonical_completion_symbol_is_exact(
        const SZrParserSemanticSymbolQuery *symbol) {
    return symbol != ZR_NULL &&
           symbol->symbolId != ZR_SEMANTIC_ID_INVALID &&
           symbol->role == ZR_SEMANTIC_REFERENCE_DECLARATION &&
           symbol->declarationNode != ZR_NULL &&
           symbol->displayName != ZR_NULL;
}

/* 优先使用 parser 给出的完整签名；缺失时才从同一快照中的 canonical 类型补齐。 */
static const TZrChar *canonical_completion_detail(
        const SZrSemanticContext *semanticContext,
        const SZrParserSemanticSymbolQuery *symbol,
        TZrChar *typeBuffer,
        TZrSize typeBufferSize) {
    if (symbol->signatureDisplay != ZR_NULL &&
        ZrCore_String_GetByteLength(symbol->signatureDisplay) > 0U) {
        return ZrCore_String_GetNativeString(symbol->signatureDisplay);
    }
    if (ZrParser_CanonicalType_Format(
                semanticContext,
                symbol->typeId,
                typeBuffer,
                typeBufferSize)) {
        return typeBuffer;
    }
    return "cannot infer exact type";
}

/* 文档事实由 parser 的符号 ID 管理，返回值只在当前语义快照存活期间借用。 */
static SZrString *canonical_completion_documentation(
        SZrState *state,
        const SZrSemanticContext *semanticContext,
        const SZrParserSemanticSymbolQuery *symbol) {
    SZrString *documentation;

    if (state == ZR_NULL || semanticContext == ZR_NULL ||
        symbol == ZR_NULL || symbol->symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    documentation = ZrParser_SemanticQuery_DocumentationOfSymbol(
            semanticContext, symbol->symbolId);
    if (documentation == ZR_NULL) {
        return ZR_NULL;
    }
    return documentation;
}

/* 类型使用位置与声明共用 ID 时，额外展示本次实例化的类型，避免误把声明类型当成使用类型。 */
static const TZrChar *canonical_completion_type_use_detail(
        const SZrSemanticContext *semanticContext,
        const SZrParserSemanticSymbolQuery *symbol,
        const SZrParserSemanticSymbolQuery *typeUse,
        const TZrChar *detail,
        TZrChar *buffer,
        TZrSize bufferSize) {
    TZrChar typeText[ZR_LSP_TYPE_BUFFER_LENGTH];
    int length;

    if (typeUse->symbolId == ZR_SEMANTIC_ID_INVALID ||
        typeUse->symbolId != symbol->symbolId ||
        typeUse->role != ZR_SEMANTIC_REFERENCE_TYPE ||
        typeUse->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
        typeUse->typeId == symbol->typeId ||
        !ZrParser_CanonicalType_Format(
                semanticContext, typeUse->typeId, typeText, sizeof(typeText))) {
        return detail;
    }
    length = snprintf(buffer, bufferSize, "%s\nResolved Type: %s", detail, typeText);
    return length >= 0 && (TZrSize)length < bufferSize ? buffer : detail;
}

/* lsp_semantic_query 在结构化接收者补全无结果后调用此入口；失败与零候选都返回 false。 */
TZrBool ZrLanguageServer_LspCanonicalCompletion_AppendVisibleSymbols(
        SZrState *state,
        const SZrSemanticContext *semanticContext,
        SZrFileRange position,
        SZrArray *result) {
    SZrParserSemanticVisibleSymbolOptions options;
    SZrParserSemanticSymbolQuery typeUse;
    SZrArray symbols;
    TZrSize initialLength;

    if (state == ZR_NULL || semanticContext == ZR_NULL || result == ZR_NULL ||
        !result->isValid ||
        result->elementSize != sizeof(SZrCompletionItem *)) {
        return ZR_FALSE;
    }

    memset(&options, 0, sizeof(options));
    memset(&typeUse, 0, sizeof(typeUse));
    (void)ZrParser_SemanticQuery_SymbolAt(semanticContext, position, ZR_NULL, &typeUse);
    ZrCore_Array_Construct(&symbols);
    if (!ZrParser_SemanticQuery_VisibleSymbols(
                semanticContext,
                position,
                ZR_NULL,
                &options,
                &symbols)) {
        if (symbols.isValid) {
            ZrCore_Array_Free(state, &symbols);
        }
        return ZR_FALSE;
    }

    /* result 由上层持有；只判断本次是否增加候选，已有候选不算本函数成功。 */
    initialLength = result->length;
    for (TZrSize index = 0U; index < symbols.length; index++) {
        const SZrParserSemanticSymbolQuery *symbol =
                (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                        &symbols, index);
        TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
        TZrChar detailBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
        const TZrChar *label;
        const TZrChar *detail;
        SZrString *documentation;
        SZrCompletionItem *item;

        if (!canonical_completion_symbol_is_exact(symbol)) {
            continue;
        }
        label = ZrCore_String_GetNativeString(symbol->displayName);
        detail = canonical_completion_detail(
                semanticContext, symbol, typeBuffer, sizeof(typeBuffer));
        if (detail == ZR_NULL) {
            continue;
        }
        detail = canonical_completion_type_use_detail(
                semanticContext, symbol, &typeUse, detail, detailBuffer, sizeof(detailBuffer));
        documentation = canonical_completion_documentation(
                state, semanticContext, symbol);
        item = ZrLanguageServer_CompletionItem_New(
                state,
                label,
                canonical_completion_kind_text(symbol->kind),
                detail,
                documentation != ZR_NULL
                        ? ZrCore_String_GetNativeString(documentation)
                        : ZR_NULL,
                ZR_NULL);
        if (item != ZR_NULL) {
            ZrCore_Array_Push(state, result, &item);
        }
    }
    ZrCore_Array_Free(state, &symbols);
    return result->length > initialLength;
}
