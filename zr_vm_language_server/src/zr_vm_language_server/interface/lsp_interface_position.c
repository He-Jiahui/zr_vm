//
// Created by Auto on 2025/01/XX.
//

#include "interface/lsp_interface_internal.h"
#include "zr_vm_language_server/lsp_uri.h"

// 转换 FileRange 到 LspRange（带文件内容）
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

// 辅助函数：从行号和列号计算偏移量
TZrSize ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(const TZrChar *content,
                                                           TZrSize contentLength,
                                                           TZrInt32 line,
                                                           TZrInt32 column) {
    SZrLspPosition position;

    position.line = line;
    position.character = column;
    return ZrLanguageServer_LspPositionCodec_Utf16PositionToByteOffset(content, contentLength, position);
}

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_FileUriToNativePath(SZrString *uri,
                                                                        TZrChar *buffer,
                                                                        TZrSize bufferSize) {
    return ZrLanguageServer_LspUri_FileToNativePath(uri, buffer, bufferSize);
}

// 转换 LspRange 到 FileRange（带文件内容）
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

// 转换 FilePosition 到 LspPosition（带文件内容）
SZrLspPosition ZrLanguageServer_LspPosition_FromFilePositionWithContent(SZrFilePosition filePosition,
                                                                        const TZrChar *content,
                                                                        TZrSize contentLength) {
    return ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(content,
                                                                       contentLength,
                                                                       filePosition.offset);
}

// 转换 LspPosition 到 FilePosition（带文件内容）
SZrFilePosition ZrLanguageServer_LspPosition_ToFilePositionWithContent(SZrLspPosition lspPosition,
                                                        const TZrChar *content, TZrSize contentLength) {
    TZrSize offset = ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(content,
                                                                        contentLength,
                                                                        lspPosition.line,
                                                                        lspPosition.character);
    return ZrLanguageServer_LspPositionCodec_ByteOffsetToFilePosition(content, contentLength, offset);
}

// 创建 LSP 上下文
