/* 直接 unique/loaned 资源由槽位和 GC domain root 管理；共享资源及普通对象以
 * control block 管理 strong/weak 引用，普通对象用显式 GC ignore 根保活。 */

#ifndef ZR_VM_CORE_OWNERSHIP_H
#define ZR_VM_CORE_OWNERSHIP_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

/** @brief VM state 前置类型；ownership API 借用，生命周期由调用方管理。 */
struct SZrState;
/** @brief 原始对象前置类型；持有与转移规则由具体 API 确定。 */
struct SZrRawObject;

/** @brief 保留的弱引用前置类型；当前 weak 值使用控制块，ownershipWeakRef 字段置空。 */
struct SZrOwnershipWeakRef;
typedef struct SZrOwnershipWeakRef SZrOwnershipWeakRef;

/**
 * @brief 带 strong/weak 引用的对象控制块，绑定单个 isolation domain。
 * @note 最后一份 strong 释放时先使 object 不可唤醒，再执行 drop/归还 GC；
 *       weak 和隐含 weak 共同维持控制块寿命。ownsGcIgnore 表示控制块承担撤根责任。
 */
struct ZR_STRUCT_ALIGN SZrOwnershipControl {
    struct SZrRawObject *object;
    TZrUInt32 strongRefCount;
    TZrUInt32 weakRefCount;
    TZrUInt64 isolationDomainId;
    TZrBool objectIsAlive;
    TZrBool dropInProgress;
    TZrBool usesAtomicRefCounts;
    TZrBool ownsGcIgnore;
};

/** @brief 同一控制块的类型别名。 */
typedef struct SZrOwnershipControl SZrOwnershipControl;

/**
 * @brief 从原始对象建立 unique 值，并将原目标槽位释放/清空。
 * @note 资源对象登记 GC domain root；普通对象建立控制块并显式保活。
 */
ZR_CORE_API TZrBool ZrCore_Ownership_InitUniqueValue(struct SZrState *state,
                                                     SZrTypeValue *destination,
                                                     struct SZrRawObject *object);

/** @brief 将普通 GC 值变为 unique，或移动直接 unique 资源；普通源保持原值，自身赋值无操作。 */
ZR_CORE_API TZrBool ZrCore_Ownership_UniqueValue(struct SZrState *state,
                                                 SZrTypeValue *destination,
                                                 SZrTypeValue *source);

/** @brief 在不同槽位建立不增加强引用的 borrowed 视图；其他 owner 或 GC 根须保活对象至借用结束。 */
ZR_CORE_API TZrBool ZrCore_Ownership_BorrowValue(struct SZrState *state,
                                                 SZrTypeValue *destination,
                                                 SZrTypeValue *source);

/** @brief 将 unique owner 移入 loaned 槽位并清空源；两槽必须不同。 */
ZR_CORE_API TZrBool ZrCore_Ownership_LoanValue(struct SZrState *state,
                                               SZrTypeValue *destination,
                                               SZrTypeValue *source);

/** @brief 将 loaned 值移回 unique owner 槽位并清空源；两槽必须不同。 */
ZR_CORE_API TZrBool ZrCore_Ownership_ReturnLoanValue(struct SZrState *state,
                                                     SZrTypeValue *destination,
                                                     SZrTypeValue *source);

/** @brief 在不同槽位消耗 unique 源，产生 shared 值；已有控制块的转移不增加强引用。 */
ZR_CORE_API TZrBool ZrCore_Ownership_ShareValue(struct SZrState *state,
                                                SZrTypeValue *destination,
                                                SZrTypeValue *source);

/** @brief 在不同槽位从普通 GC 源新增一份 shared 强引用；源槽位保持原值。 */
ZR_CORE_API TZrBool ZrCore_Ownership_SharePlainValue(struct SZrState *state,
                                                     SZrTypeValue *destination,
                                                     SZrTypeValue *source);

