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

/** @brief 建立带 GC 和注册表的独立测试状态；由 destroyTestState 释放。 */
SZrState* createTestState(void);
/** @brief 释放 createTestState 返回的全局状态及其 GC 对象。 */
void destroyTestState(SZrState* state);

/** @brief 通过真实 GC 分配路径创建测试对象，返回值归测试状态管理。 */
SZrRawObject* createTestObject(SZrState* state, EZrValueType type, TZrSize size);
/** @brief 创建并清空可供标记器扫描的 native-data 值槽。
 *  @pre valueCount 至少为 1，尾部数组的分配大小按 valueCount - 1 计算。
 */
struct SZrNativeData* createTestNativeData(SZrState* state, TZrUInt32 valueCount);

#endif // ZR_VM_GC_TEST_UTILS_H
