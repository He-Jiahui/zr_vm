/**
 * @file
 * @brief 声明反射服务 `resolve` 导出项使用的内部 native 接口。
 *
 * 该入口随目标 MetadataRuntime 绑定到 `zr.reflection` 模块；模块创建和缓存校验均使用这些符号。
 */
#ifndef ZR_VM_REFLECTION_TYPE_RESOLVE_NATIVE_INTERNAL_H
#define ZR_VM_REFLECTION_TYPE_RESOLVE_NATIVE_INTERNAL_H

#include "zr_vm_core/reflection.h"

/**
 * @brief 将一个 TypeId 对象解析为反射 descriptor，并写回 native 调用的唯一结果槽。
 * @pre `state` 和当前 call info 描述有效的 native frame，参数布局为 closure 加一个 TypeId 对象。
 * @param state 正在执行此 native closure 的 VM 线程状态。
 * @return 有可写结果槽时返回一个结果（descriptor 或 null）并返回 1；缺少有效 state、call info 或函数槽时返回 0。
 * @note 有效函数槽存在时，无法恢复有效 bound runtime、参数形状不符或 TypeId 无法解析会写出 null；
 *       TypeId 是否须属于 bound runtime 仍待确认，见实现中的 TODO。
 */
TZrInt64 ZrCore_Reflection_ResolveTypeIdNativeEntryInternal(struct SZrState *state);

/**
 * @brief 创建绑定指定 MetadataRuntime 的 TypeId 解析 native closure。
 * @pre `state`、`runtime` 及 `runtime->module` 必须描述仍有效的运行时元数据。
 * @param state 创建 closure 并暂存 GC root 的 VM 线程状态。
 * @param runtime 要由 closure 捕获的元数据运行时。
 * @param pinRuntimeModule 是否请求额外保留 runtime module 的 native-handle pin。
 * @return 成功时返回 closure，失败时返回 null。
 * @note 调用方应在可能触发 GC 的后续操作前将返回对象放入受追踪的 root；本模块构造路径会立即这样做。
 */
struct SZrClosureNative *ZrCore_Reflection_CreateResolveTypeIdNativeClosureInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule);

#endif