/** @brief 在不同槽位从 shared 源增加弱引用并产生 weak 值；不消耗源。 */
ZR_CORE_API TZrBool ZrCore_Ownership_DegradeValue(struct SZrState *state,
                                               SZrTypeValue *destination,
                                               SZrTypeValue *source);

/** @brief 在不同槽位尝试将 weak 唤醒为 shared；对象失效时返回成功并写入 null。 */
ZR_CORE_API TZrBool ZrCore_Ownership_WakeValue(struct SZrState *state,
                                                  SZrTypeValue *destination,
                                                  SZrTypeValue *source);

/** @brief ReturnToGcValue 的兼容入口，供解释器和 AOT 的 detach 操作调用。 */
ZR_CORE_API TZrBool ZrCore_Ownership_DetachValue(struct SZrState *state,
                                                 SZrTypeValue *destination,
                                                 SZrTypeValue *source);

/**
 * @brief 将普通对象的最后一份强引用交还 GC，清空源并在目标写普通 GC 值。
 * @note 非最后一份 shared 强引用会被拒绝；源和目标必须是不同槽位。
 */
ZR_CORE_API TZrBool ZrCore_Ownership_ReturnToGcValue(struct SZrState *state,
                                                     SZrTypeValue *destination,
                                                     SZrTypeValue *source);

/** @brief 将本域直接 unique 资源改为 GC box，撤销 ownership root 并清空源。 */
ZR_CORE_API TZrBool ZrCore_Ownership_IntoGcBoxValue(struct SZrState *state,
                                                    SZrTypeValue *destination,
                                                    SZrTypeValue *source);

/** @brief 判断对象是否仍为存活的资源 GC box。 */
ZR_CORE_API TZrBool ZrCore_Ownership_IsGcBoxObject(const struct SZrRawObject *object);

/**
 * @brief 释放值持有的引用并清空槽位；borrowed 仅清槽，weak 减弱引用。
 * @note 最后强引用会 drop 资源或归还普通对象给 GC；资源 drop 失败可继续抛异常。
 *       shared/weak 跨 isolation domain 调用不释放，也不清空原槽位。
 */
ZR_CORE_API void ZrCore_Ownership_ReleaseValue(struct SZrState *state, SZrTypeValue *value);

/** @brief 查询控制块强引用数；无控制块时返回零，直接 unique 资源不计入。 */
ZR_CORE_API TZrUInt32 ZrCore_Ownership_GetStrongRefCount(struct SZrRawObject *object);

/**
 * @brief 值复制的慢路径：保留 shared/weak 引用，普通 struct 值按结构布局克隆。
 * @note borrowed 仍是非持有视图；unique 普通对象复制为 shared，loaned 保持 loaned；
 *       直接 unique 资源复制为同一对象的镜像槽，调用方须协调其清理次序。
 */
ZR_CORE_API void ZrCore_Ownership_AssignValue(struct SZrState *state,
                                              SZrTypeValue *destination,
                                              const SZrTypeValue *source);

/** @brief GC 释放原始对象时失效控制块，令残存 weak 不再唤醒对象。 */
ZR_CORE_API void ZrCore_Ownership_NotifyObjectReleased(struct SZrState *state,
                                                       struct SZrRawObject *object);

/** @brief native `unique` 包装：转换失败返回 null 值，正常返回一个结果槽位。 */
ZR_CORE_API TZrInt64 ZrCore_Ownership_NativeUnique(struct SZrState *state);
/** @brief native `share` 包装：消耗 unique 实参并返回 shared 或 null。 */
ZR_CORE_API TZrInt64 ZrCore_Ownership_NativeShare(struct SZrState *state);
/** @brief native `share plain` 包装：保留普通源并返回 shared 或 null。 */
ZR_CORE_API TZrInt64 ZrCore_Ownership_NativeSharePlain(struct SZrState *state);
/** @brief native `degrade` 包装：保留 shared 源并返回 weak 或 null。 */
ZR_CORE_API TZrInt64 ZrCore_Ownership_NativeDegrade(struct SZrState *state);

#endif // ZR_VM_CORE_OWNERSHIP_H
