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
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/value.h"
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
// 注意：FZrAllocator 的最后一个参数是 TZrInt64，不是 EZrMemoryNativeType
static TZrPtr wasm_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
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
        g_wasm_global = ZrCore_GlobalState_New(wasm_allocator, ZR_NULL, 0, &callbacks);
        if (g_wasm_global != ZR_NULL) {
            g_wasm_state = g_wasm_global->mainThreadState;
            // 初始化注册表
            if (g_wasm_state != ZR_NULL) {
                ZrCore_GlobalState_InitRegistry(g_wasm_state, g_wasm_global);
            }
        }
    }
}

// JSON 序列化辅助函数

// 序列化 LSP 位置
static cJSON* serialize_lsp_position(SZrLspPosition pos) {
    cJSON *json = cJSON_CreateObject();
    if (json != ZR_NULL) {
        cJSON_AddNumberToObject(json, ZR_LSP_FIELD_LINE, pos.line);
        cJSON_AddNumberToObject(json, ZR_LSP_FIELD_CHARACTER, pos.character);
    }
    return json;
}

// 序列化 LSP 范围
static cJSON* serialize_lsp_range(SZrLspRange range) {
    cJSON *json = cJSON_CreateObject();
    if (json != ZR_NULL) {
        cJSON *start = serialize_lsp_position(range.start);
        cJSON *end = serialize_lsp_position(range.end);
        cJSON_AddItemToObject(json, ZR_LSP_FIELD_START, start);
        cJSON_AddItemToObject(json, ZR_LSP_FIELD_END, end);
    }
    return json;
}

// 序列化字符串（从 SZrString 到 C 字符串）
static const char* string_to_cstr(SZrState *state, SZrString *str) {
    if (str == ZR_NULL || state == ZR_NULL) {
        return ZR_NULL;
    }
    
    TZrNativeString nativeStr;
    TZrSize len;
    if (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nativeStr = ZrCore_String_GetNativeStringShort(str);
        len = str->shortStringLength;
    } else {
        nativeStr = ZrCore_String_GetNativeString(str);
        len = str->longStringLength;
    }
    
    // 分配内存并复制字符串
    TZrChar *cstr = (TZrChar *)ZrCore_Memory_RawMalloc(state->global, (len + 1) * sizeof(TZrChar));
    if (cstr != ZR_NULL) {
        memcpy(cstr, nativeStr, len * sizeof(TZrChar));
        cstr[len] = '\0';
    }
    return cstr;
}

static void free_cstr(SZrState *state, const char *cstr) {
    TZrSize length;
    if (state == ZR_NULL || cstr == ZR_NULL) {
        return;
    }

    length = strlen(cstr) + 1;
    ZrCore_Memory_RawFree(state->global, (void*)cstr, length);
}

