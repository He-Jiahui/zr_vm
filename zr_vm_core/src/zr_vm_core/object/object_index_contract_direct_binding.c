//
// 为缓存索引契约提供受限的同步原生回调直派遣与上下文构造。
//

#include "object/object_call_internal.h"

#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_binding.h"
#include <string.h>

enum {
    ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS =
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT,
    ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_NO_RESULT_FLAGS =
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT |
            ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL
};

static ZR_FORCE_INLINE TZrUInt32 object_index_contract_dispatch_flags(
        const SZrObjectKnownNativeDirectDispatch *dispatch) {
    if (dispatch == ZR_NULL) {
        return 0u;
    }
    if (dispatch->functionDescriptor != ZR_NULL) {
        return ((const ZrLibFunctionDescriptor *)dispatch->functionDescriptor)
                ->dispatchFlags;
    }
    if (dispatch->methodDescriptor != ZR_NULL) {
        return ((const ZrLibMethodDescriptor *)dispatch->methodDescriptor)
                ->dispatchFlags;
    }
    if (dispatch->metaMethodDescriptor != ZR_NULL) {
        return ((const ZrLibMetaMethodDescriptor *)
                        dispatch->metaMethodDescriptor)
                ->dispatchFlags;
    }
    return 0u;
}

static ZR_FORCE_INLINE TZrBool object_index_contract_native_enter(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *dispatch) {
    TZrUInt32 modeFlags = object_index_contract_dispatch_flags(dispatch) &
                          (TZrUInt32)
                                  ZR_LIB_NATIVE_DISPATCH_FLAG_SAFEPOINT_MODE_MASK;
    EZrGcNativeSafepointMode mode;

    if (modeFlags == 0u) {
        mode = ZR_GC_NATIVE_SAFEPOINT_MODE_GC_AWARE;
    } else if (modeFlags ==
               (TZrUInt32)ZR_LIB_NATIVE_DISPATCH_FLAG_BLOCKING_DETACHED) {
        mode = ZR_GC_NATIVE_SAFEPOINT_MODE_BLOCKING_DETACHED;
    } else if (modeFlags ==
               (TZrUInt32)ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SAFEPOINT_CRITICAL) {
        mode = ZR_GC_NATIVE_SAFEPOINT_MODE_NO_SAFEPOINT_CRITICAL;
    } else {
        return ZR_FALSE;
    }
    return ZrCore_GcDomain_NativeEnter(state, mode);
}

static ZR_FORCE_INLINE TZrBool object_index_contract_value_resides_on_vm_stack(const SZrState *state,
                                                                                const SZrTypeValue *value) {
    TZrStackValuePointer stackValue;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    stackValue = ZR_CAST(TZrStackValuePointer, value);
    return (TZrBool)(stackValue >= state->stackBase.valuePointer && stackValue < state->stackTail.valuePointer);
}

static ZR_FORCE_INLINE void object_index_contract_init_direct_binding_inline_value_context(
        ZrLibCallContext *context,
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *dispatch,
        SZrTypeValue *selfValue,
        SZrTypeValue *argumentValues,
        TZrSize argumentCount) {
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(dispatch != ZR_NULL);

    memset(context, 0, sizeof(*context));
    context->state = state;
    context->moduleDescriptor = (const struct ZrLibModuleDescriptor *)dispatch->moduleDescriptor;
    context->typeDescriptor = (const struct ZrLibTypeDescriptor *)dispatch->typeDescriptor;
    context->functionDescriptor = (const struct ZrLibFunctionDescriptor *)dispatch->functionDescriptor;
    context->methodDescriptor = (const struct ZrLibMethodDescriptor *)dispatch->methodDescriptor;
    context->metaMethodDescriptor = (const struct ZrLibMetaMethodDescriptor *)dispatch->metaMethodDescriptor;
    context->ownerPrototype = dispatch->ownerPrototype;
    context->constructTargetPrototype = context->ownerPrototype;
    context->functionBase = ZR_NULL;
    context->argumentBase = ZR_NULL;
    context->argumentValues = argumentValues;
    context->argumentValuePointers = ZR_NULL;
    context->argumentCount = argumentCount;
    context->selfValue = selfValue;
    context->functionBaseAnchor.offset = 0;
    context->stackBasePointer = state->stackBase.valuePointer;
    context->rawArgumentCount = (TZrSize)dispatch->rawArgumentCount;
    context->stackLayoutUsesReceiver = dispatch->usesReceiver;
    context->stackLayoutAnchored = ZR_FALSE;
}

