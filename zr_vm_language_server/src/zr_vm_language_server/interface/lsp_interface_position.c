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

static TZrInt32 file_line_to_lsp_line(TZrInt32 fileLine) {
    return fileLine > 0 ? fileLine - 1 : 0;
}

static TZrInt32 lsp_line_to_file_line(TZrInt32 lspLine) {
    return lspLine + 1;
}

static TZrInt32 file_column_to_lsp_character(TZrInt32 fileColumn) {
    return fileColumn > 0 ? fileColumn - 1 : 0;
}

static TZrInt32 lsp_character_to_file_column(TZrInt32 lspCharacter) {
    return lspCharacter + 1;
}

// 转换 FileRange 到 LspRange
SZrLspRange ZrLanguageServer_LspRange_FromFileRange(SZrFileRange fileRange) {
    SZrLspRange lspRange;
    lspRange.start.line = file_line_to_lsp_line(fileRange.start.line);
    lspRange.start.character = file_column_to_lsp_character(fileRange.start.column);
    lspRange.end.line = file_line_to_lsp_line(fileRange.end.line);
    lspRange.end.character = file_column_to_lsp_character(fileRange.end.column);
    return lspRange;
}

// 辅助函数：从行号和列号计算偏移量
TZrSize ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(const TZrChar *content, TZrSize contentLength, 
                                                   TZrInt32 line, TZrInt32 column) {
    if (content == ZR_NULL) {
        return 0;
    }
    
    TZrSize offset = 0;
    TZrInt32 currentLine = 0;
    TZrInt32 currentColumn = 0;
    
    // 遍历内容直到到达目标行和列
    for (TZrSize i = 0; i < contentLength && currentLine < line; i++) {
        if (content[i] == '\n') {
            currentLine++;
            currentColumn = 0;
            offset = i + 1; // 跳过换行符
        } else {
            currentColumn++;
        }
    }
    
    // 如果已经到达目标行，添加列偏移
    if (currentLine == line) {
        // 在当前行中查找列位置
        TZrSize lineStart = offset;
        for (TZrSize i = lineStart; i < contentLength && currentColumn < column; i++) {
            if (content[i] == '\n') {
                break; // 到达行尾
            }
            currentColumn++;
            offset = i + 1;
        }
    }
    
    return offset;
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

// 转换 LspRange 到 FileRange
SZrFileRange ZrLanguageServer_LspRange_ToFileRange(SZrLspRange lspRange, SZrString *uri) {
    SZrFileRange fileRange;
    fileRange.start.line = lsp_line_to_file_line(lspRange.start.line);
    fileRange.start.column = lsp_character_to_file_column(lspRange.start.character);
    fileRange.start.offset = 0; // 需要文件内容才能计算，暂时设为0
    fileRange.end.line = lsp_line_to_file_line(lspRange.end.line);
    fileRange.end.column = lsp_character_to_file_column(lspRange.end.character);
    fileRange.end.offset = 0; // TODO: 需要文件内容才能计算，暂时设为0
    fileRange.source = uri;
    return fileRange;
}

// 转换 LspRange 到 FileRange（带文件内容）
SZrFileRange ZrLanguageServer_LspRange_ToFileRangeWithContent(SZrLspRange lspRange, SZrString *uri, 
                                                const TZrChar *content, TZrSize contentLength) {
    SZrFileRange fileRange;
    fileRange.start.line = lsp_line_to_file_line(lspRange.start.line);
    fileRange.start.column = lsp_character_to_file_column(lspRange.start.character);
    fileRange.start.offset = ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(content, contentLength, 
                                                                 lspRange.start.line,
                                                                 lspRange.start.character);
    fileRange.end.line = lsp_line_to_file_line(lspRange.end.line);
    fileRange.end.column = lsp_character_to_file_column(lspRange.end.character);
    fileRange.end.offset = ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(content, contentLength, 
                                                              lspRange.end.line,
                                                              lspRange.end.character);
    fileRange.source = uri;
    return fileRange;
}

// 转换 FilePosition 到 LspPosition
SZrLspPosition ZrLanguageServer_LspPosition_FromFilePosition(SZrFilePosition filePosition) {
    SZrLspPosition lspPosition;
    lspPosition.line = file_line_to_lsp_line(filePosition.line);
    lspPosition.character = file_column_to_lsp_character(filePosition.column);
    return lspPosition;
}

// 转换 LspPosition 到 FilePosition
SZrFilePosition ZrLanguageServer_LspPosition_ToFilePosition(SZrLspPosition lspPosition) {
    SZrFilePosition filePosition;
    filePosition.line = lsp_line_to_file_line(lspPosition.line);
    filePosition.column = lsp_character_to_file_column(lspPosition.character);
    filePosition.offset = 0; // TODO: 需要文件内容才能计算，暂时设为0
    return filePosition;
}

// 转换 LspPosition 到 FilePosition（带文件内容）
SZrFilePosition ZrLanguageServer_LspPosition_ToFilePositionWithContent(SZrLspPosition lspPosition,
                                                        const TZrChar *content, TZrSize contentLength) {
    SZrFilePosition filePosition;
    filePosition.line = lsp_line_to_file_line(lspPosition.line);
    filePosition.column = lsp_character_to_file_column(lspPosition.character);
    filePosition.offset = ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(content, contentLength, 
                                                              lspPosition.line, 
                                                              lspPosition.character);
    return filePosition;
}

// 创建 LSP 上下文
