#include "interface/lsp_interface_internal.h"

#include <string.h>

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_TryRangeFromDescriptorMetadataCoordinates(
        SZrFileRange range,
        SZrLspRange *outRange) {
    if (outRange != ZR_NULL) {
        memset(outRange, 0, sizeof(*outRange));
    }
    if (outRange == ZR_NULL || range.start.line < 1 || range.start.column < 1 || range.end.column < 1 ||
        range.end.line < range.start.line ||
        (range.end.line == range.start.line && range.end.column < range.start.column)) {
        return ZR_FALSE;
    }

    outRange->start.line = range.start.line - 1;
    outRange->start.character = range.start.column - 1;
    outRange->end.line = range.end.line - 1;
    outRange->end.character = range.end.column - 1;
    return ZR_TRUE;
}