static ZR_FORCE_INLINE void object_index_contract_init_direct_binding_inline_pointer_value_context(
        ZrLibCallContext *context,
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *dispatch,
        SZrTypeValue *selfValue,
        SZrTypeValue **argumentValuePointers,
        TZrSize argumentCount) {
    ZR_ASSERT(context != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);
    ZR_ASSERT(dispatch != ZR_NULL);

    memset(context, 0, sizeof(*context));
    context->state = state;
    context->moduleDescriptor = (const struct ZrLibModuleDescriptor *)dispatch->moduleDescriptor;
    context->typeDescriptor = (const struct ZrLibTypeDescriptor *)dispatch->typeDescriptor;
    context->functionDescriptor = (const struct ZrLibFunctionDescriptor *)dispatch->functionDescriptor;
    context->methodDescriptor = (const struct ZrLibMethodDescriptor *)dispatch->methodDescriptor;
    context->metaMethodDescriptor = (const struct ZrLibMetaMethodDescriptor *)dispatch->metaMethodDescriptor;
    context->ownerPrototype = dispatch->ownerPrototype;
    context->constructTargetPrototype = context->ownerPrototype;
    context->functionBase = ZR_NULL;
    context->argumentBase = ZR_NULL;
    context->argumentValues = ZR_NULL;
    context->argumentValuePointers = argumentValuePointers;
    context->argumentCount = argumentCount;
    context->selfValue = selfValue;
    context->functionBaseAnchor.offset = 0;
    context->stackBasePointer = state->stackBase.valuePointer;
    context->rawArgumentCount = (TZrSize)dispatch->rawArgumentCount;
    context->stackLayoutUsesReceiver = dispatch->usesReceiver;
    context->stackLayoutAnchored = ZR_FALSE;
}

TZrBool ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineOneArgumentStack(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        SZrTypeValue *result) {
    ZrLibCallContext context;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || directBindingDispatch == ZR_NULL || directBindingDispatch->callback == ZR_NULL ||
        receiver == ZR_NULL || argument0 == ZR_NULL || result == ZR_NULL || state->debugHookSignal != 0u ||
        directBindingDispatch->rawArgumentCount != 2u || !directBindingDispatch->usesReceiver ||
        (directBindingDispatch->reserved0 & ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS) !=
                ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS ||
        !object_index_contract_value_resides_on_vm_stack(state, receiver) ||
        !object_index_contract_value_resides_on_vm_stack(state, argument0)) {
        return ZR_FALSE;
    }
    if (!object_index_contract_native_enter(state, directBindingDispatch)) {
        return ZR_FALSE;
    }

    if (directBindingDispatch->readonlyInlineGetFastCallback != ZR_NULL) {
        success = directBindingDispatch->readonlyInlineGetFastCallback(state, receiver, argument0, result);
        ZrCore_GcDomain_NativeLeave(state);
        if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
            ZrCore_Value_ResetAsNullNoProfile(result);
            success = ZR_TRUE;
        }
        return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
    }

    object_index_contract_init_direct_binding_inline_value_context(&context,
                                                                   state,
                                                                   directBindingDispatch,
                                                                   (SZrTypeValue *)receiver,
                                                                   (SZrTypeValue *)argument0,
                                                                   1u);
    success = directBindingDispatch->callback(&context, result);
    ZrCore_GcDomain_NativeLeave(state);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        ZrCore_Value_ResetAsNullNoProfile(result);
        success = ZR_TRUE;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

