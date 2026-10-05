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

/** @brief 原生绑定的 receiver 来源；NONE 不传接收者，FRAME 从调用帧取，CAPTURED 从闭包捕获取。
 * @note 捕获模式的 owner 与值由原生闭包的两个尾部数组关联。 */
#define ZR_NATIVE_BINDING_RECEIVER_NONE 0u
#define ZR_NATIVE_BINDING_RECEIVER_FRAME 1u
#define ZR_NATIVE_BINDING_RECEIVER_CAPTURED 2u

/** @brief 捕获单元的互斥存储：开放时挂在线程链上，关闭后保存离栈的值。
 * @note 由 value 是否指向 closedValue 判别，摘链必须先于覆盖 link。 */
union TZrClosureLink {
    struct {
        struct SZrClosureValue *next; /**< 下一较低栈槽的开放单元，关闭后不再读取。 */
        struct SZrClosureValue **previous; /**< 指向前驱的 next 或线程链头，供摘链原地更新。 */
    };
    // if value is not on stack, value is closed
    SZrTypeValue closedValue; /**< 摘链后接收捕获值；与开放链指针互斥。 */
};

/** @brief 共享同一栈槽的捕获单元；开放链按栈地址降序，便于退栈时从高处关闭。
 * @note 关闭后 value 指回本对象的 closedValue，GC 按关闭状态扫描或重写内部值。 */
struct ZR_STRUCT_ALIGN SZrClosureValue {
    SZrRawObject super; /**< GC 对象头，开放链与闭包捕获数组提供可达关系。 */
    TZrStackPointer value; /**< 开放时指向活栈槽，关闭时指向本对象的 closedValue。 */
    union TZrClosureLink link; /**< 开放链与关闭值的互斥存储，由 value 的自指状态选择。 */
    TZrUInt32 captureScopeDepth; /**< 共享捕获合并后的最外层有效深度，NONE 表示尚无作用域信息。 */
    TZrUInt32 captureEscapeFlags; /**< 来自函数捕获记录的逃逸标志，按位累积。 */
    TZrUInt32 anchoredEscapeFlags; /**< 闭包实际逃逸后记录的标志，关闭时补传给离栈值。 */
    TZrUInt32 anchoredPromotionReason; /**< 逃逸传播的提升原因；已有明确原因优先于 NONE 或 SURVIVAL。 */
};

typedef struct SZrClosureValue SZrClosureValue;


/** @brief 原生 callable 与 AOT shim 共用的闭包对象。
 * @note 捕获指针数组后紧跟等长 owner 数组；owner 为捕获单元时，以单元当前值为准。
 * 原生绑定描述符供 library 派发使用，GC 另行扫描 shim 与捕获 owner。 */
struct ZR_STRUCT_ALIGN SZrClosureNative {
    SZrRawObject super; /**< 原生闭包对象头，isNative 区分 VM 分支。 */
    // SZrRawObject *gcList;
    FZrNativeFunction nativeFunction; /**< 原生派发入口；构造时为空，由注册者或 AOT 绑定安装。 */
    struct SZrFunction *aotShimFunction; /**< AOT callable 的函数元数据，普通原生闭包可以为空。 */
    TZrSize nativeBindingLookupIndex; /**< library 绑定表定位索引，未绑定时为 ZR_MAX_SIZE。 */
    TZrUInt64 callBindingGeneration; /**< typed call 缓存验证使用的非零代次，绑定刷新时推进。 */
    TZrPtr nativeBindingDescriptor; /**< 按 nativeBindingKind 解释的 library 函数或方法描述符。 */
    TZrPtr nativeBindingModuleDescriptor; /**< 绑定所属模块描述符，供 library 重建解析条目。 */
    TZrPtr nativeBindingTypeDescriptor; /**< 方法绑定所属类型描述符，供 library 重建解析条目。 */
    TZrPtr nativeBindingOwnerPrototype; /**< 绑定所属原型的定位信息，供 library 派发重建条目。 */
    TZrUInt32 nativeBindingKind; /**< 决定描述符种类及 receiver 处理方式的 library 绑定类别。 */
    TZrUInt32 nativeBindingUsesReceiver; /**< 由 receiver 来源常量编码；不可将 CAPTURED 当成独立 owner。 */
    SZrObjectKnownNativeDirectDispatch nativeBindingDirectDispatch; /**< known native 快速派发的绑定记录，构造时清零。 */
    TZrSize closureValueCount; /**< 捕获指针与 owner 两个尾部数组共有的有效长度。 */
    SZrTypeValue *closureValuesExtend[1]; /**< 捕获值地址尾数组；owner 为 upvalue 时应使用 GetCaptureValue 重取当前值。 */
};

