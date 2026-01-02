//
// Created by AI Assistant on 2026/1/2.
//

#include <stdlib.h>
#include <string.h>
#include "gc_test_utils.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

// 简单的测试分配器
static TZrPtr testAllocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);
    
    if (newSize == 0) {
        // 释放内存
        if (pointer != ZR_NULL) {
            // 检查指针是否在合理范围内（避免释放无效指针）
            if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
                free(pointer);
            }
            // 如果指针无效，不调用free，避免崩溃
        }
        return ZR_NULL;
    }
    
    if (pointer == ZR_NULL) {
        // 分配新内存
        return malloc(newSize);
    } else {
        // 重新分配内存
        // 检查指针是否在合理范围内（避免realloc无效指针）
        if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
            return realloc(pointer, newSize);
        } else {
            // 无效指针，分配新内存
            return malloc(newSize);
        }
    }
}

// 创建测试用的SZrState
SZrState* createTestState() {
    // 创建全局状态
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState* global = ZrGlobalStateNew(testAllocator, ZR_NULL, 12345, &callbacks);
    if (!global) return NULL;
    
    // 初始化注册表
    SZrState* mainState = global->mainThreadState;
    if (mainState) {
        ZrGlobalStateInitRegistry(mainState, global);
    }
    
    return mainState;
}

// 销毁测试用的SZrState
void destroyTestState(SZrState* state) {
    if (!state) return;
    
    SZrGlobalState* global = state->global;
    if (global) {
        // 释放全局状态（会自动释放GC和所有资源）
        ZrGlobalStateFree(global);
    }
}

// 创建测试对象
SZrRawObject* createTestObject(SZrState* state, EZrValueType type, TZrSize size) {
    if (!state || !state->global) return NULL;
    
    return ZrRawObjectNew(state, type, size, ZR_FALSE);
}

// 创建测试用的Native Data对象
struct SZrNativeData* createTestNativeData(SZrState* state, TUInt32 valueCount) {
    if (!state || !state->global) return NULL;
    
    TZrSize totalSize = sizeof(struct SZrNativeData) + (valueCount - 1) * sizeof(SZrTypeValue);
    SZrRawObject* rawObj = ZrRawObjectNew(state, ZR_VALUE_TYPE_NATIVE_DATA, totalSize, ZR_FALSE);
    if (!rawObj) return NULL;
    
    struct SZrNativeData* nativeData = ZR_CAST(struct SZrNativeData *, rawObj);
    nativeData->valueLength = valueCount;
    
    // 初始化值数组
    for (TUInt32 i = 0; i < valueCount; i++) {
        ZrValueResetAsNull(&nativeData->valueExtend[i]);
    }
    
    return nativeData;
}
