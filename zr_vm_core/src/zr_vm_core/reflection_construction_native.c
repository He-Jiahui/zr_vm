#include "reflection_construction_native_internal.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#include "reflection_bound_runtime_native_internal.h"

/* 有可写结果槽时统一收敛为一个 null 返回；没有有效帧基址时无法写槽，返回零结果。 */
static TZrInt64 construction_native_return_null(
        SZrState *state,
        TZrStackValuePointer functionBase) {
    if (state == ZR_NULL || functionBase == ZR_NULL) {
        return 0;
    }
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

/* 这里只挡住非对象值和非对象 raw 类型；反射对象身份由下层公开构造 API 再校验。 */
static SZrObject *construction_native_descriptor(
        SZrState *state,
        TZrStackValuePointer valuePointer) {
    SZrTypeValue *value;

    if (state == ZR_NULL || valuePointer == ZR_NULL) {
        return ZR_NULL;
    }
    value = ZrCore_Stack_GetValue(valuePointer);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT ||
        value->value.object == ZR_NULL ||
        value->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(state, value->value.object);
}

/* 原生帧限定为 [闭包, descriptor]，并校验该闭包绑定的正是当前导出入口。 */
TZrInt64 ZrCore_Reflection_RequireConstructibleNativeEntryInternal(
        SZrState *state) {
    TZrStackValuePointer functionBase;
    SZrObject *descriptor;
    EZrReflectionConstructionStatus status;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }
    functionBase = state->callInfoList->functionBase.valuePointer;
    /* 该导出只接受一个 descriptor；多余实参不属于 requireConstructible 的调用形状。 */
    if (functionBase == ZR_NULL ||
        state->stackTop.valuePointer != functionBase + 2 ||
        ZrCore_Reflection_GetBoundRuntimeFromCallInternal(
                state,
                ZrCore_Reflection_RequireConstructibleNativeEntryInternal) ==
                ZR_NULL) {
        return construction_native_return_null(state, functionBase);
    }
    descriptor = construction_native_descriptor(state, functionBase + 1);
    if (descriptor == ZR_NULL ||
        !ZrCore_Reflection_RequireConstructible(
                state, descriptor, &status)) {
        return construction_native_return_null(state, functionBase);
    }
    /* 守卫成功时保留实参本身，供语言侧继续链式调用 createInstance。 */
    *ZrCore_Stack_GetValue(functionBase) =
            *ZrCore_Stack_GetValue(functionBase + 1);
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

/* descriptor 后的槽位都是构造实参；下层同步完成构造，适配层再写结果并收缩为一个返回槽。 */
TZrInt64 ZrCore_Reflection_CreateInstanceNativeEntryInternal(
        SZrState *state) {
    TZrStackValuePointer functionBase;
    SZrObject *descriptor;
    TZrSize argumentCount;
    SZrTypeValue result;
    EZrReflectionConstructionStatus status;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }
    functionBase = state->callInfoList->functionBase.valuePointer;
    /* createInstance 接受 descriptor 后的连续实参槽；至少需要闭包槽和 descriptor。 */
    if (functionBase == ZR_NULL ||
        state->stackTop.valuePointer < functionBase + 2 ||
        ZrCore_Reflection_GetBoundRuntimeFromCallInternal(
                state,
                ZrCore_Reflection_CreateInstanceNativeEntryInternal) == ZR_NULL) {
        return construction_native_return_null(state, functionBase);
    }
    descriptor = construction_native_descriptor(state, functionBase + 1);
    argumentCount = (TZrSize)(state->stackTop.valuePointer - functionBase - 2);
    ZrCore_Value_ResetAsNull(&result);
    /* 参数仅在本次同步构造中借用；下层负责构造器选择、实例固定及异常收尾。 */
    /* BUG: 自定义构造器可扩展并搬迁 VM 栈；下层恢复 savedCallInfo 锚点，但本地 functionBase 未重算，随后结果/null 写入会用旧地址。 */
    if (descriptor == ZR_NULL ||
        !ZrCore_Reflection_CreateInstance(
                state,
                descriptor,
                argumentCount > 0u
                        ? ZrCore_Stack_GetValue(functionBase + 2)
                        : ZR_NULL,
                argumentCount,
                &result,
                &status)) {
        return construction_native_return_null(state, functionBase);
    }
    /* 原生返回约定只保留结果槽；失败路径由统一 helper 写入 null。 */
    *ZrCore_Stack_GetValue(functionBase) = result;
    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

/* requireConstructible 闭包绑定守卫 entry；共享工厂负责闭包 root、runtime 捕获和 pin 策略。 */
SZrClosureNative *
ZrCore_Reflection_CreateRequireConstructibleNativeClosureInternal(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule) {
    return ZrCore_Reflection_CreateBoundRuntimeNativeClosureInternal(
            state,
            runtime,
            ZrCore_Reflection_RequireConstructibleNativeEntryInternal,
            pinRuntimeModule);
}

/* createInstance 闭包绑定构造 entry；runtime 捕获与模块固定仍由共享工厂统一处理。 */
SZrClosureNative *ZrCore_Reflection_CreateInstanceNativeClosureInternal(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrBool pinRuntimeModule) {
    return ZrCore_Reflection_CreateBoundRuntimeNativeClosureInternal(
            state,
            runtime,
            ZrCore_Reflection_CreateInstanceNativeEntryInternal,
            pinRuntimeModule);
}
