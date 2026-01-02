//
// Created by AI Assistant on 2026/1/2.
//

#ifndef ZR_VM_GC_TEST_UTILS_H
#define ZR_VM_GC_TEST_UTILS_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/native.h"

// 测试工具函数声明
SZrState* createTestState(void);
void destroyTestState(SZrState* state);

SZrRawObject* createTestObject(SZrState* state, EZrValueType type, TZrSize size);
struct SZrNativeData* createTestNativeData(SZrState* state, TUInt32 valueCount);

#endif // ZR_VM_GC_TEST_UTILS_H
