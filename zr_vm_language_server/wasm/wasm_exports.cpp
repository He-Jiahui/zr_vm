//
// Created by Auto on 2025/01/XX.
// WASM 导出函数实现（使用 EMSCRIPTEN_BINDINGS）
//

// 在 C++ 模式下，用 extern "C" 包装 C 头文件以避免冲突
#ifdef __cplusplus
extern "C" {
// 定义宏来避免 C++ 关键字冲突
#define class class_
#endif

#include "wasm_exports.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/array.h"
#include "zr_vm_parser/location.h"

// 包含 LSP 接口头文件（必须在 extern "C" 块内）
#include "zr_vm_language_server/lsp_interface.h"

#ifdef __cplusplus
#undef class
}
#endif

#include "cJSON/cJSON.h"

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#ifdef ZR_WASM_BUILD

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// 全局状态（WASM 中需要全局状态）
static SZrGlobalState *g_wasm_global = ZR_NULL;
static SZrState *g_wasm_state = ZR_NULL;

// WASM 内存分配器（使用标准 malloc/free）
// 注意：FZrAllocator 的最后一个参数是 TInt64，不是 EZrMemoryNativeType
static TZrPtr wasm_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);
    
    if (newSize == 0) {
        // 释放内存
        if (pointer != ZR_NULL) {
            free(pointer);
        }
        return ZR_NULL;
    }
    
    if (pointer == ZR_NULL) {
        // 分配新内存
        return malloc(newSize);
    } else {
        // 重新分配内存
        return realloc(pointer, newSize);
    }
}

// 初始化全局状态（在第一次调用时初始化）
static void init_wasm_state(void) {
    if (g_wasm_global == ZR_NULL) {
        // 使用 WASM 分配器和回调创建全局状态
        SZrCallbackGlobal callbacks = {0};
        g_wasm_global = ZrGlobalStateNew(wasm_allocator, ZR_NULL, 0, &callbacks);
        if (g_wasm_global != ZR_NULL) {
            g_wasm_state = g_wasm_global->mainThreadState;
            // 初始化注册表
            if (g_wasm_state != ZR_NULL) {
                ZrGlobalStateInitRegistry(g_wasm_state, g_wasm_global);
            }
        }
    }
}

// JSON 序列化辅助函数

// 序列化 LSP 位置
static cJSON* serialize_lsp_position(SZrLspPosition pos) {
    cJSON *json = cJSON_CreateObject();
    if (json != ZR_NULL) {
        cJSON_AddNumberToObject(json, "line", pos.line);
        cJSON_AddNumberToObject(json, "character", pos.character);
    }
    return json;
}

// 序列化 LSP 范围
static cJSON* serialize_lsp_range(SZrLspRange range) {
    cJSON *json = cJSON_CreateObject();
    if (json != ZR_NULL) {
        cJSON *start = serialize_lsp_position(range.start);
        cJSON *end = serialize_lsp_position(range.end);
        cJSON_AddItemToObject(json, "start", start);
        cJSON_AddItemToObject(json, "end", end);
    }
    return json;
}

// 序列化字符串（从 SZrString 到 C 字符串）
static const char* string_to_cstr(SZrState *state, SZrString *str) {
    if (str == ZR_NULL || state == ZR_NULL) {
        return ZR_NULL;
    }
    
    TNativeString nativeStr;
    TZrSize len;
    if (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nativeStr = ZrStringGetNativeStringShort(str);
        len = str->shortStringLength;
    } else {
        nativeStr = ZrStringGetNativeString(str);
        len = str->longStringLength;
    }
    
    // 分配内存并复制字符串
    TChar *cstr = (TChar *)ZrMemoryRawMalloc(state->global, (len + 1) * sizeof(TChar));
    if (cstr != ZR_NULL) {
        memcpy(cstr, nativeStr, len * sizeof(TChar));
        cstr[len] = '\0';
    }
    return cstr;
}