// 从 C 字符串创建 SZrString
static SZrString* cstr_to_string(SZrState *state, const char *cstr, int len) {
    if (state == ZR_NULL || cstr == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(state, (TZrNativeString)cstr, (TZrSize)len);
}

// 序列化诊断数组
static cJSON* serialize_diagnostics(SZrState *state, SZrArray *diagnostics) {
    cJSON *json = cJSON_CreateArray();
    if (json == ZR_NULL || state == ZR_NULL || diagnostics == ZR_NULL) {
        return json;
    }
    
    for (TZrSize i = 0; i < diagnostics->length; i++) {
        SZrLspDiagnostic **diagPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                SZrLspDiagnostic *diag = *diagPtr;
                cJSON *diagJson = cJSON_CreateObject();
                if (diagJson != ZR_NULL) {
                    cJSON *range = serialize_lsp_range(diag->range);
                    cJSON_AddItemToObject(diagJson, ZR_LSP_FIELD_RANGE, range);
                    cJSON_AddNumberToObject(diagJson, ZR_LSP_FIELD_SEVERITY, diag->severity);
                
                    if (diag->code != ZR_NULL) {
                        const char *codeStr = string_to_cstr(state, diag->code);
                        if (codeStr != ZR_NULL) {
                            cJSON_AddStringToObject(diagJson, ZR_LSP_FIELD_CODE, codeStr);
                            free_cstr(state, codeStr);
                        }
                    }
                
                    if (diag->message != ZR_NULL) {
                        const char *msgStr = string_to_cstr(state, diag->message);
                        if (msgStr != ZR_NULL) {
                            cJSON_AddStringToObject(diagJson, ZR_LSP_FIELD_MESSAGE, msgStr);
                            free_cstr(state, msgStr);
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
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(completions, i);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            SZrLspCompletionItem *item = *itemPtr;
            cJSON *itemJson = cJSON_CreateObject();
            if (itemJson != ZR_NULL) {
                if (item->label != ZR_NULL) {
                    const char *labelStr = string_to_cstr(state, item->label);
                    if (labelStr != ZR_NULL) {
                        cJSON_AddStringToObject(itemJson, ZR_LSP_FIELD_LABEL, labelStr);
                        free_cstr(state, labelStr);
                    }
                }
                
                cJSON_AddNumberToObject(itemJson, ZR_LSP_FIELD_KIND, item->kind);
                
                if (item->detail != ZR_NULL) {
                    const char *detailStr = string_to_cstr(state, item->detail);
                    if (detailStr != ZR_NULL) {
                        cJSON_AddStringToObject(itemJson, ZR_LSP_FIELD_DETAIL, detailStr);
                        free_cstr(state, detailStr);
                    }
                }
                
                if (item->documentation != ZR_NULL) {
                    const char *docStr = string_to_cstr(state, item->documentation);
                    if (docStr != ZR_NULL) {
                        cJSON *docJson = cJSON_CreateObject();
                        cJSON_AddStringToObject(docJson, ZR_LSP_FIELD_KIND, ZR_LSP_MARKUP_KIND_MARKDOWN);
                        cJSON_AddStringToObject(docJson, ZR_LSP_FIELD_VALUE, docStr);
                        cJSON_AddItemToObject(itemJson, ZR_LSP_FIELD_DOCUMENTATION, docJson);
                        free_cstr(state, docStr);
                    }
                }
                
                if (item->insertText != ZR_NULL) {
                    const char *insertStr = string_to_cstr(state, item->insertText);
                    if (insertStr != ZR_NULL) {
                        cJSON_AddStringToObject(itemJson, ZR_LSP_FIELD_INSERT_TEXT, insertStr);
                        free_cstr(state, insertStr);
                    }
                }
                
                if (item->insertTextFormat != ZR_NULL) {
                    const char *formatStr = string_to_cstr(state, item->insertTextFormat);
                    if (formatStr != ZR_NULL) {
                        cJSON_AddStringToObject(itemJson, ZR_LSP_FIELD_INSERT_TEXT_FORMAT, formatStr);
                        free_cstr(state, formatStr);
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
        SZrLspLocation **locPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, i);
        if (locPtr != ZR_NULL && *locPtr != ZR_NULL) {
            SZrLspLocation *loc = *locPtr;
            cJSON *locJson = cJSON_CreateObject();
            if (locJson != ZR_NULL) {
                if (loc->uri != ZR_NULL) {
                    const char *uriStr = string_to_cstr(state, loc->uri);
                    if (uriStr != ZR_NULL) {
                        cJSON_AddStringToObject(locJson, ZR_LSP_FIELD_URI, uriStr);
                        free_cstr(state, uriStr);
                    }
                }
                
                cJSON *range = serialize_lsp_range(loc->range);
                cJSON_AddItemToObject(locJson, ZR_LSP_FIELD_RANGE, range);
                
                cJSON_AddItemToArray(json, locJson);
            }
        }
    }
    
    return json;
}

static cJSON* serialize_semantic_tokens(SZrArray *tokens) {
    cJSON *json = cJSON_CreateObject();
    cJSON *data = cJSON_CreateArray();

    if (json == ZR_NULL || data == ZR_NULL) {
        cJSON_Delete(json);
        cJSON_Delete(data);
        return ZR_NULL;
    }

    for (TZrSize i = 0; tokens != ZR_NULL && i < tokens->length; i++) {
        TZrUInt32 *valuePtr = (TZrUInt32 *)ZrCore_Array_Get(tokens, i);
        if (valuePtr != ZR_NULL) {
            cJSON_AddItemToArray(data, cJSON_CreateNumber((double)(*valuePtr)));
        }
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_DATA, data);
    return json;
}

// 序列化悬停信息
static cJSON* serialize_hover(SZrState *state, SZrLspHover *hover) {
    cJSON *json = cJSON_CreateObject();
    const char *contentStr = ZR_NULL;
    if (json == ZR_NULL || state == ZR_NULL || hover == ZR_NULL) {
        return json;
    }
    
    cJSON *contents = cJSON_CreateObject();
    if (contents != ZR_NULL) {
        if (hover->contents.length > 0) {
            SZrString **strPtr = (SZrString **)ZrCore_Array_Get(&hover->contents, 0);
            if (strPtr != ZR_NULL && *strPtr != ZR_NULL) {
                contentStr = string_to_cstr(state, *strPtr);
            }
        }
        cJSON_AddStringToObject(contents, ZR_LSP_FIELD_KIND, ZR_LSP_MARKUP_KIND_MARKDOWN);
        cJSON_AddStringToObject(contents, ZR_LSP_FIELD_VALUE, contentStr != ZR_NULL ? contentStr : "");
        cJSON_AddItemToObject(json, ZR_LSP_FIELD_CONTENTS, contents);
    }
    
    cJSON *range = serialize_lsp_range(hover->range);
    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, range);
    if (contentStr != ZR_NULL) {
        free_cstr(state, contentStr);
    }
    
    return json;
}

static cJSON* serialize_symbol_information(SZrState *state, SZrLspSymbolInformation *symbol) {
    cJSON *json = cJSON_CreateObject();
    cJSON *locationJson;
    if (json == ZR_NULL || state == ZR_NULL || symbol == ZR_NULL) {
        return json;
    }

    if (symbol->name != ZR_NULL) {
        const char *nameStr = string_to_cstr(state, symbol->name);
        if (nameStr != ZR_NULL) {
            cJSON_AddStringToObject(json, ZR_LSP_FIELD_NAME, nameStr);
            free_cstr(state, nameStr);
        }
    }

    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_KIND, symbol->kind);

    if (symbol->containerName != ZR_NULL) {
        const char *containerStr = string_to_cstr(state, symbol->containerName);
        if (containerStr != ZR_NULL) {
            cJSON_AddStringToObject(json, ZR_LSP_FIELD_CONTAINER_NAME, containerStr);
            free_cstr(state, containerStr);
        }
    }

    locationJson = cJSON_CreateObject();
    if (locationJson != ZR_NULL) {
        if (symbol->location.uri != ZR_NULL) {
            const char *uriStr = string_to_cstr(state, symbol->location.uri);
            if (uriStr != ZR_NULL) {
                cJSON_AddStringToObject(locationJson, ZR_LSP_FIELD_URI, uriStr);
                free_cstr(state, uriStr);
            }
        }
        cJSON_AddItemToObject(locationJson, ZR_LSP_FIELD_RANGE, serialize_lsp_range(symbol->location.range));
        cJSON_AddItemToObject(json, ZR_LSP_FIELD_LOCATION, locationJson);
    }
    return json;
}

static cJSON* serialize_symbol_array(SZrState *state, SZrArray *symbols) {
    cJSON *json = cJSON_CreateArray();
    if (json == ZR_NULL || state == ZR_NULL || symbols == ZR_NULL) {
        return json;
    }

    for (TZrSize i = 0; i < symbols->length; i++) {
        SZrLspSymbolInformation **symbolPtr = (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, i);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_symbol_information(state, *symbolPtr));
        }
    }

    return json;
}

static cJSON* serialize_document_highlight(SZrLspDocumentHighlight *highlight) {
    cJSON *json = cJSON_CreateObject();
    if (json == ZR_NULL || highlight == ZR_NULL) {
        return json;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_lsp_range(highlight->range));
    cJSON_AddNumberToObject(json, ZR_LSP_FIELD_KIND, highlight->kind);
    return json;
}

static cJSON* serialize_highlights(SZrArray *highlights) {
    cJSON *json = cJSON_CreateArray();
    if (json == ZR_NULL || highlights == ZR_NULL) {
        return json;
    }

    for (TZrSize i = 0; i < highlights->length; i++) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, i);
        if (highlightPtr != ZR_NULL && *highlightPtr != ZR_NULL) {
            cJSON_AddItemToArray(json, serialize_document_highlight(*highlightPtr));
        }
    }

    return json;
}

