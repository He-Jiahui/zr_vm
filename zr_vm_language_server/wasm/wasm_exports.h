//
// Created by Auto on 2025/01/XX.
// WASM 导出函数头文件
//

#ifndef ZR_VM_LANGUAGE_SERVER_WASM_EXPORTS_H
#define ZR_VM_LANGUAGE_SERVER_WASM_EXPORTS_H

#include "zr_vm_common.h"

// WASM 导出宏（在 C++ 模式下不使用 emscripten.h，避免与 extern "C" 冲突）
#ifdef __EMSCRIPTEN__
    #ifdef __cplusplus
        // C++ 模式下，使用 EMSCRIPTEN_BINDINGS，不需要 KEEPALIVE
        #define WASM_EXPORT
    #else
        // C 模式下，使用 EMSCRIPTEN_KEEPALIVE
        #include <emscripten.h>
        #define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
    #endif
#else
    // 其他 WASM 编译器（如 wasm32-unknown-unknown）使用 export_name 属性
    #define WASM_EXPORT __attribute__((export_name))
#endif

// 如果没有定义 ZR_WASM_BUILD，则不导出
#ifndef ZR_WASM_BUILD
    #undef WASM_EXPORT
    #define WASM_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 内存管理函数（必须导出）
void* wasm_malloc(size_t size);

void wasm_free(void* ptr);

// LSP 上下文管理
void* wasm_ZrLspContextNew(void);

void wasm_ZrLspContextFree(void* context);

// 文档更新
// uri: UTF-8 字符串指针
// uriLen: URI 长度
// content: UTF-8 字符串指针
// contentLen: 内容长度
// version: 版本号
// 返回: JSON 字符串指针（需要调用 wasm_free 释放）
const char* wasm_ZrLspUpdateDocument(void* context, const char* uri, int uriLen, 
                                 const char* content, int contentLen, int version);

// 获取诊断
// uri: UTF-8 字符串指针
// uriLen: URI 长度
// 返回: JSON 字符串指针（包含诊断数组，需要调用 wasm_free 释放）
const char* wasm_ZrLspGetDiagnostics(void* context, const char* uri, int uriLen);

// 获取补全
// uri: UTF-8 字符串指针
// uriLen: URI 长度
// line: 行号（从0开始）
// character: 列号（从0开始）
// 返回: JSON 字符串指针（包含补全项数组，需要调用 wasm_free 释放）
const char* wasm_ZrLspGetCompletion(void* context, const char* uri, int uriLen,
                               int line, int character);

// 获取悬停信息
// uri: UTF-8 字符串指针
// uriLen: URI 长度
// line: 行号（从0开始）
// character: 列号（从0开始）
// 返回: JSON 字符串指针（包含悬停信息，需要调用 wasm_free 释放）
const char* wasm_ZrLspGetHover(void* context, const char* uri, int uriLen,
                          int line, int character);

// 获取定义位置
// uri: UTF-8 字符串指针
// uriLen: URI 长度
// line: 行号（从0开始）
// character: 列号（从0开始）
// 返回: JSON 字符串指针（包含位置数组，需要调用 wasm_free 释放）
const char* wasm_ZrLspGetDefinition(void* context, const char* uri, int uriLen,
                                int line, int character);

// 查找引用
// uri: UTF-8 字符串指针
// uriLen: URI 长度
// line: 行号（从0开始）
// character: 列号（从0开始）
// includeDeclaration: 是否包含声明（1=true, 0=false）
// 返回: JSON 字符串指针（包含位置数组，需要调用 wasm_free 释放）
const char* wasm_ZrLspFindReferences(void* context, const char* uri, int uriLen,
                               int line, int character, int includeDeclaration);

// 重命名符号
// uri: UTF-8 字符串指针
// uriLen: URI 长度
// line: 行号（从0开始）
// character: 列号（从0开始）
// newName: 新名称（UTF-8 字符串指针）
// newNameLen: 新名称长度
// 返回: JSON 字符串指针（包含位置数组，需要调用 wasm_free 释放）
const char* wasm_ZrLspRename(void* context, const char* uri, int uriLen,
                        int line, int character, const char* newName, int newNameLen);

#ifdef __cplusplus
}
#endif

#endif //ZR_VM_LANGUAGE_SERVER_WASM_EXPORTS_H
