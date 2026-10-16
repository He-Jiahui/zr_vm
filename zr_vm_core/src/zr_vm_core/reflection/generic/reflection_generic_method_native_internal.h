/**
 * @file
 * @brief 声明 reflection 模块构造 MakeGenericMethod 导出所用的内部 native 工厂。
 *
 * 模块构造路径用该接口创建捕获目标 MetadataRuntime 的 closure，再将它注册为公开导出。
 */
#ifndef ZR_VM_REFLECTION_GENERIC_METHOD_NATIVE_INTERNAL_H
#define ZR_VM_REFLECTION_GENERIC_METHOD_NATIVE_INTERNAL_H

#include "zr_vm_core/reflection.h"

/**
 * @brief 创建绑定指定 MetadataRuntime 的 MakeGenericMethod native closure。
 * @pre `state`、`runtime` 及 `runtime->module` 必须有效，且该 module 持有此 runtime。
 * @param state 创建 closure 时使用的 VM 线程状态。
 * @param runtime closure 要捕获的目标元数据运行时。
 * @param pinRuntimeModule 是否请求额外保留 runtime module 的 native-handle pin。
 * @return 创建成功时返回 closure；参数或运行时模块校验失败时返回 null。
 * @note 返回的 closure 不再占用创建时的临时 root；调用方须在可能触发 GC 的操作前将其放入受追踪 root。
 */
struct SZrClosureNative *ZrCore_Reflection_CreateMakeGenericMethodNativeClosureInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule);

#endif
