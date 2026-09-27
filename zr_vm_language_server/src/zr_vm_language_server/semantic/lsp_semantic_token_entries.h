#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TOKEN_ENTRIES_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_TOKEN_ENTRIES_H

#include "semantic/lsp_semantic_token_canonical.h"
#include "interface/lsp_interface_internal.h"

/** @brief 编码前的绝对 UTF-16 坐标；同一位置的候选先合并，再转换为 LSP 相对五元组。 */
typedef struct SZrLspSemanticTokenEntry {
    TZrUInt32 line;
    TZrUInt32 character;
    TZrUInt32 length;
    TZrUInt32 typeIndex;
    TZrUInt32 modifiers;
} SZrLspSemanticTokenEntry;

/** @brief 汇合声明事实与文本扫描候选；同跨度冲突按语义类型优先级裁决。 */
void ZrLanguageServer_LspSemanticTokenEntries_Add(
        SZrState *state,
        SZrArray *entries,
        TZrUInt32 line,
        TZrUInt32 character,
        TZrUInt32 length,
        TZrUInt32 typeIndex,
        TZrUInt32 modifiers);

/**
 * @brief 将当前文档的单行字节区间转成 UTF-16 token；跨行或无效跨度不提交。
 * @pre content 必须与传入字节偏移所依据的快照一致。
 */
TZrBool ZrLanguageServer_LspSemanticTokenEntries_AddUtf16Span(
        SZrState *state,
        SZrArray *entries,
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize startOffset,
        TZrSize endOffset,
        TZrUInt32 typeIndex,
        TZrUInt32 modifiers);

/** @brief 将 parser 的源文件范围投影到指定文档的 LSP 坐标，供声明 token 使用。 */
void ZrLanguageServer_LspSemanticTokenEntries_AddFileRange(
        SZrState *state,
        SZrLspContext *context,
        SZrArray *entries,
        SZrString *uri,
        SZrFileRange range,
        TZrUInt32 typeIndex,
        TZrUInt32 modifiers);

/**
 * @brief 排序并消除重叠候选，将最终 token 追加为协议规定的五元组数据。
 * @pre entries 是可写数组，调用会重排并缩短它；result 已初始化且由调用方释放。
 */
void ZrLanguageServer_LspSemanticTokenEntries_AppendEncoded(
        SZrState *state,
        SZrArray *entries,
        SZrArray *result);

#endif
