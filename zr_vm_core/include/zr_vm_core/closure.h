//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_CLOSURE_H
#define ZR_VM_CORE_CLOSURE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/object_known_native_dispatch.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/stack.h"

struct SZrState;
struct SZrClosureValue;
struct SZrCallInfo;

/* 原生绑定的 receiver 来源；捕获模式由闭包自身的捕获 owner 保持生命周期。 */
#define ZR_NATIVE_BINDING_RECEIVER_NONE 0u
#define ZR_NATIVE_BINDING_RECEIVER_FRAME 1u
#define ZR_NATIVE_BINDING_RECEIVER_CAPTURED 2u

/* 开放时保存按栈槽降序排列的双向链；关闭后同一空间保存独立值。 */
union TZrClosureLink {
    struct {
        struct SZrClosureValue *next;
        struct SZrClosureValue **previous;
    };
    // if value is not on stack, value is closed
    SZrTypeValue closedValue;
};

/* 栈槽捕获由线程开放链表锚定；关闭后 value 指回本对象的 closedValue。 */
struct ZR_STRUCT_ALIGN SZrClosureValue {
    SZrRawObject super;
    TZrStackPointer value;
    union TZrClosureLink link;
    TZrUInt32 captureScopeDepth;
    TZrUInt32 captureEscapeFlags;
    TZrUInt32 anchoredEscapeFlags;
    TZrUInt32 anchoredPromotionReason;
};

typedef struct SZrClosureValue SZrClosureValue;


/* 捕获指针数组后紧跟等长 owner 数组；owner 指向捕获单元时读取当前槽值。 */
struct ZR_STRUCT_ALIGN SZrClosureNative {
    SZrRawObject super;
    // SZrRawObject *gcList;
    FZrNativeFunction nativeFunction;
    struct SZrFunction *aotShimFunction;
    TZrSize nativeBindingLookupIndex;
    TZrUInt64 callBindingGeneration;
    TZrPtr nativeBindingDescriptor;
    TZrPtr nativeBindingModuleDescriptor;
    TZrPtr nativeBindingTypeDescriptor;
    TZrPtr nativeBindingOwnerPrototype;
    TZrUInt32 nativeBindingKind;
    TZrUInt32 nativeBindingUsesReceiver;
    SZrObjectKnownNativeDirectDispatch nativeBindingDirectDispatch;
    TZrSize closureValueCount;
    SZrTypeValue *closureValuesExtend[1];
};

typedef struct SZrClosureNative SZrClosureNative;

/* VM 闭包借用函数元数据，捕获数组引用由 GC 管理的共享 upvalue 单元。 */
struct ZR_STRUCT_ALIGN SZrClosure {
    SZrRawObject super;
    // SZrRawObject *gcList;
    // todo: closure info
    SZrFunction *function;
    TZrSize closureValueCount;
    SZrClosureValue *closureValuesExtend[1];
};

typedef struct SZrClosure SZrClosure;

/* 两种闭包共用原始对象头，分支由 isNative 判定。 */
union TZrClosure {
    SZrClosureNative nativeClosure;
    SZrClosure zrClosure;
};

typedef union TZrClosure TZrClosure;

/** @brief 分配原生闭包及捕获值、owner 两个等长尾部数组。
 * @pre state 持有有效 GC 域，捕获数对应可分配的对象大小。
 * @return 新闭包由 GC 管理，调用方须在后续可触发 GC 的操作前建立根。 */
ZR_CORE_API SZrClosureNative *ZrCore_ClosureNative_New(struct SZrState *state, TZrSize closureValueCount);

/** @brief 分配 VM 闭包及捕获单元指针数组。
 * @pre state 持有有效 GC 域，捕获数对应可分配的对象大小。
 * @return 新闭包由 GC 管理，调用方须在后续可触发 GC 的操作前建立根。 */
ZR_CORE_API SZrClosure *ZrCore_Closure_New(struct SZrState *state, TZrSize closureValueCount);

/** @brief 为各捕获位创建独立且初值为空的已关闭单元。
 * @pre closure 是有效且已锚定的 VM 闭包；分配可能触发 GC。
 * @note 新增引用通过写屏障交给 GC。 */
ZR_CORE_API void ZrCore_Closure_InitValue(struct SZrState *state, SZrClosure *closure);

/** @brief 获取栈槽共享的开放捕获单元，缺失时插入线程的有序链表。
 * @pre stackPointer 是当前线程仍有效的栈槽。 */
ZR_CORE_API SZrClosureValue *ZrCore_Closure_FindOrCreateValue(struct SZrState *state, TZrStackValuePointer stackPointer);

/** @brief 查询半开栈范围内是否仍有开放捕获，供退栈与所有权检查使用。 */
ZR_CORE_API TZrBool ZrCore_Closure_HasOpenStackValueInRange(const struct SZrState *state,
                                                            TZrStackValuePointer stackStart,
                                                            TZrStackValuePointer stackEnd);

/** @brief 将具备所有权清理或 CLOSE 元方法的栈槽登记为待关闭值。 */
ZR_CORE_API void ZrCore_Closure_ToBeClosedValueClosureNew(struct SZrState *state, TZrStackValuePointer stackPointer);

/** @brief 从开放捕获链表摘除单元；调用方随后必须完成值的关闭转移。 */
ZR_CORE_API void ZrCore_Closure_UnlinkValue(SZrClosureValue *closureValue);

/** @brief 将阈值以上且仍位于 stackTop 内的开放栈槽值复制到捕获单元并摘链。 */
ZR_CORE_API void ZrCore_Closure_CloseStackValue(struct SZrState *state, TZrStackValuePointer stackPointer);

