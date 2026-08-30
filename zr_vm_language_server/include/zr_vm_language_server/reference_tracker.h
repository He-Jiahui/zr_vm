//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_LANGUAGE_SERVER_REFERENCE_TRACKER_H
#define ZR_VM_LANGUAGE_SERVER_REFERENCE_TRACKER_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_language_server/symbol_table.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/array.h"

// 引用类型枚举
enum EZrReferenceType {
    ZR_REFERENCE_READ,      // 读引用
    ZR_REFERENCE_WRITE,     // 写引用
    ZR_REFERENCE_DEFINITION, // 定义引用
    ZR_REFERENCE_CALL,      // 函数调用引用
};

typedef enum EZrReferenceType EZrReferenceType;

// 引用信息
typedef struct SZrReference {
    SZrSymbol *symbol;                // 引用的符号
    TZrSymbolId symbolId;             // canonical symbol identity; invalid for syntax-only recovery
    SZrFileRange location;            // 引用位置
    EZrReferenceType type;            // 引用类型
} SZrReference;

// 引用追踪器
typedef struct SZrReferenceTracker {
    SZrArray allReferences;           // 所有引用（SZrReference*）
} SZrReferenceTracker;

// 引用追踪器管理函数

// 创建引用追踪器
ZR_LANGUAGE_SERVER_API SZrReferenceTracker *ZrLanguageServer_ReferenceTracker_New(SZrState *state, 
                                                                     SZrSymbolTable *symbolTable);

// 释放引用追踪器
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_ReferenceTracker_Free(SZrState *state, SZrReferenceTracker *tracker);

// 添加引用
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_ReferenceTracker_AddReference(SZrState *state, 
                                                              SZrReferenceTracker *tracker,
                                                              SZrSymbol *symbol,
                                                              SZrFileRange location,
                                                              EZrReferenceType type);

// 查找位置处的引用
ZR_LANGUAGE_SERVER_API SZrReference *ZrLanguageServer_ReferenceTracker_FindReferenceAt(SZrReferenceTracker *tracker,
                                                                        SZrFileRange position);

#endif //ZR_VM_LANGUAGE_SERVER_REFERENCE_TRACKER_H
