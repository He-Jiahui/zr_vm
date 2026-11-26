#include "interface/lsp_position_codec.h"

#include <string.h>

#include "zr_vm_core/memory.h"

/** 保持畸形 UTF-8 的扫描边界局限于单字节，避免误吞随后换行或光标目标。 */
static TZrSize lsp_position_codec_utf8_codepoint_length(const TZrChar *content,
                                                        TZrSize contentLength,
                                                        TZrSize offset) {
    unsigned char first;

    if (content == ZR_NULL || offset >= contentLength) {
        return 0;
    }

    first = (unsigned char)content[offset];
    /* TODO: 这里只检查 continuation 形状，未拒绝 overlong、代理码点和超出 Unicode 上限的序列；
     * 需与文档 UTF-8 验证边界及客户端对畸形文本的 UTF-16 计数约定对齐。 */
    if (first < 0x80u) {
        return 1;
    }
    if ((first & 0xE0u) == 0xC0u &&
        offset + 1 < contentLength &&
        (((unsigned char)content[offset + 1]) & 0xC0u) == 0x80u) {
        return 2;
    }
    if ((first & 0xF0u) == 0xE0u &&
        offset + 2 < contentLength &&
        (((unsigned char)content[offset + 1]) & 0xC0u) == 0x80u &&
        (((unsigned char)content[offset + 2]) & 0xC0u) == 0x80u) {
        return 3;
    }
    if ((first & 0xF8u) == 0xF0u &&
        offset + 3 < contentLength &&
        (((unsigned char)content[offset + 1]) & 0xC0u) == 0x80u &&
        (((unsigned char)content[offset + 2]) & 0xC0u) == 0x80u &&
        (((unsigned char)content[offset + 3]) & 0xC0u) == 0x80u) {
        return 4;
    }

    return 1;
}

/** 将四字节码点计作两个 UTF-16 单元，使 emoji 后的编辑器列保持一致。 */
static TZrInt32 lsp_position_codec_utf16_units_for_utf8_codepoint(const TZrChar *content,
                                                                  TZrSize contentLength,
                                                                  TZrSize offset,
                                                                  TZrSize byteLength) {
    unsigned char first;
    TZrUInt32 codepoint;

    if (content == ZR_NULL || offset >= contentLength || byteLength == 0) {
        return 1;
    }

    first = (unsigned char)content[offset];
    if (byteLength == 4 && (first & 0xF8u) == 0xF0u) {
        codepoint = ((TZrUInt32)(first & 0x07u) << 18) |
                    ((TZrUInt32)((unsigned char)content[offset + 1] & 0x3Fu) << 12) |
                    ((TZrUInt32)((unsigned char)content[offset + 2] & 0x3Fu) << 6) |
                    (TZrUInt32)((unsigned char)content[offset + 3] & 0x3Fu);
        return codepoint >= 0x10000u ? 2 : 1;
    }

    return 1;
}

/** 请求入口使用的统一位置转换；在不完整代理对或行尾将光标收敛到可表示的字节边界。 */
TZrSize ZrLanguageServer_LspPositionCodec_Utf16PositionToByteOffset(const TZrChar *content,
                                                                    TZrSize contentLength,
                                                                    SZrLspPosition position) {
    TZrSize offset = 0;
    TZrInt32 currentLine = 0;
    TZrInt32 currentCharacter = 0;

    if (content == ZR_NULL || position.line < 0 || position.character < 0) {
        return 0;
    }

    while (offset < contentLength && currentLine < position.line) {
        TZrSize codepointLength;

        if (content[offset] == '\n') {
            currentLine++;
            currentCharacter = 0;
            offset++;
            continue;
        }

        codepointLength = lsp_position_codec_utf8_codepoint_length(content, contentLength, offset);
        offset += codepointLength > 0 ? codepointLength : 1;
    }

    if (currentLine != position.line) {
        return contentLength;
    }

    while (offset < contentLength && currentCharacter < position.character) {
        TZrSize codepointLength;
        TZrInt32 utf16Units;

        if (content[offset] == '\n' || content[offset] == '\r') {
            break;
        }

        codepointLength = lsp_position_codec_utf8_codepoint_length(content, contentLength, offset);
        utf16Units = lsp_position_codec_utf16_units_for_utf8_codepoint(content,
                                                                       contentLength,
                                                                       offset,
                                                                       codepointLength);
        if (currentCharacter + utf16Units > position.character) {
            break;
        }

        currentCharacter += utf16Units;
        offset += codepointLength > 0 ? codepointLength : 1;
    }

    return offset;
}