// 从 C 字符串创建 SZrString
static SZrString* cstr_to_string(SZrState *state, const char *cstr, int len) {
    if (state == ZR_NULL || cstr == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrStringCreate(state, (TNativeString)cstr, (TZrSize)len);
}

// 序列化诊断数组
static cJSON* serialize_diagnostics(SZrState *state, SZrArray *diagnostics) {
    cJSON *json = cJSON_CreateArray();
    if (json == ZR_NULL || state == ZR_NULL || diagnostics == ZR_NULL) {
        return json;
    }
    
    for (TZrSize i = 0; i < diagnostics->length; i++) {
        SZrLspDiagnostic **diagPtr = (SZrLspDiagnostic **)ZrArrayGet(diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            SZrLspDiagnostic *diag = *diagPtr;
            cJSON *diagJson = cJSON_CreateObject();
            if (diagJson != ZR_NULL) {
                cJSON *range = serialize_lsp_range(diag->range);
                cJSON_AddItemToObject(diagJson, "range", range);
                cJSON_AddNumberToObject(diagJson, "severity", diag->severity);
                
                if (diag->code != ZR_NULL) {
                    const char *codeStr = string_to_cstr(state, diag->code);
                    if (codeStr != ZR_NULL) {
                        cJSON_AddStringToObject(diagJson, "code", codeStr);
                        ZrMemoryRawFree(state->global, (void*)codeStr, 0);
                    }
                }
                
                if (diag->message != ZR_NULL) {
                    const char *msgStr = string_to_cstr(state, diag->message);
                    if (msgStr != ZR_NULL) {
                        cJSON_AddStringToObject(diagJson, "message", msgStr);
                        ZrMemoryRawFree(state->global, (void*)msgStr, 0);
                    }
                }
                
                cJSON_AddItemToArray(json, diagJson);
            }
        }
    }
    
    return json;
}

// 序列化补全项数组
static cJSON* serialize_completions(SZrState *state, SZrArray *completions) {
    cJSON *json = cJSON_CreateArray();
    if (json == ZR_NULL || state == ZR_NULL || completions == ZR_NULL) {
        return json;
    }
    
    for (TZrSize i = 0; i < completions->length; i++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrArrayGet(completions, i);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            SZrLspCompletionItem *item = *itemPtr;
            cJSON *itemJson = cJSON_CreateObject();
            if (itemJson != ZR_NULL) {
                if (item->label != ZR_NULL) {
                    const char *labelStr = string_to_cstr(state, item->label);
                    if (labelStr != ZR_NULL) {
                        cJSON_AddStringToObject(itemJson, "label", labelStr);
                        ZrMemoryRawFree(state->global, (void*)labelStr, 0);
                    }
                }
                
                cJSON_AddNumberToObject(itemJson, "kind", item->kind);
                
                if (item->detail != ZR_NULL) {
                    const char *detailStr = string_to_cstr(state, item->detail);
                    if (detailStr != ZR_NULL) {
                        cJSON_AddStringToObject(itemJson, "detail", detailStr);
                        ZrMemoryRawFree(state->global, (void*)detailStr, 0);
                    }
                }
                
                if (item->documentation != ZR_NULL) {
                    const char *docStr = string_to_cstr(state, item->documentation);
                    if (docStr != ZR_NULL) {
                        cJSON *docJson = cJSON_CreateObject();
                        cJSON_AddStringToObject(docJson, "kind", "markdown");
                        cJSON_AddStringToObject(docJson, "value", docStr);
                        cJSON_AddItemToObject(itemJson, "documentation", docJson);
                        ZrMemoryRawFree(state->global, (void*)docStr, 0);
                    }
                }
                
                if (item->insertText != ZR_NULL) {
                    const char *insertStr = string_to_cstr(state, item->insertText);
                    if (insertStr != ZR_NULL) {
                        cJSON_AddStringToObject(itemJson, "insertText", insertStr);
                        ZrMemoryRawFree(state->global, (void*)insertStr, 0);
                    }
                }
                
                if (item->insertTextFormat != ZR_NULL) {
                    const char *formatStr = string_to_cstr(state, item->insertTextFormat);
                    if (formatStr != ZR_NULL) {
                        cJSON_AddStringToObject(itemJson, "insertTextFormat", formatStr);
                        ZrMemoryRawFree(state->global, (void*)formatStr, 0);
                    }
                }
                
                cJSON_AddItemToArray(json, itemJson);
            }
        }
    }
    
    return json;
}

