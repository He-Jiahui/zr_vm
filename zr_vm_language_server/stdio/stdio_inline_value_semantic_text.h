#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_INLINE_VALUE_SEMANTIC_TEXT_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_INLINE_VALUE_SEMANTIC_TEXT_H

#include "zr_vm_language_server_stdio_internal.h"

cJSON *ZrStdioInlineValue_CreateSemanticTextForLspRange(SZrStdioServer *server,
                                                        SZrString *uri,
                                                        SZrLspRange range,
                                                        SZrLspPosition queryPosition);

#endif
