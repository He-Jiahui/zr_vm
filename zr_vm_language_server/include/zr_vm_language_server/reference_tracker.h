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
#include "zr_vm_core/hash_set.h"

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
    SZrFileRange location;            // 引用位置
    EZrReferenceType type;            // 引用类型
} SZrReference;

// 引用追踪器
typedef struct SZrReferenceTracker {
    SZrState *state;
    SZrSymbolTable *symbolTable;
    SZrArray allReferences;           // 所有引用（SZrReference*）
    SZrHashSet symbolToReferencesMap; // TODO: 符号到引用的映射（简化实现，暂时不使用）
} SZrReferenceTracker;

// 引用追踪器管理函数

// 创建引用追踪器
ZR_LANGUAGE_SERVER_API SZrReferenceTracker *ZrReferenceTrackerNew(SZrState *state, 
                                                                     SZrSymbolTable *symbolTable);

// 释放引用追踪器
ZR_LANGUAGE_SERVER_API void ZrReferenceTrackerFree(SZrState *state, SZrReferenceTracker *tracker);

// 添加引用
ZR_LANGUAGE_SERVER_API TBool ZrReferenceTrackerAddReference(SZrState *state, 
                                                              SZrReferenceTracker *tracker,
                                                              SZrSymbol *symbol,
                                                              SZrFileRange location,
                                                              EZrReferenceType type);

// 查找所有引用
ZR_LANGUAGE_SERVER_API TBool ZrReferenceTrackerFindReferences(SZrState *state,
                                                               SZrReferenceTracker *tracker,
                                                               SZrSymbol *symbol,
                                                               SZrArray *result);

// 获取引用计数
ZR_LANGUAGE_SERVER_API TZrSize ZrReferenceTrackerGetReferenceCount(SZrReferenceTracker *tracker,
                                                                    SZrSymbol *symbol);

// 查找位置处的引用
ZR_LANGUAGE_SERVER_API SZrReference *ZrReferenceTrackerFindReferenceAt(SZrReferenceTracker *tracker,
                                                                        SZrFileRange position);

// 获取符号的所有引用位置
ZR_LANGUAGE_SERVER_API TBool ZrReferenceTrackerGetReferenceLocations(SZrState *state,
                                                                     SZrReferenceTracker *tracker,
                                                                     SZrSymbol *symbol,
                                                                     SZrArray *result);

#endif //ZR_VM_LANGUAGE_SERVER_REFERENCE_TRACKER_H