static void remove_document_state(SZrLspContext *context, SZrString *uri) {
    SZrTypeValue key;
    SZrHashKeyValuePair *pair;

    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return;
    }

    if (context->parser != ZR_NULL) {
        ZrLanguageServer_IncrementalParser_RemoveFile(g_wasm_state, context->parser, uri);
    }

    ZrCore_Value_InitAsRawObject(g_wasm_state, &key, &uri->super);
    pair = ZrCore_HashSet_Find(g_wasm_state, &context->uriToAnalyzerMap, &key);
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
        SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
        if (analyzer != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_Free(g_wasm_state, analyzer);
        }
    }

    ZrCore_HashSet_Remove(g_wasm_state, &context->uriToAnalyzerMap, &key);
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
        return ZrCore_Memory_RawMalloc(g_wasm_global, (TZrSize)size);
    }
    return ZR_NULL;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void wasm_free(void* ptr) {
    if (ptr != ZR_NULL) {
        free(ptr);
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
    SZrLspContext *context = ZrLanguageServer_LspContext_New(g_wasm_state);
    return (void*)context;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void wasm_ZrLspContextFree(void* context) {
    if (g_wasm_state != ZR_NULL && context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(g_wasm_state, (SZrLspContext*)context);
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
    
    TZrBool result = ZrLanguageServer_Lsp_UpdateDocument(g_wasm_state, (SZrLspContext*)context, 
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
const char* wasm_ZrLspCloseDocument(void* context, const char* uri, int uriLen) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }

    SZrString *uriStr = cstr_to_string(g_wasm_state, uri, uriLen);
    if (uriStr == ZR_NULL) {
        return create_error_response("Failed to create URI string");
    }

    remove_document_state((SZrLspContext*)context, uriStr);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "closed", cJSON_True);
    return create_success_response(json);
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
    ZrCore_Array_Init(g_wasm_state, &diagnostics, sizeof(SZrLspDiagnostic*), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    
    TZrBool result = ZrLanguageServer_Lsp_GetDiagnostics(g_wasm_state, (SZrLspContext*)context, uriStr, &diagnostics);
    
    if (result) {
        cJSON *data = serialize_diagnostics(g_wasm_state, &diagnostics);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        for (TZrSize i = 0; i < diagnostics.length; i++) {
            SZrLspDiagnostic **diagPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(&diagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                ZrCore_Memory_RawFree(g_wasm_state->global, *diagPtr, sizeof(SZrLspDiagnostic));
            }
        }
        ZrCore_Array_Free(g_wasm_state, &diagnostics);
        
        return jsonStr;
    } else {
        ZrCore_Array_Free(g_wasm_state, &diagnostics);
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
    ZrCore_Array_Init(g_wasm_state, &completions, sizeof(SZrLspCompletionItem*), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    
    TZrBool result = ZrLanguageServer_Lsp_GetCompletion(g_wasm_state, (SZrLspContext*)context, 
                                      uriStr, position, &completions);
    
    if (result) {
        cJSON *data = serialize_completions(g_wasm_state, &completions);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        for (TZrSize i = 0; i < completions.length; i++) {
            SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(&completions, i);
            if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
                ZrCore_Memory_RawFree(g_wasm_state->global, *itemPtr, sizeof(SZrLspCompletionItem));
            }
        }
        ZrCore_Array_Free(g_wasm_state, &completions);
        
        return jsonStr;
    } else {
        ZrCore_Array_Free(g_wasm_state, &completions);
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
    TZrBool result = ZrLanguageServer_Lsp_GetHover(g_wasm_state, (SZrLspContext*)context, 
                                 uriStr, position, &hover);
    
    if (result && hover != ZR_NULL) {
        cJSON *data = serialize_hover(g_wasm_state, hover);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        ZrCore_Array_Free(g_wasm_state, &hover->contents);
        ZrCore_Memory_RawFree(g_wasm_state->global, hover, sizeof(SZrLspHover));
        
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
    ZrCore_Array_Init(g_wasm_state, &locations, sizeof(SZrLspLocation*), 1);
    
    TZrBool result = ZrLanguageServer_Lsp_GetDefinition(g_wasm_state, (SZrLspContext*)context, 
                                      uriStr, position, &locations);
    
    if (result) {
        cJSON *data = serialize_locations(g_wasm_state, &locations);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        for (TZrSize i = 0; i < locations.length; i++) {
            SZrLspLocation **locPtr = (SZrLspLocation **)ZrCore_Array_Get(&locations, i);
            if (locPtr != ZR_NULL && *locPtr != ZR_NULL) {
                ZrCore_Memory_RawFree(g_wasm_state->global, *locPtr, sizeof(SZrLspLocation));
            }
        }
        ZrCore_Array_Free(g_wasm_state, &locations);
        
        return jsonStr;
    } else {
        ZrCore_Array_Free(g_wasm_state, &locations);
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
    ZrCore_Array_Init(g_wasm_state, &locations, sizeof(SZrLspLocation*), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    
    TZrBool result = ZrLanguageServer_Lsp_FindReferences(g_wasm_state, (SZrLspContext*)context, 
                                      uriStr, position, includeDeclaration != 0, &locations);
    
    if (result) {
        cJSON *data = serialize_locations(g_wasm_state, &locations);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        for (TZrSize i = 0; i < locations.length; i++) {
            SZrLspLocation **locPtr = (SZrLspLocation **)ZrCore_Array_Get(&locations, i);
            if (locPtr != ZR_NULL && *locPtr != ZR_NULL) {
                ZrCore_Memory_RawFree(g_wasm_state->global, *locPtr, sizeof(SZrLspLocation));
            }
        }
        ZrCore_Array_Free(g_wasm_state, &locations);
        
        return jsonStr;
    } else {
        ZrCore_Array_Free(g_wasm_state, &locations);
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
    ZrCore_Array_Init(g_wasm_state, &locations, sizeof(SZrLspLocation*), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    
    TZrBool result = ZrLanguageServer_Lsp_Rename(g_wasm_state, (SZrLspContext*)context, 
                              uriStr, position, newNameStr, &locations);
    
    if (result) {
        cJSON *data = serialize_locations(g_wasm_state, &locations);
        const char *jsonStr = create_success_response(data);
        
        // 清理
        for (TZrSize i = 0; i < locations.length; i++) {
            SZrLspLocation **locPtr = (SZrLspLocation **)ZrCore_Array_Get(&locations, i);
            if (locPtr != ZR_NULL && *locPtr != ZR_NULL) {
                ZrCore_Memory_RawFree(g_wasm_state->global, *locPtr, sizeof(SZrLspLocation));
            }
        }
        ZrCore_Array_Free(g_wasm_state, &locations);
        
        return jsonStr;
    } else {
        ZrCore_Array_Free(g_wasm_state, &locations);
        return create_error_response("Failed to rename");
    }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspGetDocumentSymbols(void* context, const char* uri, int uriLen) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }

    SZrString *uriStr = cstr_to_string(g_wasm_state, uri, uriLen);
    if (uriStr == ZR_NULL) {
        return create_error_response("Failed to create URI string");
    }

    SZrArray symbols;
    ZrCore_Array_Init(g_wasm_state, &symbols, sizeof(SZrLspSymbolInformation*), ZR_LSP_ARRAY_INITIAL_CAPACITY);

    if (ZrLanguageServer_Lsp_GetDocumentSymbols(g_wasm_state, (SZrLspContext*)context, uriStr, &symbols)) {
        cJSON *data = serialize_symbol_array(g_wasm_state, &symbols);
        const char *jsonStr = create_success_response(data);

        for (TZrSize i = 0; i < symbols.length; i++) {
            SZrLspSymbolInformation **symbolPtr =
                (SZrLspSymbolInformation **)ZrCore_Array_Get(&symbols, i);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                ZrCore_Memory_RawFree(g_wasm_state->global, *symbolPtr, sizeof(SZrLspSymbolInformation));
            }
        }
        ZrCore_Array_Free(g_wasm_state, &symbols);
        return jsonStr;
    }

    ZrCore_Array_Free(g_wasm_state, &symbols);
    return create_error_response("Failed to get document symbols");
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspGetWorkspaceSymbols(void* context, const char* query, int queryLen) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || query == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }

    SZrString *queryStr = cstr_to_string(g_wasm_state, query, queryLen);
    if (queryStr == ZR_NULL) {
        return create_error_response("Failed to create query string");
    }

    SZrArray symbols;
    ZrCore_Array_Init(g_wasm_state, &symbols, sizeof(SZrLspSymbolInformation*), ZR_LSP_ARRAY_INITIAL_CAPACITY);

    if (ZrLanguageServer_Lsp_GetWorkspaceSymbols(g_wasm_state, (SZrLspContext*)context, queryStr, &symbols)) {
        cJSON *data = serialize_symbol_array(g_wasm_state, &symbols);
        const char *jsonStr = create_success_response(data);

        for (TZrSize i = 0; i < symbols.length; i++) {
            SZrLspSymbolInformation **symbolPtr =
                (SZrLspSymbolInformation **)ZrCore_Array_Get(&symbols, i);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                ZrCore_Memory_RawFree(g_wasm_state->global, *symbolPtr, sizeof(SZrLspSymbolInformation));
            }
        }
        ZrCore_Array_Free(g_wasm_state, &symbols);
        return jsonStr;
    }

    ZrCore_Array_Free(g_wasm_state, &symbols);
    return create_error_response("Failed to get workspace symbols");
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspGetDocumentHighlights(void* context, const char* uri, int uriLen,
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

    SZrArray highlights;
    ZrCore_Array_Init(g_wasm_state, &highlights, sizeof(SZrLspDocumentHighlight*), ZR_LSP_ARRAY_INITIAL_CAPACITY);

    if (ZrLanguageServer_Lsp_GetDocumentHighlights(
            g_wasm_state,
            (SZrLspContext*)context,
            uriStr,
            position,
            &highlights)) {
        cJSON *data = serialize_highlights(&highlights);
        const char *jsonStr = create_success_response(data);

        for (TZrSize i = 0; i < highlights.length; i++) {
            SZrLspDocumentHighlight **highlightPtr =
                (SZrLspDocumentHighlight **)ZrCore_Array_Get(&highlights, i);
            if (highlightPtr != ZR_NULL && *highlightPtr != ZR_NULL) {
                ZrCore_Memory_RawFree(g_wasm_state->global, *highlightPtr, sizeof(SZrLspDocumentHighlight));
            }
        }
        ZrCore_Array_Free(g_wasm_state, &highlights);
        return jsonStr;
    }

    ZrCore_Array_Free(g_wasm_state, &highlights);
    return create_error_response("Failed to get document highlights");
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspGetSemanticTokens(void* context, const char* uri, int uriLen) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }

    SZrString *uriStr = cstr_to_string(g_wasm_state, uri, uriLen);
    if (uriStr == ZR_NULL) {
        return create_error_response("Failed to create URI string");
    }

    SZrArray tokens;
    ZrCore_Array_Init(g_wasm_state, &tokens, sizeof(TZrUInt32), ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY);

    if (ZrLanguageServer_Lsp_GetSemanticTokens(g_wasm_state, (SZrLspContext*)context, uriStr, &tokens)) {
        cJSON *data = serialize_semantic_tokens(&tokens);
        const char *jsonStr = create_success_response(data);
        ZrCore_Array_Free(g_wasm_state, &tokens);
        return jsonStr;
    }

    ZrCore_Array_Free(g_wasm_state, &tokens);
    return create_error_response("Failed to get semantic tokens");
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* wasm_ZrLspPrepareRename(void* context, const char* uri, int uriLen,
                               int line, int character) {
    if (g_wasm_state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL) {
        return create_error_response("Invalid parameters");
    }

    SZrString *uriStr = cstr_to_string(g_wasm_state, uri, uriLen);
    if (uriStr == ZR_NULL) {
        return create_error_response("Failed to create URI string");
    }

    SZrLspPosition position;
    SZrLspRange range;
    SZrString *placeholder = ZR_NULL;
    position.line = line;
    position.character = character;

    if (ZrLanguageServer_Lsp_PrepareRename(
            g_wasm_state,
            (SZrLspContext*)context,
            uriStr,
            position,
            &range,
            &placeholder)) {
        cJSON *data = cJSON_CreateObject();
        const char *placeholderStr = string_to_cstr(g_wasm_state, placeholder);

        cJSON_AddItemToObject(data, ZR_LSP_FIELD_RANGE, serialize_lsp_range(range));
        cJSON_AddStringToObject(data, ZR_LSP_FIELD_PLACEHOLDER, placeholderStr != ZR_NULL ? placeholderStr : "");
        if (placeholderStr != ZR_NULL) {
            free_cstr(g_wasm_state, placeholderStr);
        }
        return create_success_response(data);
    }

    return create_success_response(cJSON_CreateNull());
}

} // extern "C"

#endif // ZR_WASM_BUILD