TZrBool ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineTwoArgumentsNoResultStack(
        SZrState *state,
        const SZrObjectKnownNativeDirectDispatch *directBindingDispatch,
        const SZrTypeValue *receiver,
        const SZrTypeValue *argument0,
        const SZrTypeValue *argument1) {
    ZrLibCallContext context;
    SZrTypeValue *argumentValuePointers[2];
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || directBindingDispatch == ZR_NULL || directBindingDispatch->callback == ZR_NULL ||
        receiver == ZR_NULL || argument0 == ZR_NULL || argument1 == ZR_NULL || state->debugHookSignal != 0u ||
        directBindingDispatch->rawArgumentCount != 3u || !directBindingDispatch->usesReceiver ||
        (directBindingDispatch->reserved0 & ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_NO_RESULT_FLAGS) !=
                ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_NO_RESULT_FLAGS ||
        !object_index_contract_value_resides_on_vm_stack(state, receiver) ||
        !object_index_contract_value_resides_on_vm_stack(state, argument0) ||
        !object_index_contract_value_resides_on_vm_stack(state, argument1)) {
        return ZR_FALSE;
    }
    if (!object_index_contract_native_enter(state, directBindingDispatch)) {
        return ZR_FALSE;
    }

    if (directBindingDispatch->readonlyInlineSetNoResultFastCallback != ZR_NULL) {
        success = directBindingDispatch->readonlyInlineSetNoResultFastCallback(state, receiver, argument0, argument1);
        ZrCore_GcDomain_NativeLeave(state);
        if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
            success = ZR_TRUE;
        }
        return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
    }

    if (argument1 == argument0 + 1) {
        object_index_contract_init_direct_binding_inline_value_context(&context,
                                                                       state,
                                                                       directBindingDispatch,
                                                                       (SZrTypeValue *)receiver,
                                                                       (SZrTypeValue *)argument0,
                                                                       2u);
    } else {
        argumentValuePointers[0] = (SZrTypeValue *)argument0;
        argumentValuePointers[1] = (SZrTypeValue *)argument1;
        object_index_contract_init_direct_binding_inline_pointer_value_context(&context,
                                                                               state,
                                                                               directBindingDispatch,
                                                                               (SZrTypeValue *)receiver,
                                                                               argumentValuePointers,
                                                                               2u);
    }
    success = directBindingDispatch->callback(&context, ZR_NULL);
    ZrCore_GcDomain_NativeLeave(state);
    if (!success && state->threadStatus == ZR_THREAD_STATUS_FINE) {
        success = ZR_TRUE;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE && success;
}

/**
 * @file zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
 * ## Summary
 * 为满足固定形状的缓存索引契约提供窄化直派遣入口：先核对描述符承诺、接收者与参数所在的 VM 栈位置，
 * 再按描述符规定进入 GC safepoint 模式，最后同步调用只读 GET 或无结果 SET 回调。
 * 该文件不负责解析或缓存绑定；缓存解析由对象调用层完成，调用者只在已选定此契约时进入这里。
 *
 * ## Constraints and lifecycle
 * 所有回调参数、描述符指针、上下文和两参数非连续分支的指针数组都只在同步回调期间借用；回调返回后不得保留。
 * NativeEnter 成功后，正常返回路径由调用函数配对 NativeLeave；若调用位于 VM TryRun 边界内，非局部异常由该边界恢复 VM/GC scope，
 * 本文件不捕获异常，也不能把普通返回值归一化规则套用于异常状态。此实现不创建线程，也不提供异步回调生命周期。
 *
 * ## References
 * 内部声明见 `zr_vm_core/src/zr_vm_core/object/object_call_internal.h:69` 与 `:114`。
 * 当前对象路径在 `zr_vm_core/src/zr_vm_core/object/object.c:1172` 与 `:1215` 直接执行缓存 GET/SET callback，
 * 索引操作通过 `:3677`、`:3710`、`:3911`、`:3944` 到达这些对象层包装函数；这些不是本文件两个导出 helper 的直接调用点。
 * generic callback 的 descriptor 选择见 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h:311`；
 * descriptor 分类写入及可选专用回调字段见该文件 `:348`、`:351`、`:354`、`:356`、`:358`。
 */

