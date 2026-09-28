#include "interface/lsp_interface_internal.h"

#include <string.h>

/**
 * @brief 将插件描述符的结构坐标用于定义、引用和高亮位置。
 * @note 描述符没有可供字节偏移重算的源文本；调用方必须仅传一基且顺序有效的范围。
 */
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