/** @brief 关闭开放捕获并按逆序调用已登记值的清理逻辑。
 * @return 栈可能增长后的阈值地址；调用方继续访问栈时应使用返回值。 */
ZR_CORE_API TZrStackValuePointer ZrCore_Closure_CloseClosure(struct SZrState *state, TZrStackValuePointer stackPointer,
                                                       EZrThreadStatus errorStatus, TZrBool isYield);

/** @brief 从待关闭链表处理至多 count 个登记值。
 * @return 实际处理数；清理调用可能让出执行权或抛出状态。 */
ZR_CORE_API TZrSize ZrCore_Closure_CloseRegisteredValues(struct SZrState *state,
                                                   TZrSize count,
                                                   EZrThreadStatus errorStatus,
                                                   TZrBool isYield);

/** @brief 在目标栈槽发布新 VM 闭包，并从父帧栈槽或父闭包连接捕获。
 * @note 分配可能触发 GC；base 与目标栈槽在实现中以锚点、偏移恢复。 */
ZR_CORE_API void ZrCore_Closure_PushToStack(struct SZrState *state, struct SZrFunction *function,
                                      SZrClosureValue **closureValueList, TZrStackValuePointer base,
                                      TZrStackValuePointer closurePointer);

/** @brief 合并同一捕获单元的最外层作用域深度与逃逸标志。 */
ZR_CORE_API void ZrCore_ClosureValue_SetCaptureMetadata(SZrClosureValue *closureValue,
                                                        TZrUInt32 scopeDepth,
                                                        TZrUInt32 escapeFlags);

/** @brief 将捕获单元标记为逃逸根，并把标记传给已关闭的值。 */
ZR_CORE_API void ZrCore_ClosureValue_AnchorEscape(struct SZrState *state,
                                                  SZrClosureValue *closureValue,
                                                  TZrUInt32 escapeFlags,
                                                  EZrGarbageCollectPromotionReason promotionReason);

/** @brief 由逃逸闭包向 VM 或原生捕获 owner 传播 GC 逃逸信息。 */
ZR_CORE_API void ZrCore_Closure_PropagateEscapeFromObject(struct SZrState *state,
                                                          SZrRawObject *closureObject,
                                                          TZrUInt32 escapeFlags,
                                                          EZrGarbageCollectPromotionReason promotionReason);

/** @brief 从函数值或闭包值取得其借用的函数元数据。
 * @return 无元数据或值类型不匹配时为空；结果归属原值或闭包。 */
ZR_CORE_API struct SZrFunction *ZrCore_Closure_GetMetadataFunctionFromValue(struct SZrState *state,
                                                                            const SZrTypeValue *value);

/** @brief 优先读取调用帧已缓存的元数据，再从被调用值解析。 */
ZR_CORE_API struct SZrFunction *ZrCore_Closure_GetMetadataFunctionFromCallInfo(struct SZrState *state,
                                                                               struct SZrCallInfo *callInfo);

/** @brief 以 value 是否指向本单元的 closedValue 判定关闭状态。 */
static ZR_FORCE_INLINE TZrBool ZrCore_ClosureValue_IsClosed(SZrClosureValue *closureValue) {
    return closureValue->value.valuePointer == ZR_CAST_STACK_VALUE(&closureValue->link.closedValue);
}

/** @brief 取得当前捕获值；开放时借用栈槽，关闭时借用单元内部值。
 * @note 返回指针不得跨关闭或栈迁移保存。 */
static ZR_FORCE_INLINE SZrTypeValue *ZrCore_ClosureValue_GetValue(SZrClosureValue *closureValue) {
    if (ZrCore_ClosureValue_IsClosed(closureValue)) {
        return &closureValue->link.closedValue;
    }
    return ZR_CAST_FROM_STACK_VALUE(closureValue->value.valuePointer);
}

/** @brief 取得尾部 owner 数组，其布局与 New 的捕获数组分配公式一致。 */
static ZR_FORCE_INLINE SZrRawObject **ZrCore_ClosureNative_GetCaptureOwners(SZrClosureNative *closure) {
    if (closure == ZR_NULL || closure->closureValueCount == 0) {
        return ZR_NULL;
    }
    return (SZrRawObject **)(closure->closureValuesExtend + closure->closureValueCount);
}

/** @brief 取得指定捕获的 owner；越界与无捕获均返回空。 */
static ZR_FORCE_INLINE SZrRawObject *ZrCore_ClosureNative_GetCaptureOwner(SZrClosureNative *closure, TZrSize closureIndex) {
    SZrRawObject **captureOwners;

    if (closure == ZR_NULL || closureIndex >= closure->closureValueCount) {
        return ZR_NULL;
    }

    captureOwners = ZrCore_ClosureNative_GetCaptureOwners(closure);
    return captureOwners != ZR_NULL ? captureOwners[closureIndex] : ZR_NULL;
}

/** @brief 优先经 owner 重新定位 upvalue，避免关闭或 GC 后使用旧地址。 */
static ZR_FORCE_INLINE SZrTypeValue *ZrCore_ClosureNative_GetCaptureValue(SZrClosureNative *closure,
                                                                          TZrSize closureIndex) {
    SZrRawObject *captureOwner;

    if (closure == ZR_NULL || closureIndex >= closure->closureValueCount) {
        return ZR_NULL;
    }

    captureOwner = ZrCore_ClosureNative_GetCaptureOwner(closure, closureIndex);
    if (captureOwner != ZR_NULL && captureOwner->type == ZR_RAW_OBJECT_TYPE_CLOSURE_VALUE) {
        return ZrCore_ClosureValue_GetValue((SZrClosureValue *)captureOwner);
    }

    return closure->closureValuesExtend[closureIndex];
}


#endif // ZR_VM_CORE_CLOSURE_H