/**
 * @defgroup object_index_contract_direct_binding_flag_masks 只读索引直派遣组合契约位
 * @{
 * ## Summary
 * 这两个匿名枚举值是本文件导出 helper 对 `reserved0` 做子集检查时使用的组合契约位；
 * 它们不是 safepoint mode。调用仍允许 `reserved0` 带有额外位，但所列每一位都必须存在。
 *
 * ## Fields
 * - `ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_RESULT_WRITTEN_FLAGS`：GET 必须禁止 self 重绑定，使用 inline-value 与 readonly-inline 上下文，并承诺写结果。
 * - `ZR_OBJECT_INDEX_DIRECT_BINDING_READONLY_INLINE_NO_RESULT_FLAGS`：SET 必须禁止 self 重绑定，使用 inline-value 与 readonly-inline 上下文，并允许省略结果。
 *
 * ## Constraints and lifecycle
 * safepoint mode 从 descriptor 的 `dispatchFlags` 单独读取；不能用这些 `reserved0` 组合位代替 GC 模式选择。
 * 组合位由调用者从匹配的函数/方法/元方法 descriptor 传播，必须与实际 callback 行为相符。
 *
 * ## References
 * 组合值定义在 `zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c:13`；
 * 快速形状就绪位由 `zr_vm_core/src/zr_vm_core/object/object_call.c:550` 与 `:565` 计算，
 * 对象调用层在 `zr_vm_core/src/zr_vm_core/object/object.c:967` 与 `:1007` 检查形状。
 * @}
 */

/**
 * @fn static TZrUInt32 object_index_contract_dispatch_flags(const SZrObjectKnownNativeDirectDispatch *dispatch)
 * ## Summary
 * 从已解析的直派遣记录中取出 descriptor 的 safepoint/回调契约位，供同文件的 NativeEnter 选择模式。
 * 函数、方法、元方法字段按固定优先级选首个非空 descriptor，避免把不同 descriptor 的 flags 混合。
 *
 * ## Parameters
 * @param[in] dispatch 借用的缓存派遣记录；可为空。非空时其中 descriptor 指针须仍指向有效的注册元数据。
 *
 * ## Returns
 * @return 首个非空函数、方法或元方法 descriptor 的 `dispatchFlags`；记录为空或没有 descriptor 时返回 `0u`。
 *
 * ## Constraints and lifecycle
 * 只读取记录与 descriptor，不分配、不改变缓存，也不延长 descriptor 生命周期；返回值是按值复制的位集合。
 * 若多个 descriptor 字段同时非空，仅函数字段优先于方法字段、方法字段优先于元方法字段。
 *
 * ## References
 * 由 `object_index_contract_native_enter` 在 `object_index_contract_direct_binding.c:50` 调用；
 * generic callback 和 descriptor 类型映射见 `native_binding_internal.h:311`、`:348`、`:351`、`:354`。
 */

/**
 * @fn static TZrBool object_index_contract_native_enter(SZrState *state, const SZrObjectKnownNativeDirectDispatch *dispatch)
 * ## Summary
 * 把 descriptor 的 safepoint mode 位转换为 GC 域的原生调用模式，并在 callback 前登记当前 state 的原生调用。
 *
 * ## Parameters
 * @param[in,out] state 正在执行该绑定的 VM state；必须是有效 state，并遵循其 mutator 的调用顺序；GC 原生域状态会随进入而改变。
 * @param[in] dispatch 借用的直派遣记录；由 `object_index_contract_dispatch_flags` 读取 descriptor flags。
 *
 * ## Returns
 * @return 若 mode 位组合受支持且 `ZrCore_GcDomain_NativeEnter` 成功则返回真；非法组合或进入失败返回假。
 * 返回真后必须在正常 callback 返回路径调用一次 `ZrCore_GcDomain_NativeLeave`。
 *
 * ## Constraints and lifecycle
 * mode 位为零时采用 GC-aware；唯一的 blocking-detached 与 no-safepoint-critical 组合映射到对应 GC 模式；
 * 其余组合在不进入原生域时失败。此 helper 不自行离开原生域；若 callback 在 VM TryRun 边界内非局部退出，由该边界还原 scope。descriptor/callback 必须遵守所选模式：BLOCKING_DETACHED 会使暂停扫描不再等候该 mutator，必须遵守 detached 期间不持有可移动引用的既有协议；传入 state/栈地址不构成访问 GC 引用的安全证明，本 helper 不检查 callback 是否满足该协议（gc_domain_mutator.c:170/:181/:685–687）。
 *
 * ## References
 * 由两个导出 helper 在 callback 前调用：`object_index_contract_direct_binding.c:165` 与 `:215`；
 * 正常 callback 返回后分别在 `:171`、`:186`、`:221`、`:246` 配对 NativeLeave。
 * 异常恢复入口见 `zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:19/:26 与 gc/gc_domain_scopes.c:60`。
 */

