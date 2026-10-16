#ifndef ZR_VM_CORE_OWNERSHIP_SHARED_INTERNAL_H
#define ZR_VM_CORE_OWNERSHIP_SHARED_INTERNAL_H

#include "zr_vm_core/ownership.h"

struct SZrRawObject;
struct SZrState;

/**
 * @brief 给普通对象或首次共享的资源取得稳定控制块，供上层 owner 转换复用。
 * @return 已有控制块且域不符、参数无效或分配失败时返回空；新块绑定当前 state。
 * BUG: 新块路径未检查 object 的真实 GC 域，调用方可把外域对象登记进错误 collector。
 */
SZrOwnershipControl *ZrCore_OwnershipShared_GetOrCreateControl(
        struct SZrState *state,
        struct SZrRawObject *object);

/** @brief 验证控制块仍属于发起操作的 state 域；跨域 owner 操作须先拒绝。 */
TZrBool ZrCore_OwnershipShared_IsInIsolationDomain(
        const struct SZrState *state,
        const SZrOwnershipControl *control);

/** @brief 为 shared 副本或 weak 唤醒增加强引用；目标必须存活且未开始 drop。 */
TZrBool ZrCore_OwnershipShared_RetainStrong(
        struct SZrState *state,
        SZrOwnershipControl *control);

/**
 * @brief 为首次 unique owner 建立一份强引用；上层负责协调 GC 保活根。
 * @pre 控制块尚无强引用；已有 owner 时重设计数会破坏其生命周期。
 */
TZrBool ZrCore_OwnershipShared_SetInitialStrong(
        struct SZrState *state,
        SZrOwnershipControl *control,
        TZrUInt32 *outPreviousCount);

/**
 * @brief 消耗一份强引用；最后一份先阻止 weak 唤醒，再交由调用方完成 drop。
 * @pre 可能释放最后一份 strong 时 outFinalObject 必须非空。
 * @note 返回最终释放时，调用方必须处理 outFinalObject 并调用 FinishFinalStrong。
 */
TZrBool ZrCore_OwnershipShared_ReleaseStrong(
        struct SZrState *state,
        SZrOwnershipControl *control,
        struct SZrRawObject **outFinalObject);

/** @brief 在最终对象清理后解除反向关联和隐含 weak；不得在 drop 尚未结束时调用。 */
void ZrCore_OwnershipShared_FinishFinalStrong(
        struct SZrState *state,
        SZrOwnershipControl *control,
        struct SZrRawObject *object);

/** @brief 为 shared 派生或复制 weak 句柄；只延长控制块寿命，不保活对象。 */
TZrBool ZrCore_OwnershipShared_RetainWeak(
        struct SZrState *state,
        SZrOwnershipControl *control);

/** @brief 释放本域 weak 句柄；最后一份 weak 可使已失效控制块被回收。 */
void ZrCore_OwnershipShared_ReleaseWeak(
        struct SZrState *state,
        SZrOwnershipControl *control);

/** @brief GC 回收对象时使其 weak 身份失效，并保留仍被 weak 句柄引用的控制块。 */
void ZrCore_OwnershipShared_InvalidateObject(
        struct SZrState *state,
        SZrOwnershipControl *control,
        struct SZrRawObject *object);

#endif
