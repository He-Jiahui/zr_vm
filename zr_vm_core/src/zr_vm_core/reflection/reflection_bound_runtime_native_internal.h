#ifndef ZR_VM_REFLECTION_BOUND_RUNTIME_NATIVE_INTERNAL_H
#define ZR_VM_REFLECTION_BOUND_RUNTIME_NATIVE_INTERNAL_H

#include "zr_vm_core/reflection.h"
#include "zr_vm_core/value.h"

/**
 * @brief 构造绑定 runtime module 的 native closure，供四个反射导出工厂共用。
 * @pre state、runtime、runtime->module 和回调均须有效，module 必须持有该 runtime。
 * @note 返回后临时栈根已撤销；调用方应在下次可能触发 GC 前将 closure 放入受追踪根。
 * @note pinRuntimeModule 请求额外 native-handle pin，供宿主直接持有 closure 的公开工厂使用。
 */
struct SZrClosureNative *ZrCore_Reflection_CreateBoundRuntimeNativeClosureInternal(
        struct SZrState *state,
        struct SZrMetadataRuntime *runtime,
        FZrNativeFunction nativeFunction,
        TZrBool pinRuntimeModule);

/**
 * @brief 校验 closure 的函数身份、已闭合捕获和 runtime/module 关系。
 * @note 模块导出与缓存验证传入预期 module；native 调用入口用函数身份校验后读取捕获。
 */
TZrBool ZrCore_Reflection_BoundRuntimeNativeClosureIsValidInternal(
        struct SZrState *state,
        struct SZrClosureNative *closure,
        FZrNativeFunction expectedFunction,
        struct SZrObjectModule *expectedRuntimeModule);

/**
 * @brief 从当前 native frame 的 closure 恢复经验证的绑定 runtime。
 * @pre 当前调用帧的函数槽应保存预期 native closure；无法验证时返回 null。
 * @note 参数读取和反射操作由调用该接口的 native 入口负责。
 */
struct SZrMetadataRuntime *ZrCore_Reflection_GetBoundRuntimeFromCallInternal(
        struct SZrState *state,
        FZrNativeFunction expectedFunction);

#endif
