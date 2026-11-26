#ifndef ZR_VM_LANGUAGE_SERVER_LSP_POSITION_CODEC_H
#define ZR_VM_LANGUAGE_SERVER_LSP_POSITION_CODEC_H

#include "zr_vm_language_server/lsp_interface.h"

/** 客户端的零基 UTF-16 位置转换为当前 UTF-8 快照偏移；越界位置收敛到可用边界。 */
ZR_LANGUAGE_SERVER_API TZrSize ZrLanguageServer_LspPositionCodec_Utf16PositionToByteOffset(const TZrChar *content,
                                                                    TZrSize contentLength,
                                                                    SZrLspPosition position);
/** 解析器字节偏移投影回 LSP UTF-16 位置；结果用于诊断、导航、格式化与语义 token。 */
ZR_LANGUAGE_SERVER_API SZrLspPosition ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(const TZrChar *content,
                                                                           TZrSize contentLength,
                                                                           TZrSize offset);
/** @brief 一次请求内复用的行首偏移索引：把逐 token 的 ByteOffsetToUtf16Position 从“从文件开头扫描”
 *  降为“二分定位行 + 只扫描该行前缀”，结果与 ByteOffsetToUtf16Position 完全一致。
 *  @note 借用 content，不拥有；lineStarts 由 Build/Free 使用同一 state 分配与释放。content 与构建时不同时
 *        查询自动退回完整扫描，因此索引失效不会产生错误位置。 */
typedef struct SZrLspLineIndex {
    const TZrChar *content;
    TZrSize contentLength;
    TZrSize *lineStarts;
    TZrSize lineCount;
} SZrLspLineIndex;

/** @brief 为 content 建立行首索引；分配失败时返回 false，索引保持为空（查询退回完整扫描）。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspLineIndex_Build(SZrState *state,
                                                                  SZrLspLineIndex *index,
                                                                  const TZrChar *content,
                                                                  TZrSize contentLength);
/** @brief 释放 Build 分配的行首数组并清空索引；可重复调用。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspLineIndex_Free(SZrState *state, SZrLspLineIndex *index);
/** @brief 与 ByteOffsetToUtf16Position 语义相同（CRLF、畸形 UTF-8、越界收敛）；index 为空或不匹配
 *  content/contentLength 时直接调用完整扫描版本。 */
ZR_LANGUAGE_SERVER_API SZrLspPosition ZrLanguageServer_LspLineIndex_ByteOffsetToUtf16Position(
        const SZrLspLineIndex *index,
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize offset);

/** 为解析器查询生成一基行及字节列；输入偏移须对应同一内容快照。 */
SZrFilePosition ZrLanguageServer_LspPositionCodec_ByteOffsetToFilePosition(const TZrChar *content,
                                                                           TZrSize contentLength,
                                                                           TZrSize offset);

#endif