/**
 * @fn static ZR_FORCE_INLINE TZrBool object_index_contract_value_resides_on_vm_stack(const SZrState *state, const SZrTypeValue *value)
 * ## Summary
 * 在满足同数组指针比较前提时检查 VM 栈半开区间，作为两个导出 helper 的操作数范围门控；不承诺能安全拒绝任意非栈地址。
 *
 * ## Parameters
 * @param[in] state 要验证的 VM state；空指针返回假。
 * @param[in] value 待验证的值地址；空指针返回假，非空地址须满足本 helper 的 VM 栈值前置条件。
 *
 * ## Returns
 * @return 满足同数组比较前提时，位于 VM 栈有效半开区间返回真，空指针或在该数组有效范围外返回假；任意地址的安全拒绝策略见下方 TODO。
 *
 * ## Constraints and lifecycle
 * 本 helper 不复制或保活值，验证通过后仍由调用者保证操作数在同步 callback 完成前有效。
 * TODO: 当前范围检查对输入地址与栈指针使用关系比较；若未来调用方可能传入不属于同一 VM 栈数组的任意地址，
 * 需核查 C 指针比较的可移植性及安全拒绝策略。当前唯一直接 helper 测试传入 VM 栈地址，见 `test_object_call_known_native_fast_path.c:6348`。
 *
 * ## References
 * 被 GET 参数门控 `object_index_contract_direct_binding.c:161` 与 SET 参数门控 `:210`、`:211` 使用；
 * 对象层另有栈操作数检查 `zr_vm_core/src/zr_vm_core/object/object.c:596`。
 */

/**
 * @fn static ZR_FORCE_INLINE void object_index_contract_init_direct_binding_inline_value_context(ZrLibCallContext *context, SZrState *state, const SZrObjectKnownNativeDirectDispatch *dispatch, SZrTypeValue *selfValue, SZrTypeValue *argumentValues, TZrSize argumentCount)
 * ## Summary
 * 为通用 native callback 清零并填充 inline-value `ZrLibCallContext`，用于一个或多个连续 VM 栈参数。
 * 它只组装借用视图，不调用 callback、不复制值，也不创建堆对象。
 *
 * ## Parameters
 * @param[out] context 输出上下文；必须非空，函数以断言约束其有效性。
 * @param[in] state 提供 VM 栈基址及当前执行状态的有效 state。
 * @param[in] dispatch 已通过调用者门控的直派遣记录；其 descriptor 与 owner prototype 被复制为借用指针。
 * @param[in] selfValue 接收者的 VM 栈值地址；仅在 callback 执行期间借用。
 * @param[in] argumentValues 显式参数连续区域的首地址；以 `argumentCount` 指定读取数量。
 * @param[in] argumentCount callback 可见的显式参数个数，不包括作为 `selfValue` 提供的接收者。
 *
 * ## Returns
 * 函数通过 `context` 写出上下文，不返回状态；空的 `context`、`state` 或 `dispatch` 违反断言前置条件。
 *
 * ## Constraints and lifecycle
 * `rawArgumentCount` 从派遣记录复制，反映原始调用布局；`argumentCount` 是显式参数数量，两者语义不同。
 * `functionBase` 与 `argumentBase` 置空，`argumentValuePointers` 置空，`argumentValues` 指向调用者提供的连续栈区域；
 * descriptor、self、参数和 context 均只保证在同步 callback 期间可借用，callback 不得留存这些地址。
 * `constructTargetPrototype` 与 `ownerPrototype` 相同；该赋值不转移 prototype 所有权。
 *
 * ## References
 * 由 one-argument generic GET 在 `object_index_contract_direct_binding.c:179` 使用；
 * two-argument SET 的连续分支在 `:228` 使用。
 */