/** 响应出口使用的逆向转换；CRLF 作为单次换行边界，列数按 UTF-16 单元计算。 */
SZrLspPosition ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(const TZrChar *content,
                                                                           TZrSize contentLength,
                                                                           TZrSize offset) {
    SZrLspPosition position;
    TZrSize index = 0;

    position.line = 0;
    position.character = 0;

    if (content == ZR_NULL) {
        return position;
    }

    if (offset > contentLength) {
        offset = contentLength;
    }

    while (index < offset) {
        TZrSize codepointLength;

        if (content[index] == '\n') {
            position.line++;
            position.character = 0;
            index++;
            continue;
        }

        if (content[index] == '\r') {
            index++;
            continue;
        }

        codepointLength = lsp_position_codec_utf8_codepoint_length(content, contentLength, index);
        if (index + codepointLength > offset) {
            break;
        }

        position.character += lsp_position_codec_utf16_units_for_utf8_codepoint(content,
                                                                                contentLength,
                                                                                index,
                                                                                codepointLength);
        index += codepointLength > 0 ? codepointLength : 1;
    }

    return position;
}

/** 从已知行首开始的列计算，与 ByteOffsetToUtf16Position 的行内分支逐字节一致。 */
static TZrInt32 lsp_position_codec_utf16_column_from(const TZrChar *content,
                                                     TZrSize contentLength,
                                                     TZrSize index,
                                                     TZrSize offset) {
    TZrInt32 character = 0;

    while (index < offset) {
        TZrSize codepointLength;

        if (content[index] == '\r') {
            index++;
            continue;
        }
        codepointLength = lsp_position_codec_utf8_codepoint_length(content, contentLength, index);
        if (index + codepointLength > offset) {
            break;
        }
        character += lsp_position_codec_utf16_units_for_utf8_codepoint(content,
                                                                       contentLength,
                                                                       index,
                                                                       codepointLength);
        index += codepointLength > 0 ? codepointLength : 1;
    }
    return character;
}

/* 先计数换行再一次分配；UTF-8 continuation 字节不可能是 '\n'，所以按字节切分行与完整扫描的行数一致。 */
TZrBool ZrLanguageServer_LspLineIndex_Build(SZrState *state,
                                           SZrLspLineIndex *index,
                                           const TZrChar *content,
                                           TZrSize contentLength) {
    TZrSize newlines = 0;
    TZrSize line = 1;

    if (index == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(index, 0, sizeof(*index));
    if (state == ZR_NULL || content == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize i = 0; i < contentLength; i++) {
        newlines += content[i] == '\n' ? 1U : 0U;
    }
    index->lineStarts = (TZrSize *)ZrCore_Memory_RawMalloc(state->global, (newlines + 1U) * sizeof(TZrSize));
    if (index->lineStarts == ZR_NULL) {
        return ZR_FALSE;
    }
    index->lineStarts[0] = 0;
    for (TZrSize i = 0; i < contentLength; i++) {
        if (content[i] == '\n') {
            index->lineStarts[line++] = i + 1U;
        }
    }
    index->content = content;
    index->contentLength = contentLength;
    index->lineCount = newlines + 1U;
    return ZR_TRUE;
}

void ZrLanguageServer_LspLineIndex_Free(SZrState *state, SZrLspLineIndex *index) {
    if (index == ZR_NULL) {
        return;
    }
    if (state != ZR_NULL && index->lineStarts != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, index->lineStarts, index->lineCount * sizeof(TZrSize));
    }
    memset(index, 0, sizeof(*index));
}

SZrLspPosition ZrLanguageServer_LspLineIndex_ByteOffsetToUtf16Position(const SZrLspLineIndex *index,
                                                                       const TZrChar *content,
                                                                       TZrSize contentLength,
                                                                       TZrSize offset) {
    SZrLspPosition position;
    TZrSize low;
    TZrSize high;

    if (index == ZR_NULL || index->lineStarts == ZR_NULL || content == ZR_NULL ||
        index->content != content || index->contentLength != contentLength) {
        return ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(content, contentLength, offset);
    }
    if (offset > contentLength) {
        offset = contentLength;
    }
    /* 最后一个 lineStart <= offset 的行；lineStarts[k] = 第 k 个 '\n' 之后，故恰等于 offset 之前的换行数。 */
    low = 0;
    high = index->lineCount - 1U;
    while (low < high) {
        TZrSize middle = low + (high - low + 1U) / 2U;
        if (index->lineStarts[middle] <= offset) {
            low = middle;
        } else {
            high = middle - 1U;
        }
    }
    position.line = (TZrInt32)low;
    position.character = lsp_position_codec_utf16_column_from(content, contentLength, index->lineStarts[low], offset);
    return position;
}

/** 供语义分析器按字节列定位，同时保留 LSP 转换后的最终偏移。 */
SZrFilePosition ZrLanguageServer_LspPositionCodec_ByteOffsetToFilePosition(const TZrChar *content,
                                                                           TZrSize contentLength,
                                                                           TZrSize offset) {
    TZrInt32 line = 1;
    TZrSize lineStart = 0;
    TZrSize index;

    if (content == ZR_NULL) {
        return ZrParser_FilePosition_Create(offset, 1, 1);
    }

    if (offset > contentLength) {
        offset = contentLength;
    }

    for (index = 0; index < offset; index++) {
        if (content[index] == '\n') {
            line++;
            lineStart = index + 1;
        }
    }

    return ZrParser_FilePosition_Create(offset, line, (TZrInt32)(offset - lineStart + 1));
}
