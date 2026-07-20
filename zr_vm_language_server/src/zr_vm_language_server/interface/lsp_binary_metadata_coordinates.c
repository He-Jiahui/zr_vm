#include "interface/lsp_interface_internal.h"

#include <limits.h>
#include <string.h>

static TZrBool binary_metadata_position_to_byte_offset(const TZrChar *content,
                                                        TZrSize contentLength,
                                                        SZrFilePosition position,
                                                        TZrSize *outOffset) {
    TZrSize lineStart = 0;
    TZrSize lineEnd;
    TZrInt32 currentLine = 1;
    TZrSize columnOffset;

    if (content == ZR_NULL || outOffset == ZR_NULL || position.line < 1 || position.column < 1) {
        return ZR_FALSE;
    }

    while (currentLine < position.line) {
        while (lineStart < contentLength && content[lineStart] != '\n') {
            lineStart++;
        }
        if (lineStart >= contentLength) {
            return ZR_FALSE;
        }
        lineStart++;
        currentLine++;
    }

    lineEnd = lineStart;
    while (lineEnd < contentLength && content[lineEnd] != '\n' && content[lineEnd] != '\r') {
        lineEnd++;
    }

    columnOffset = (TZrSize)(position.column - 1);
    if (columnOffset > lineEnd - lineStart) {
        return ZR_FALSE;
    }

    *outOffset = lineStart + columnOffset;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_TryRangeFromBinaryMetadataCoordinates(SZrLspContext *context,
                                                                   SZrString *uri,
                                                                   SZrFileRange range,
                                                                   SZrLspRange *outRange) {
    SZrString *rangeUri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot;
    TZrSize startOffset;
    TZrSize endOffset;
    SZrFileRange offsetRange;

    if (outRange != ZR_NULL) {
        memset(outRange, 0, sizeof(*outRange));
    }
    if (outRange == ZR_NULL || range.start.line < 1 || range.start.column < 1 || range.end.column < 1 ||
        range.end.line < range.start.line ||
        (range.end.line == range.start.line && range.end.column < range.start.column)) {
        return ZR_FALSE;
    }

    rangeUri = range.source != ZR_NULL ? range.source : uri;
    if (context != ZR_NULL && rangeUri != ZR_NULL) {
        fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, rangeUri);
        if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(context->state, fileVersion, &snapshot)) {
            TZrBool hasOffsets = binary_metadata_position_to_byte_offset(snapshot.content,
                                                                         snapshot.contentLength,
                                                                         range.start,
                                                                         &startOffset) &&
                                  binary_metadata_position_to_byte_offset(snapshot.content,
                                                                         snapshot.contentLength,
                                                                         range.end,
                                                                         &endOffset) &&
                                  startOffset <= endOffset;
            if (hasOffsets) {
                offsetRange = range;
                offsetRange.start.offset = startOffset;
                offsetRange.end.offset = endOffset;
                *outRange = ZrLanguageServer_LspRange_FromFileRangeWithContent(offsetRange,
                                                                               snapshot.content,
                                                                               snapshot.contentLength);
            }
            ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
            return hasOffsets;
        }
    }

    // Binary artifacts do not embed source text, so only their stored one-based coordinates remain here.
    outRange->start.line = range.start.line - 1;
    outRange->start.character = range.start.column - 1;
    outRange->end.line = range.end.line - 1;
    outRange->end.character = range.end.column - 1;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_TryFilePositionFromBinaryMetadataCoordinates(SZrLspContext *context,
                                                                          SZrString *uri,
                                                                          SZrLspPosition position,
                                                                          SZrFilePosition *outPosition) {
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot;

    if (outPosition != ZR_NULL) {
        *outPosition = ZrParser_FilePosition_Create(0, 0, 0);
    }
    if (outPosition == ZR_NULL || position.line < 0 || position.character < 0 ||
        position.line == INT_MAX || position.character == INT_MAX) {
        return ZR_FALSE;
    }

    if (context != ZR_NULL && uri != ZR_NULL) {
        fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
        if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(context->state, fileVersion, &snapshot)) {
            *outPosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(position,
                                                                                  snapshot.content,
                                                                                  snapshot.contentLength);
            ZrLanguageServer_FileVersionContentSnapshot_Free(context->state, &snapshot);
            return ZR_TRUE;
        }
    }

    *outPosition = ZrParser_FilePosition_Create(0, position.line + 1, position.character + 1);
    return ZR_TRUE;
}