/**
 * @fn static ZR_FORCE_INLINE void object_index_contract_init_direct_binding_inline_pointer_value_context(ZrLibCallContext *context, SZrState *state, const SZrObjectKnownNativeDirectDispatch *dispatch, SZrTypeValue *selfValue, SZrTypeValue **argumentValuePointers, TZrSize argumentCount)
 * ## Summary
 * 为通用 native callback 组装 pointer-value 上下文，用于显式参数在 VM 栈上但不连续的情况。
 *
 * ## Parameters
 * @param[out] context 输出上下文；必须非空，函数以断言约束其有效性。
 * @param[in] state 提供 VM 栈基址及当前执行状态的有效 state。
 * @param[in] dispatch 已通过调用者门控的直派遣记录；descriptor 与 owner prototype 均为借用指针。
 * @param[in] selfValue 接收者 VM 栈值地址；只在同步 callback 中有效。
 * @param[in] argumentValuePointers 指向参数地址的数组；数组及每个值地址都须在 callback 完成前保持有效。
 * @param[in] argumentCount callback 可见的显式参数个数，不包括接收者。
 *
 * ## Returns
 * 函数通过 `context` 写出上下文，不返回状态；空的 `context`、`state` 或 `dispatch` 违反断言前置条件。
 *
 * ## Constraints and lifecycle
 * 本文件当前以导出 SET helper 的局部 `argumentValuePointers[2]` 提供该数组；数组本身位于 C 栈上，
 * 因而 callback 必须同步消费，不能保存 context 或其中任一指针供返回后使用。
 * `argumentValues` 置空，参数数量表示显式参数数目，`rawArgumentCount` 则从派遣记录复制。
 * 此 helper 不验证参数地址；调用者先逐个确认其位于 VM 栈范围内。
 *
 * ## References
 * 唯一当前调用点为 two-argument SET 非连续参数分支 `object_index_contract_direct_binding.c:238`；
 * 局部数组在 `:202` 声明，并由 `:236` 与 `:237` 分别写入两个参数地址。
 */

/**
 * @fn TZrBool ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineOneArgumentStack(SZrState *state, const SZrObjectKnownNativeDirectDispatch *directBindingDispatch, const SZrTypeValue *receiver, const SZrTypeValue *argument0, SZrTypeValue *result)
 * ## Summary
 * 尝试按只读 inline GET 索引契约同步调用缓存 native binding；形状不匹配时在调用 callback 前返回假，
 * 让上层继续使用普通路径。存在专用 fast callback 时直接传入 state/receiver/key/result，否则构造通用 value context。
 *
 * ## Parameters
 * @param[in,out] state 正在执行的有效 VM state；debug hook 未挂起，且 GC 域允许进入 descriptor 指定模式；NativeEnter/Leave 会更新其 GC 域状态。
 * @param[in] directBindingDispatch 缓存的 descriptor、generic callback、可选专用 GET callback 及契约位；
 * 必须非空且 callback 非空，即使专用 fast callback 已设置。
 * @param[in] receiver VM 栈中的接收者值；按只读 GET descriptor 契约借给 callback，不转移栈槽所有权。
 * @param[in] argument0 VM 栈中的唯一显式参数（索引键）；按 inline-value descriptor 契约借给 callback。
 * @param[out] result 调用方提供且必须存活到本 helper 返回的可写输出槽；只检查非空，不要求其位于 VM 栈，不建立临时根，也不保证与 receiver/key 不重叠。
 * TODO: 当前 helper 不检查结果槽与输入槽的别名，也不为局部输出建立临时根；需明确重叠是否受支持，以及 callback 写入 GC 引用后在其余 callback 操作和 NativeLeave 条件 Poll 前后由谁保持可达。核查入口是本文件 :157/:170/:185/:171/:186、gc_domain_mutator.c:717–725 和实际 callback 契约；唯一直接测试 :6344/:6371 以独立 C 局部槽接收整数 72，不能证明引用输出的 rooting 协议。
 *
 * ## Returns
 * @return 前置形状不满足或 NativeEnter 失败时返回假且不调用 callback；callback 正常返回后，若 VM thread status 为 FINE 则返回归一化结果。
 * callback 返回假但 thread status 仍为 FINE 时，本函数把 result 重置为 null 并返回真，表示索引未命中是成功的 null 结果；
 * thread status 非 FINE 时返回假，不执行该 null 归一化。
 *
 * ## Constraints and lifecycle
 * `rawArgumentCount` 必须为 2（含 receiver），`usesReceiver` 必须为真；`reserved0` 必须包含 RESULT_WRITTEN 组合的所有位，
 * 但可有额外位。receiver 与 argument0 必须落在该 state 的 VM 栈内，result 是调用方提供的可写输出槽。
 * callback 在成功 NativeEnter 后同步执行；描述符 safepoint 位只能选择 GC-aware、blocking-detached 或 no-safepoint-critical 的受支持模式。
 * 正常返回路径总是 NativeLeave；callback 非局部 VM 异常若被 VM TryRun 边界包围，则由该边界恢复，不转成普通 false/null 结果。
 * context/descriptor/input 值均为借用，不能跨 callback 保留。
 *
 * ## References
 * 内部声明见 `zr_vm_core/src/zr_vm_core/object/object_call_internal.h:69`。
 * 当前唯一直接调用是 `tests/core/test_object_call_known_native_fast_path.c:6371`；该用例验证专用 GET fast callback 的 blocking-detached 模式、结果 72 和 mutator 计数恢复。
 * 生产对象层在 `object.c:1172` 执行缓存 GET callback，索引操作通过 `object.c:3677` 与 `:3710` 到达该包装函数；
 * 这不是本 helper 的直接调用。专用 GET callback 字段由 `native_binding_internal.h:356` 缓存。
 */

