/**
 * @file
 * @brief 实现绑定 MetadataRuntime 的 MakeGenericMethod native 导出。
 *
 * 入口从 native frame 读取方法定义与泛型实参数组，验证 closure 捕获的 runtime，再委托反射对象层构造结果。
 */
#include "zr_vm_core/reflection.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#include "reflection_bound_runtime_native_internal.h"
#include "reflection_generic_method_native_internal.h"

/* 有可写的调用结果槽时用单个 null 表示失败；没有函数槽才返回零个结果。 */
static TZrInt64 reflection_make_generic_method_native_return_null(
        SZrState *state,
        TZrStackValuePointer functionBase) {
    if (state == ZR_NULL || functionBase == ZR_NULL) {
        return 0;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

/**
 * @brief 用 native frame 中的方法定义和泛型实参数组构造闭合泛型方法对象。
 * @pre 当前 call info 描述有效 native frame；槽位依次为 closure、方法定义对象、泛型实参数组。
 * @param state 正在执行该 native closure 的 VM 线程状态。
 * @return 结果槽可写时返回一个结果并返回 1，值为构造对象或 null；state、call info 或函数槽不可用时返回 0。
 * @note 入口验证 closure 身份与 bound runtime；`MakeGenericMethodFromObjects` 校验定义/runtime/实参并清理输入 pin 与临时 arena，成功对象随后直接写入函数槽。
 */
TZrInt64 ZrCore_Reflection_MakeGenericMethodNativeEntry(SZrState *state) {
    TZrStackValuePointer functionBase;
    SZrTypeValue *definitionValue;
    SZrTypeValue *argumentsValue;
    SZrMetadataRuntime *runtime;
    SZrObject *constructedMethod;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    if (functionBase == ZR_NULL || state->stackTop.valuePointer != functionBase + 3) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }

    definitionValue = ZrCore_Stack_GetValue(functionBase + 1);
    argumentsValue = ZrCore_Stack_GetValue(functionBase + 2);
    if (definitionValue == ZR_NULL || definitionValue->type != ZR_VALUE_TYPE_OBJECT ||
        definitionValue->value.object == ZR_NULL ||
        definitionValue->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT ||
        argumentsValue == ZR_NULL || argumentsValue->type != ZR_VALUE_TYPE_ARRAY ||
        argumentsValue->value.object == ZR_NULL ||
        argumentsValue->value.object->type != ZR_RAW_OBJECT_TYPE_ARRAY) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }

    runtime = ZrCore_Reflection_GetBoundRuntimeFromCallInternal(
            state, ZrCore_Reflection_MakeGenericMethodNativeEntry);
    if (runtime == ZR_NULL) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }
    constructedMethod = ZrCore_Reflection_MakeGenericMethodFromObjects(
            state,
            runtime,
            (SZrObject *)definitionValue->value.object,
            (SZrObject *)argumentsValue->value.object);
    if (constructedMethod == ZR_NULL) {
        return reflection_make_generic_method_native_return_null(state, functionBase);
    }

    ZrCore_Value_InitAsRawObject(
            state,
            ZrCore_Stack_GetValue(functionBase),
            ZR_CAST_RAW_OBJECT_AS_SUPER(constructedMethod));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

/**
 * @brief 创建可选 pin 目标 runtime module 的内部 MakeGenericMethod closure。
 * @param state 创建 closure 时使用的 VM 线程状态。
 * @param runtime closure 捕获的目标元数据运行时。
 * @param pinRuntimeModule 是否请求保留 runtime module 的 native-handle pin。
 * @return 创建成功时返回 closure；校验或分配失败时返回 null。
 * @note closure 捕获和 module 有效性检查委托给共享 bound-runtime 工厂；成功返回后调用方须尽快将 closure 放入受追踪 root。
 */
SZrClosureNative *ZrCore_Reflection_CreateMakeGenericMethodNativeClosureInternal(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule) {
    return ZrCore_Reflection_CreateBoundRuntimeNativeClosureInternal(
            state,
            runtime,
            ZrCore_Reflection_MakeGenericMethodNativeEntry,
            pinRuntimeModule);
}

/**
 * @brief 创建会 pin 所属 runtime module 的公开 MakeGenericMethod native closure。
 * @pre `state`、`runtime` 及其所属 module 必须有效。
 * @param state 创建 closure 时使用的 VM 线程状态。
 * @param runtime closure 捕获的目标元数据运行时。
 * @return 创建成功时返回 closure；运行时模块无效或分配失败时返回 null。
 * @note 成功时目标 module 取得 native-handle pin；调用方仍须在后续可能触发 GC 的操作前 root 返回的 closure。
 */
SZrClosureNative *ZrCore_Reflection_CreateMakeGenericMethodNativeClosure(
        SZrState *state,
        SZrMetadataRuntime *runtime) {
    return ZrCore_Reflection_CreateMakeGenericMethodNativeClosureInternal(
            state, runtime, ZR_TRUE);
}
