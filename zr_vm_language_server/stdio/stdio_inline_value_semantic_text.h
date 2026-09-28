#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_INLINE_VALUE_SEMANTIC_TEXT_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_INLINE_VALUE_SEMANTIC_TEXT_H

#include "zr_vm_language_server_stdio_internal.h"

/**
 * @brief 在文档语义快照上查询指定位置的局部事实，构造成 InlineValueText。
 * @pre range 为内部 UTF-16 坐标且非空；queryPosition 指向同一 URI 快照中的表达式。
 * @return 调用方接管 cJSON 所有权；无事实、查询失败或组装失败时返回 NULL。
 */
cJSON *ZrStdioInlineValue_CreateSemanticTextForLspRange(SZrStdioServer *server,
                                                        SZrString *uri,
                                                        SZrLspRange range,
                                                        SZrLspPosition queryPosition);

#endif
