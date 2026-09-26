#ifndef ZR_VM_LIBRARY_AOT_RUNTIME_CLEANUP_REGISTRATION_H
#define ZR_VM_LIBRARY_AOT_RUNTIME_CLEANUP_REGISTRATION_H

#include "zr_vm_library/aot_runtime.h"

/** @brief 为生成帧的同一逻辑槽选择待关闭注册位置。
 * @note 生成帧可能同时有稠密值槽和物理 VALUE 槽；后者持有隐藏 owner 时必须注册物理槽，
 * 使成员结果覆盖稠密槽后作用域清理仍能找到 owner。返回的栈位置只在当前帧有效。
 */
TZrBool aot_runtime_cleanup_registration_prepare(
        SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 logicalSlot,
        TZrStackValuePointer *outRegistration);

/** @brief 稠密槽转移或被替换后清理活动物理注册槽，避免离域时重复释放旧 owner。 */
void aot_runtime_cleanup_registration_clear(
        SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 logicalSlot);

/** @brief 稠密槽改变后更新活动注册槽，使稍后的 CloseScope 清理当前 owner。 */
void aot_runtime_cleanup_registration_refresh(
        SZrState *state,
        const ZrAotGeneratedFrame *frame,
        TZrUInt32 logicalSlot);

#endif
