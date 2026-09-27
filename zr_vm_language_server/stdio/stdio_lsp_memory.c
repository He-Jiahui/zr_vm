#include "zr_vm_language_server_stdio_internal.h"

/* 查询失败时直接回收位置数组；成功路径在 JSON 或工作区编辑消费结果后回收。
 * URI 是随 VM 状态管理的字符串引用，不由该数组释放器单独回收。 */
void free_locations_array(SZrState *state, SZrArray *locations) {
    TZrSize index;

    if (state == ZR_NULL || locations == ZR_NULL) {
        return;
    }

    for (index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *locationPtr, sizeof(SZrLspLocation));
        }
    }
    ZrCore_Array_Free(state, locations);
}

/* 文档与工作区符号查询共享这一释放边界；符号文本仍由 VM 字符串管理。 */
void free_symbols_array(SZrState *state, SZrArray *symbols) {
    TZrSize index;

    if (state == ZR_NULL || symbols == ZR_NULL) {
        return;
    }

    for (index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr =
            (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *symbolPtr, sizeof(SZrLspSymbolInformation));
        }
    }
    ZrCore_Array_Free(state, symbols);
}

/* 诊断包含 relatedInformation 与 fixes 子数组，复用 LSP 层析构契约避免遗漏。 */
void free_diagnostics_array(SZrState *state, SZrArray *diagnostics) {
    ZrLanguageServer_Lsp_FreeDiagnostics(state, diagnostics);
}

/* 补全与 resolve 查询失败时直接回收；成功时在序列化后归还原生项。 */
void free_completion_items_array(SZrState *state, SZrArray *items) {
    TZrSize index;

    if (state == ZR_NULL || items == ZR_NULL) {
        return;
    }

    for (index = 0; index < items->length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(items, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *itemPtr, sizeof(SZrLspCompletionItem));
        }
    }
    ZrCore_Array_Free(state, items);
}

/* inlayHint.label 是 VM 字符串；此处只处理 LSP 层的原生项与容器。 */
void free_inlay_hints_array(SZrState *state, SZrArray *hints) {
    TZrSize index;

    if (state == ZR_NULL || hints == ZR_NULL) {
        return;
    }

    for (index = 0; index < hints->length; index++) {
        SZrLspInlayHint **hintPtr = (SZrLspInlayHint **)ZrCore_Array_Get(hints, index);
        if (hintPtr != ZR_NULL && *hintPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *hintPtr, sizeof(SZrLspInlayHint));
        }
    }

    ZrCore_Array_Free(state, hints);
}

/* 文档高亮的范围是值字段；结果逐项释放后再释放数组缓冲区。 */
void free_highlights_array(SZrState *state, SZrArray *highlights) {
    TZrSize index;

    if (state == ZR_NULL || highlights == ZR_NULL) {
        return;
    }

    for (index = 0; index < highlights->length; index++) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, index);
        if (highlightPtr != ZR_NULL && *highlightPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *highlightPtr, sizeof(SZrLspDocumentHighlight));
        }
    }
    ZrCore_Array_Free(state, highlights);
}

/* hover.contents 是独立数组；内容字符串随 VM 状态管理，不归响应 JSON 所有。 */
void free_hover(SZrState *state, SZrLspHover *hover) {
    if (state == ZR_NULL || hover == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &hover->contents);
    ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
}

/* RichHover 的 sections 含原生分配项，委托 LSP 层统一析构。 */
void free_rich_hover(SZrState *state, SZrLspRichHover *hover) {
    ZrLanguageServer_Lsp_FreeRichHover(state, hover);
}

/* 签名帮助包含嵌套的签名和参数数组，复用 LSP 层完整析构。 */
void free_signature_help(SZrState *state, SZrLspSignatureHelp *help) {
    ZrLanguageServer_LspSignatureHelp_Free(state, help);
}