/**
 * @fn TZrBool ZrCore_Object_TryCallIndexContractDirectBindingReadonlyInlineTwoArgumentsNoResultStack(SZrState *state, const SZrObjectKnownNativeDirectDispatch *directBindingDispatch, const SZrTypeValue *receiver, const SZrTypeValue *argument0, const SZrTypeValue *argument1)
 * ## Summary
 * 尝试按只读 inline SET 契约同步调用无结果的索引写入 callback；形状不匹配时返回假供调用者走普通分派。
 * 专用 fast callback 存在时直接接收两个 VM 栈参数；否则连续参数使用 value context，非连续参数使用局部指针数组构造 pointer-value context。
 *
 * ## Parameters
 * @param[in,out] state 正在执行的有效 VM state；debug hook 未挂起，且 GC 域允许进入 descriptor 指定模式；NativeEnter/Leave 会更新其 GC 域状态。
 * @param[in] directBindingDispatch 缓存的 descriptor、generic callback、可选专用 SET callback 及契约位；必须非空且 generic callback 非空。
 * @param[in] receiver VM 栈中的接收者值；作为 self 借给 callback，不转移栈槽所有权；setter 可按索引契约修改其指向对象的内容。
 * @param[in] argument0 VM 栈中的第一个显式参数（通常为索引键）；按 inline-value descriptor 契约借给 callback。
 * @param[in] argument1 VM 栈中的第二个显式参数（通常为待写入值）；按 inline-value descriptor 契约借给 callback。
 *
 * ## Returns
 * @return 前置形状不满足或 NativeEnter 失败时返回假且不调用 callback；callback 返回后若 thread status 为 FINE 则返回归一化结果。
 * callback 返回假但 thread status 仍为 FINE 时按无结果 setter 的契约视为成功；thread status 非 FINE 时返回假。
 * 本接口不提供 result 槽，也不通过返回的值报告普通索引未命中。
 *
 * ## Constraints and lifecycle
 * `rawArgumentCount` 必须为 3（含 receiver），`usesReceiver` 必须为真；`reserved0` 必须包含 NO_RESULT 组合的所有位，允许额外位。
 * receiver、argument0、argument1 必须位于同一 state 的有效 VM 栈范围内。函数按地址相邻判断两个显式参数是否连续；
 * 连续时上下文引用原栈片段，非连续时局部 pointer 数组及其栈值地址只在 callback 同步期间有效。
 * callback 完成后正常路径调用 NativeLeave；非局部 VM 异常若处于外围 TryRun 边界内则由该边界恢复，不经过普通 callback-false 归一化。
 * 本函数不为 callback 建立额外互斥区或跨线程生命周期；NativeEnter/Leave 内部的域协调不使 state 可并发使用，调用方须遵循其 VM 线程与 safepoint 约定。
 *
 * ## References
 * 内部声明见 `zr_vm_core/src/zr_vm_core/object/object_call_internal.h:114`。
 * 本工作树没有发现对该导出 helper 的直接调用或专属运行用例；descriptor 专用 SET callback 字段写入见 `native_binding_internal.h:358`，
 * 生产对象层在 `object.c:1215` 执行缓存 SET callback，索引操作通过 `object.c:3911` 与 `:3944` 到达该包装函数；
 * 这不是本 helper 的直接调用。测试目标定义见 `tests/CMakeLists.txt:441`，Unity helper 的 executable 创建见 `:67`；
 * suite 列表见 `tests/cmake/manifest-suite-registration.cmake:4`。
 */
