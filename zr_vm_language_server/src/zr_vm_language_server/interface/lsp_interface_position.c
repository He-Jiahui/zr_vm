//
// Created by Auto on 2025/01/XX.
//

#include "interface/lsp_interface_internal.h"

static TZrInt32 lsp_hex_digit_value(TZrChar value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }

    value = (TZrChar)tolower((unsigned char)value);
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }

    return -1;
}

static TZrBool lsp_uri_supports_native_path(const TZrChar *uriText, TZrSize uriLength) {
    TZrSize index;

    if (uriText == ZR_NULL || uriLength == 0) {
        return ZR_FALSE;
    }

    if (uriLength >= 7 && memcmp(uriText, "file://", 7) == 0) {
        return ZR_TRUE;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    if (uriLength >= 2 && isalpha((unsigned char)uriText[0]) && uriText[1] == ':') {
        return ZR_TRUE;
    }
#endif

    if (uriText[0] == '/' || uriText[0] == '\\' || uriText[0] == '.') {
        return ZR_TRUE;
    }

    for (index = 0; index < uriLength; index++) {
        if (uriText[index] == ':' && index + 2 < uriLength &&
            uriText[index + 1] == '/' && uriText[index + 2] == '/') {
            return ZR_FALSE;
        }

        if (uriText[index] == ':' || uriText[index] == '/' || uriText[index] == '\\') {
            break;
        }
    }

    return ZR_TRUE;
}

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
    const TZrChar *uriText;
    TZrSize uriLength;
    TZrSize readIndex = 0;
    TZrSize writeIndex = 0;

    if (uri == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    uriText = uri->shortStringLength < ZR_VM_LONG_STRING_FLAG
                  ? ZrCore_String_GetNativeStringShort(uri)
                  : ZrCore_String_GetNativeString(uri);
    if (uriText == ZR_NULL) {
        return ZR_FALSE;
    }

    uriLength = strlen(uriText);
    if (!lsp_uri_supports_native_path(uriText, uriLength)) {
        return ZR_FALSE;
    }

    if (uriLength >= 7 && memcmp(uriText, "file://", 7) == 0) {
        readIndex = 7;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    if (readIndex < uriLength && uriText[readIndex] == '/' && readIndex + 1 < uriLength &&
        isalpha((unsigned char)uriText[readIndex + 1])) {
        TZrBool hasDriveSeparator = ZR_FALSE;

        if (readIndex + 2 < uriLength && uriText[readIndex + 2] == ':') {
            hasDriveSeparator = ZR_TRUE;
        } else if (readIndex + 4 < uriLength && uriText[readIndex + 2] == '%' &&
                   uriText[readIndex + 3] == '3' &&
                   ((TZrChar)tolower((unsigned char)uriText[readIndex + 4])) == 'a') {
            hasDriveSeparator = ZR_TRUE;
        }

        if (hasDriveSeparator) {
            readIndex++;
        }
    }
#endif

    while (readIndex < uriLength && writeIndex + 1 < bufferSize) {
        TZrChar current = uriText[readIndex];

        if (current == '%' && readIndex + 2 < uriLength) {
            TZrInt32 high = lsp_hex_digit_value(uriText[readIndex + 1]);
            TZrInt32 low = lsp_hex_digit_value(uriText[readIndex + 2]);
            if (high >= 0 && low >= 0) {
                current = (TZrChar)((high << 4) | low);
                readIndex += 3;
            } else {
                readIndex++;
            }
        } else {
            readIndex++;
        }

#ifdef ZR_VM_PLATFORM_IS_WIN
        buffer[writeIndex++] = current == '/' ? '\\' : current;
#else
        buffer[writeIndex++] = current;
#endif
    }

    buffer[writeIndex] = '\0';
    return writeIndex > 0;
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
