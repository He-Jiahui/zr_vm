//
// Created by Auto on 2025/01/XX.
//

#include "interface/lsp_interface_internal.h"
#include "zr_vm_language_server/lsp_uri.h"

/** 为诊断和导航把解析器的字节范围投影到编辑器使用的 UTF-16 范围；偏移必须属于同一份内容。 */
SZrLspRange ZrLanguageServer_LspRange_FromFileRangeWithContent(SZrFileRange fileRange,
                                                               const TZrChar *content,
                                                               TZrSize contentLength) {
    SZrLspRange lspRange;
    lspRange.start = ZrLanguageServer_LspPosition_FromFilePositionWithContent(fileRange.start,
                                                                              content,
                                                                              contentLength);
    lspRange.end = ZrLanguageServer_LspPosition_FromFilePositionWithContent(fileRange.end,
                                                                            content,
                                                                            contentLength);
    return lspRange;
}

/** 供编辑器特性与语义查询统一解释 LSP 零基行、UTF-16 列。 */
TZrSize ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(const TZrChar *content,
                                                           TZrSize contentLength,
                                                           TZrInt32 line,
                                                           TZrInt32 column) {
    SZrLspPosition position;

    position.line = line;
    position.character = column;
    return ZrLanguageServer_LspPositionCodec_Utf16PositionToByteOffset(content, contentLength, position);
}

/** 为项目、元数据和工作区编辑提供统一的 file URI 路径边界；非 file URI 由下层拒绝。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_FileUriToNativePath(SZrString *uri,
                                                                        TZrChar *buffer,
                                                                        TZrSize bufferSize) {
    return ZrLanguageServer_LspUri_FileToNativePath(uri, buffer, bufferSize);
}

/** 将客户端选择范围绑定到解析器字节范围及借用的源 URI；调用者负责维持内容和 URI 有效。 */
SZrFileRange ZrLanguageServer_LspRange_ToFileRangeWithContent(SZrLspRange lspRange,
                                                              SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength) {
    SZrFileRange fileRange;
    fileRange.start = ZrLanguageServer_LspPosition_ToFilePositionWithContent(lspRange.start,
                                                                             content,
                                                                             contentLength);
    fileRange.end = ZrLanguageServer_LspPosition_ToFilePositionWithContent(lspRange.end,
                                                                           content,
                                                                           contentLength);
    fileRange.source = uri;
    return fileRange;
}

/** 把解析器字节偏移转为 LSP UTF-16 位置；文件坐标中的行、列不替代偏移。 */
SZrLspPosition ZrLanguageServer_LspPosition_FromFilePositionWithContent(SZrFilePosition filePosition,
                                                                        const TZrChar *content,
                                                                        TZrSize contentLength) {
    return ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(content,
                                                                       contentLength,
                                                                       filePosition.offset);
}

/** 把客户端 UTF-16 光标定位到解析器使用的字节偏移和一基文件坐标。 */
SZrFilePosition ZrLanguageServer_LspPosition_ToFilePositionWithContent(SZrLspPosition lspPosition,
                                                        const TZrChar *content, TZrSize contentLength) {
    TZrSize offset = ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(content,
                                                                        contentLength,
                                                                        lspPosition.line,
                                                                        lspPosition.character);
    return ZrLanguageServer_LspPositionCodec_ByteOffsetToFilePosition(content, contentLength, offset);
}