// 序列化位置数组
static cJSON* serialize_locations(SZrState *state, SZrArray *locations) {
    cJSON *json = cJSON_CreateArray();
    if (json == ZR_NULL || state == ZR_NULL || locations == ZR_NULL) {
        return json;
    }
    
    for (TZrSize i = 0; i < locations->length; i++) {
        SZrLspLocation **locPtr = (SZrLspLocation **)ZrArrayGet(locations, i);
        if (locPtr != ZR_NULL && *locPtr != ZR_NULL) {
            SZrLspLocation *loc = *locPtr;
            cJSON *locJson = cJSON_CreateObject();
            if (locJson != ZR_NULL) {
                if (loc->uri != ZR_NULL) {
                    const char *uriStr = string_to_cstr(state, loc->uri);
                    if (uriStr != ZR_NULL) {
                        cJSON_AddStringToObject(locJson, "uri", uriStr);
                        ZrMemoryRawFree(state->global, (void*)uriStr, 0);
                    }
                }
                
                cJSON *range = serialize_lsp_range(loc->range);
                cJSON_AddItemToObject(locJson, "range", range);
                
                cJSON_AddItemToArray(json, locJson);
            }
        }
    }
    
    return json;
}

// 序列化悬停信息
static cJSON* serialize_hover(SZrState *state, SZrLspHover *hover) {
    cJSON *json = cJSON_CreateObject();
    if (json == ZR_NULL || state == ZR_NULL || hover == ZR_NULL) {
        return json;
    }
    
    cJSON *contents = cJSON_CreateArray();
    if (contents != ZR_NULL) {
        for (TZrSize i = 0; i < hover->contents.length; i++) {
            SZrString **strPtr = (SZrString **)ZrArrayGet(&hover->contents, i);
            if (strPtr != ZR_NULL && *strPtr != ZR_NULL) {
                const char *contentStr = string_to_cstr(state, *strPtr);
                if (contentStr != ZR_NULL) {
                    cJSON *contentJson = cJSON_CreateObject();
                    cJSON_AddStringToObject(contentJson, "kind", "markdown");
                    cJSON_AddStringToObject(contentJson, "value", contentStr);
                    cJSON_AddItemToArray(contents, contentJson);
                    ZrMemoryRawFree(state->global, (void*)contentStr, 0);
                }
            }
        }
        cJSON_AddItemToObject(json, "contents", contents);
    }
    
    cJSON *range = serialize_lsp_range(hover->range);
    cJSON_AddItemToObject(json, "range", range);
    
    return json;
}

// 创建错误响应 JSON
static const char* create_error_response(const char *message) {
    cJSON *json = cJSON_CreateObject();
    if (json != ZR_NULL) {
        cJSON_AddBoolToObject(json, "success", cJSON_False);
        if (message != ZR_NULL) {
            cJSON_AddStringToObject(json, "error", message);
        }
        char *result = cJSON_Print(json);
        cJSON_Delete(json);
        return result;
    }
    return ZR_NULL;
}

// 创建成功响应 JSON
static const char* create_success_response(cJSON *data) {
    cJSON *json = cJSON_CreateObject();
    if (json != ZR_NULL) {
        cJSON_AddBoolToObject(json, "success", cJSON_True);
        if (data != ZR_NULL) {
            cJSON_AddItemToObject(json, "data", data);
        }
        char *result = cJSON_Print(json);
        cJSON_Delete(json);
        return result;
    }
    return ZR_NULL;
}

// WASM 导出函数实现（使用 extern "C" 包装以保持 C 兼容性）

