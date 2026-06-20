#include "interface/lsp_position_codec.h"

static TZrSize lsp_position_codec_utf8_codepoint_length(const TZrChar *content,
                                                        TZrSize contentLength,
                                                        TZrSize offset) {
    unsigned char first;

    if (content == ZR_NULL || offset >= contentLength) {
        return 0;
    }

    first = (unsigned char)content[offset];
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
