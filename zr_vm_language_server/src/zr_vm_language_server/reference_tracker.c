//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_language_server/reference_tracker.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/string.h"

// 创建引用追踪器
SZrReferenceTracker *ZrLanguageServer_ReferenceTracker_New(SZrState *state, SZrSymbolTable *symbolTable) {
    if (state == ZR_NULL || symbolTable == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrReferenceTracker *tracker = (SZrReferenceTracker *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrReferenceTracker));
    if (tracker == ZR_NULL) {
        return ZR_NULL;
    }
    
    tracker->state = state;
    tracker->symbolTable = symbolTable;
    ZrCore_Array_Init(state, &tracker->allReferences, sizeof(SZrReference *), 16);
    
    // 初始化哈希表（使用符号名称作为键）
    tracker->symbolToReferencesMap.buckets = ZR_NULL;
    tracker->symbolToReferencesMap.bucketSize = 0;
    tracker->symbolToReferencesMap.elementCount = 0;
    tracker->symbolToReferencesMap.capacity = 0;
    tracker->symbolToReferencesMap.resizeThreshold = 0;
    tracker->symbolToReferencesMap.isValid = ZR_FALSE;
    ZrCore_HashSet_Init(state, &tracker->symbolToReferencesMap, 4); // 4 = 16 buckets
    
    return tracker;
}

// 释放引用追踪器
void ZrLanguageServer_ReferenceTracker_Free(SZrState *state, SZrReferenceTracker *tracker) {
    if (state == ZR_NULL || tracker == ZR_NULL) {
        return;
    }
    
    // 释放所有引用
    for (TZrSize i = 0; i < tracker->allReferences.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&tracker->allReferences, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *refPtr, sizeof(SZrReference));
        }
    }
    
    ZrCore_Array_Free(state, &tracker->allReferences);
    
    // 释放哈希表中的引用数组和节点
    if (tracker->symbolToReferencesMap.isValid && tracker->symbolToReferencesMap.buckets != ZR_NULL) {
        for (TZrSize i = 0; i < tracker->symbolToReferencesMap.capacity; i++) {
            SZrHashKeyValuePair *pair = tracker->symbolToReferencesMap.buckets[i];
            while (pair != ZR_NULL) {
                // 释放节点中存储的数据
                if (pair->key.type != ZR_VALUE_TYPE_NULL) {
                    if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                        SZrArray *refArray = 
                            (SZrArray *)pair->value.value.nativeObject.nativePointer;
                        if (refArray != ZR_NULL && refArray->isValid) {
                            ZrCore_Array_Free(state, refArray);
                            ZrCore_Memory_RawFree(state->global, refArray, sizeof(SZrArray));
                        }
                    }
                }
                // 释放节点本身
                SZrHashKeyValuePair *next = pair->next;
                ZrCore_Memory_RawFreeWithType(state->global, pair, sizeof(SZrHashKeyValuePair), 
                                       ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                pair = next;
            }
            tracker->symbolToReferencesMap.buckets[i] = ZR_NULL;
        }
        // 释放 buckets 数组
        ZrCore_HashSet_Deconstruct(state, &tracker->symbolToReferencesMap);
    }
    
    ZrCore_Memory_RawFree(state->global, tracker, sizeof(SZrReferenceTracker));
}

// 添加引用
TZrBool ZrLanguageServer_ReferenceTracker_AddReference(SZrState *state, 
                                     SZrReferenceTracker *tracker,
                                     SZrSymbol *symbol,
                                     SZrFileRange location,
                                     EZrReferenceType type) {
    if (state == ZR_NULL || tracker == ZR_NULL || symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 创建引用
    SZrReference *reference = (SZrReference *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrReference));
    if (reference == ZR_NULL) {
        return ZR_FALSE;
    }
    
    reference->symbol = symbol;
    reference->location = location;
    reference->type = type;
    
    // 添加到所有引用数组
    ZrCore_Array_Push(state, &tracker->allReferences, &reference);
    
    // 实现哈希表映射用于快速查找
    if (tracker->symbolToReferencesMap.isValid) {
        // 使用符号名称作为键
        SZrTypeValue key;
        ZrCore_Value_InitAsRawObject(state, &key, &symbol->name->super);
        
        // 查找或创建引用数组
        SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &tracker->symbolToReferencesMap, &key);
        SZrArray *refArray = ZR_NULL;
        
        if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            // 已存在，获取数组
            refArray = (SZrArray *)pair->value.value.nativeObject.nativePointer;
        } else {
            // 创建新数组
            refArray = (SZrArray *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrArray));
            if (refArray != ZR_NULL) {
                ZrCore_Array_Init(state, refArray, sizeof(SZrReference *), 4);
                
                // 添加到哈希表
                pair = ZrCore_HashSet_Add(state, &tracker->symbolToReferencesMap, &key);
                if (pair != ZR_NULL) {
                    SZrTypeValue value;
                    ZrCore_Value_InitAsNativePointer(state, &value, (TZrPtr)refArray);
                    ZrCore_Value_Copy(state, &pair->value, &value);
                }
            }
        }
        
        // 将引用添加到数组
        if (refArray != ZR_NULL) {
            ZrCore_Array_Push(state, refArray, &reference);
        }
    }
    
    // 同时添加到符号的引用列表
    ZrLanguageServer_Symbol_AddReference(state, symbol, location);
    
    return ZR_TRUE;
}