typedef struct SZrClosureNative SZrClosureNative;

/** @brief VM callable 将函数元数据与共享捕获单元关联。
 * @note function 与捕获数组均形成 GC 扫描边；捕获单元可在父帧退出后继续保存值。 */
struct ZR_STRUCT_ALIGN SZrClosure {
    SZrRawObject super; /**< VM 闭包对象头，与原生分支共用 raw object 类型。 */
    // SZrRawObject *gcList;
    // todo: closure info
    SZrFunction *function; /**< VM 执行和捕获元数据来源，由 GC 扫描保持引用。 */
    TZrSize closureValueCount; /**< 尾部共享捕获单元指针的有效长度。 */
    SZrClosureValue *closureValuesExtend[1]; /**< 按子函数捕获记录顺序保存共享单元，写入时需建立 GC 边。 */
};

typedef struct SZrClosure SZrClosure;

/** @brief 两种闭包共用原始对象头；调用派发按 isNative 选择有效分支。 */
union TZrClosure {
    SZrClosureNative nativeClosure; /**< isNative 为真时供原生派发读取的分支。 */
    SZrClosure zrClosure; /**< isNative 为假时供 VM 派发读取的分支。 */
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

/** @brief 为已存在的 local 登记更高的关闭代理，使内层 using 无需复制资源。
 * @pre 两槽属于当前线程的活栈；proxySlot 为空且高于 sourceSlot 与当前关闭链头。
 * @note sourceSlot 是 dense 逻辑槽；AOT 可登记独立物理代理，关闭时再定位 source 的镜像。
 * 代理槽保留到摘链；资源清理前清空 source，借用值只清视图，无清理的普通值保持可读。
 * @return 槽约束不满足或 token 构造返回空时为 false，关闭链不新增代理。
 * @note 内存耗尽可由分配层抛出 MEMORY_ERROR，不能只按布尔返回处理。 */
ZR_CORE_API TZrBool ZrCore_Closure_MarkCloseProxy(struct SZrState *state,
                                                 TZrStackValuePointer proxySlot,
                                                 TZrStackValuePointer sourceSlot);

/** @brief 从开放捕获链表摘除单元；调用方随后必须完成值的关闭转移。 */
ZR_CORE_API void ZrCore_Closure_UnlinkValue(SZrClosureValue *closureValue);

/** @brief 将阈值以上且仍位于 stackTop 内的开放栈槽值复制到捕获单元并摘链。 */
ZR_CORE_API void ZrCore_Closure_CloseStackValue(struct SZrState *state, TZrStackValuePointer stackPointer);

/** @brief 关闭开放捕获并按逆序调用已登记值的清理逻辑。
 * @return 栈可能增长后的阈值地址；调用方继续访问栈时应使用返回值。 */
ZR_CORE_API TZrStackValuePointer ZrCore_Closure_CloseClosure(struct SZrState *state, TZrStackValuePointer stackPointer,
                                                       EZrThreadStatus errorStatus, TZrBool isYield);

/** @brief 从待关闭链头处理至多 count 个登记值，供按清理登记数退出作用域。
 * @note 此接口不自行关闭开放捕获；AOT 清理 helper 在每次调用前关闭相应捕获。
 * 每项先摘链再执行清理，避免同一项在回调期间再次被链头选中。
 * @return 实际处理数，可小于 count；清理调用可能让出执行权或抛出状态。 */
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