extern "C" {

// 内存管理函数
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void* wasm_malloc(size_t size) {
    if (g_wasm_global == ZR_NULL) {
        init_wasm_state();
    }
    if (g_wasm_global != ZR_NULL) {
        return ZrMemoryRawMalloc(g_wasm_global, (TZrSize)size);
    }
    return ZR_NULL;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void wasm_free(void* ptr) {
    if (g_wasm_global != ZR_NULL && ptr != ZR_NULL) {
        // 注意：需要知道原始大小，这里简化处理
        // TODO: 实现更精确的内存管理
        // 在 WASM 环境中，内存由 WebAssembly.Memory 管理
        // 这里可以留空或实现自定义内存跟踪
    }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void* wasm_ZrLspContextNew(void) {
    init_wasm_state();
    if (g_wasm_state == ZR_NULL) {
        return ZR_NULL;
    }
    SZrLspContext *context = ZrLspContextNew(g_wasm_state);
    return (void*)context;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void wasm_ZrLspContextFree(void* context) {
    if (g_wasm_state != ZR_NULL && context != ZR_NULL) {
        ZrLspContextFree(g_wasm_state, (SZrLspContext*)context);
    }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspUpdateDocument(void* context, const char* uri, int uriLen, 
                                const char* content, int contentLen, int version) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || content == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }
    
    SZrString *uriStr = cstr_to_string(g_wasm_state, uri, uriLen);
    if (uriStr == ZR_NULL) {
        return create_error_response("Failed to create URI string");
    }
    
    TBool result = ZrLspUpdateDocument(g_wasm_state, (SZrLspContext*)context, 
                                       uriStr, content, (TZrSize)contentLen, (TZrSize)version);
    
    if (result) {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "updated", cJSON_True);
        return create_success_response(json);
    } else {
        return create_error_response("Failed to update document");
    }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspGetDiagnostics(void* context, const char* uri, int uriLen) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }
    
    SZrString *uriStr = cstr_to_string(g_wasm_state, uri, uriLen);
    if (uriStr == ZR_NULL) {
        return create_error_response("Failed to create URI string");
    }
    
    SZrArray diagnostics;
    ZrArrayInit(g_wasm_state, &diagnostics, sizeof(SZrLspDiagnostic*), 8);
    
    TBool result = ZrLspGetDiagnostics(g_wasm_state, (SZrLspContext*)context, uriStr, &diagnostics);
    
    if (result) {
        cJSON *data = serialize_diagnostics(g_wasm_state, &diagnostics);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        for (TZrSize i = 0; i < diagnostics.length; i++) {
            SZrLspDiagnostic **diagPtr = (SZrLspDiagnostic **)ZrArrayGet(&diagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                ZrMemoryRawFree(g_wasm_state->global, *diagPtr, sizeof(SZrLspDiagnostic));
            }
        }
        ZrArrayFree(g_wasm_state, &diagnostics);
        
        return jsonStr;
    } else {
        ZrArrayFree(g_wasm_state, &diagnostics);
        return create_error_response("Failed to get diagnostics");
    }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspGetCompletion(void* context, const char* uri, int uriLen,
                               int line, int character) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }
    
    SZrString *uriStr = cstr_to_string(g_wasm_state, uri, uriLen);
    if (uriStr == ZR_NULL) {
        return create_error_response("Failed to create URI string");
    }
    
    SZrLspPosition position;
    position.line = line;
    position.character = character;
    
    SZrArray completions;
    ZrArrayInit(g_wasm_state, &completions, sizeof(SZrLspCompletionItem*), 8);
    
    TBool result = ZrLspGetCompletion(g_wasm_state, (SZrLspContext*)context, 
                                      uriStr, position, &completions);
    
    if (result) {
        cJSON *data = serialize_completions(g_wasm_state, &completions);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        for (TZrSize i = 0; i < completions.length; i++) {
            SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrArrayGet(&completions, i);
            if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
                ZrMemoryRawFree(g_wasm_state->global, *itemPtr, sizeof(SZrLspCompletionItem));
            }
        }
        ZrArrayFree(g_wasm_state, &completions);
        
        return jsonStr;
    } else {
        ZrArrayFree(g_wasm_state, &completions);
        return create_error_response("Failed to get completion");
    }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspGetHover(void* context, const char* uri, int uriLen,
                          int line, int character) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }
    
    SZrString *uriStr = cstr_to_string(g_wasm_state, uri, uriLen);
    if (uriStr == ZR_NULL) {
        return create_error_response("Failed to create URI string");
    }
    
    SZrLspPosition position;
    position.line = line;
    position.character = character;
    
    SZrLspHover *hover = ZR_NULL;
    TBool result = ZrLspGetHover(g_wasm_state, (SZrLspContext*)context, 
                                 uriStr, position, &hover);
    
    if (result && hover != ZR_NULL) {
        cJSON *data = serialize_hover(g_wasm_state, hover);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        ZrArrayFree(g_wasm_state, &hover->contents);
        ZrMemoryRawFree(g_wasm_state->global, hover, sizeof(SZrLspHover));
        
        return jsonStr;
    } else {
        return create_error_response("Failed to get hover");
    }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspGetDefinition(void* context, const char* uri, int uriLen,
                               int line, int character) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }
    
    SZrString *uriStr = cstr_to_string(g_wasm_state, uri, uriLen);
    if (uriStr == ZR_NULL) {
        return create_error_response("Failed to create URI string");
    }
    
    SZrLspPosition position;
    position.line = line;
    position.character = character;
    
    SZrArray locations;
    ZrArrayInit(g_wasm_state, &locations, sizeof(SZrLspLocation*), 1);
    
    TBool result = ZrLspGetDefinition(g_wasm_state, (SZrLspContext*)context, 
                                      uriStr, position, &locations);
    
    if (result) {
        cJSON *data = serialize_locations(g_wasm_state, &locations);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        for (TZrSize i = 0; i < locations.length; i++) {
            SZrLspLocation **locPtr = (SZrLspLocation **)ZrArrayGet(&locations, i);
            if (locPtr != ZR_NULL && *locPtr != ZR_NULL) {
                ZrMemoryRawFree(g_wasm_state->global, *locPtr, sizeof(SZrLspLocation));
            }
        }
        ZrArrayFree(g_wasm_state, &locations);
        
        return jsonStr;
    } else {
        ZrArrayFree(g_wasm_state, &locations);
        return create_error_response("Failed to get definition");
    }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspFindReferences(void* context, const char* uri, int uriLen,
                               int line, int character, int includeDeclaration) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }
    
    SZrString *uriStr = cstr_to_string(g_wasm_state, uri, uriLen);
    if (uriStr == ZR_NULL) {
        return create_error_response("Failed to create URI string");
    }
    
    SZrLspPosition position;
    position.line = line;
    position.character = character;
    
    SZrArray locations;
    ZrArrayInit(g_wasm_state, &locations, sizeof(SZrLspLocation*), 8);
    
    TBool result = ZrLspFindReferences(g_wasm_state, (SZrLspContext*)context, 
                                      uriStr, position, includeDeclaration != 0, &locations);
    
    if (result) {
        cJSON *data = serialize_locations(g_wasm_state, &locations);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        for (TZrSize i = 0; i < locations.length; i++) {
            SZrLspLocation **locPtr = (SZrLspLocation **)ZrArrayGet(&locations, i);
            if (locPtr != ZR_NULL && *locPtr != ZR_NULL) {
                ZrMemoryRawFree(g_wasm_state->global, *locPtr, sizeof(SZrLspLocation));
            }
        }
        ZrArrayFree(g_wasm_state, &locations);
        
        return jsonStr;
    } else {
        ZrArrayFree(g_wasm_state, &locations);
        return create_error_response("Failed to find references");
    }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspRename(void* context, const char* uri, int uriLen,
                        int line, int character, const char* newName, int newNameLen) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || newName == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }
    
    SZrString *uriStr = cstr_to_string(g_wasm_state, uri, uriLen);
    SZrString *newNameStr = cstr_to_string(g_wasm_state, newName, newNameLen);
    
    if (uriStr == ZR_NULL || newNameStr == ZR_NULL) {
        return create_error_response("Failed to create string");
    }
    
    SZrLspPosition position;
    position.line = line;
    position.character = character;
    
    SZrArray locations;
    ZrArrayInit(g_wasm_state, &locations, sizeof(SZrLspLocation*), 8);
    
    TBool result = ZrLspRename(g_wasm_state, (SZrLspContext*)context, 
                              uriStr, position, newNameStr, &locations);
    
    if (result) {
        cJSON *data = serialize_locations(g_wasm_state, &locations);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        for (TZrSize i = 0; i < locations.length; i++) {
            SZrLspLocation **locPtr = (SZrLspLocation **)ZrArrayGet(&locations, i);
            if (locPtr != ZR_NULL && *locPtr != ZR_NULL) {
                ZrMemoryRawFree(g_wasm_state->global, *locPtr, sizeof(SZrLspLocation));
            }
        }
        ZrArrayFree(g_wasm_state, &locations);
        
        return jsonStr;
    } else {
        ZrArrayFree(g_wasm_state, &locations);
        return create_error_response("Failed to rename");
    }
}

} // extern "C"

#endif // ZR_WASM_BUILD