// 查找所有引用
TZrBool ZrLanguageServer_ReferenceTracker_FindReferences(SZrState *state,
                                       SZrReferenceTracker *tracker,
                                       SZrSymbol *symbol,
                                       SZrArray *result) {
    if (state == ZR_NULL || tracker == ZR_NULL || symbol == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrReference *), 4);
    }
    
    // 使用哈希表快速查找
    if (tracker->symbolToReferencesMap.isValid) {
        SZrTypeValue key;
        ZrCore_Value_InitAsRawObject(state, &key, &symbol->name->super);
        
        SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &tracker->symbolToReferencesMap, &key);
        if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            SZrArray *refArray = (SZrArray *)pair->value.value.nativeObject.nativePointer;
            if (refArray != ZR_NULL && refArray->isValid) {
                // 从数组中获取所有引用
                for (TZrSize i = 0; i < refArray->length; i++) {
                    SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(refArray, i);
                    if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
                        SZrReference *ref = *refPtr;
                        // 验证符号匹配（因为可能有同名符号）
                        if (ref->symbol == symbol) {
                            ZrCore_Array_Push(state, result, refPtr);
                        }
                    }
                }
                return ZR_TRUE;
            }
        }
    }
    
    // 回退到线性查找
    for (TZrSize i = 0; i < tracker->allReferences.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&tracker->allReferences, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            if (ref->symbol == symbol) {
                ZrCore_Array_Push(state, result, refPtr);
            }
        }
    }
    
    return ZR_TRUE;
}

// 获取引用计数
TZrSize ZrLanguageServer_ReferenceTracker_GetReferenceCount(SZrReferenceTracker *tracker,
                                            SZrSymbol *symbol) {
    if (tracker == ZR_NULL || symbol == ZR_NULL) {
        return 0;
    }
    
    return ZrLanguageServer_Symbol_GetReferenceCount(symbol);
}

// 查找位置处的引用
SZrReference *ZrLanguageServer_ReferenceTracker_FindReferenceAt(SZrReferenceTracker *tracker,
                                                 SZrFileRange position) {
    if (tracker == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 遍历所有引用查找匹配的位置
    for (TZrSize i = 0; i < tracker->allReferences.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&tracker->allReferences, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            // 实现精确的位置比较
            // 检查位置是否完全匹配或包含在引用范围内
            SZrFileRange refRange = ref->location;
            SZrFileRange posRange = position;
            
            // 首先检查源文件是否相同
            if (refRange.source != posRange.source) {
                continue;
            }
            
            // 检查位置是否在引用范围内（包含边界）
            // 位置在引用范围内：ref.start <= pos.start && pos.end <= ref.end
            TZrBool startMatch = (refRange.start.offset <= posRange.start.offset);
            TZrBool endMatch = (posRange.end.offset <= refRange.end.offset);
            
            // 如果offset为0，使用行号和列号比较
            if (refRange.start.offset == 0 && posRange.start.offset == 0) {
                startMatch = (refRange.start.line < posRange.start.line) ||
                            (refRange.start.line == posRange.start.line && 
                             refRange.start.column <= posRange.start.column);
                endMatch = (posRange.end.line < refRange.end.line) ||
                          (posRange.end.line == refRange.end.line && 
                           posRange.end.column <= refRange.end.column);
            }
            
            if (startMatch && endMatch) {
                return ref;
            }
        }
    }
    
    return ZR_NULL;
}

// 获取符号的所有引用位置
TZrBool ZrLanguageServer_ReferenceTracker_GetReferenceLocations(SZrState *state,
                                              SZrReferenceTracker *tracker,
                                              SZrSymbol *symbol,
                                              SZrArray *result) {
    if (state == ZR_NULL || tracker == ZR_NULL || symbol == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrFileRange), 4);
    }
    
    // 使用哈希表快速查找
    if (tracker->symbolToReferencesMap.isValid) {
        SZrTypeValue key;
        ZrCore_Value_InitAsRawObject(state, &key, &symbol->name->super);
        
        SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &tracker->symbolToReferencesMap, &key);
        if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            SZrArray *refArray = (SZrArray *)pair->value.value.nativeObject.nativePointer;
            if (refArray != ZR_NULL && refArray->isValid) {
                // 从数组中获取所有引用的位置
                for (TZrSize i = 0; i < refArray->length; i++) {
                    SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(refArray, i);
                    if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
                        SZrReference *ref = *refPtr;
                        // 验证符号匹配（因为可能有同名符号）
                        if (ref->symbol == symbol) {
                            SZrFileRange range = ref->location;
                            ZrCore_Array_Push(state, result, &range);
                        }
                    }
                }
                return ZR_TRUE;
            }
        }
    }
    
    // 回退到线性查找
    for (TZrSize i = 0; i < tracker->allReferences.length; i++) {
        SZrReference **refPtr = (SZrReference **)ZrCore_Array_Get(&tracker->allReferences, i);
        if (refPtr != ZR_NULL && *refPtr != ZR_NULL) {
            SZrReference *ref = *refPtr;
            if (ref->symbol == symbol) {
                SZrFileRange range = ref->location;
                ZrCore_Array_Push(state, result, &range);
            }
        }
    }
    
    return ZR_TRUE;
}
