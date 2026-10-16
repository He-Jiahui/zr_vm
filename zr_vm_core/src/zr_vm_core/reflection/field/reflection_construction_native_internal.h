#ifndef ZR_VM_REFLECTION_CONSTRUCTION_NATIVE_INTERNAL_H
#define ZR_VM_REFLECTION_CONSTRUCTION_NATIVE_INTERNAL_H

#include "zr_vm_core/reflection.h"

/**
 * @brief 检查反射 descriptor 是否可构造，并在成功时返回该 descriptor 供链式调用。
 * @pre 原生帧形状为闭包槽和一个 descriptor 参数；闭包须绑定此 entry。
 * @param state 保存当前 native call frame 的 VM 状态。
 * @return 有可写结果槽时返回一个 descriptor 或 null；帧不可用时返回零个结果。
 */
TZrInt64 ZrCore_Reflection_RequireConstructibleNativeEntryInternal(
        struct SZrState *state);

/**
 * @brief 创建 descriptor 所描述的实例，并把原生返回值放入结果槽。
 * @pre 原生帧至少含闭包槽和 descriptor 参数；其后各槽按顺序作为构造器参数。
 * @param state 保存当前 native call frame 的 VM 状态。
 * @return 有可写结果槽时返回一个实例或 null；帧不可用时返回零个结果。
 */
TZrInt64 ZrCore_Reflection_CreateInstanceNativeEntryInternal(
        struct SZrState *state);

/**
 * @brief 创建绑定指定 metadata runtime 的构造守卫 native closure。
 * @pre state、runtime 及其所属 module 有效；调用方须在后续分配前 root 返回 closure。
 * @param state 当前 VM 状态。
 * @param runtime closure 捕获的目标 metadata runtime。
 * @param pinRuntimeModule 是否由共享 closure 工厂额外固定 runtime module。
 * @return 创建失败时返回 null。
 */
struct SZrClosureNative *
ZrCore_Reflection_CreateRequireConstructibleNativeClosureInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule);

/**
 * @brief 创建绑定指定 metadata runtime 的 createInstance native closure。
 * @pre state、runtime 及其所属 module 有效；调用方须在后续分配前 root 返回 closure。
 * @param state 当前 VM 状态。
 * @param runtime closure 捕获的目标 metadata runtime。
 * @param pinRuntimeModule 是否由共享 closure 工厂额外固定 runtime module。
 * @return 创建失败时返回 null。
 */
struct SZrClosureNative *
ZrCore_Reflection_CreateInstanceNativeClosureInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule);

#endif
