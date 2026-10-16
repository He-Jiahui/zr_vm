#ifndef ZR_VM_CORE_CLOSURE_CLOSE_META_GUARD_H
#define ZR_VM_CORE_CLOSURE_CLOSE_META_GUARD_H

#include "zr_vm_core/state.h"

/** @brief 在外层异常展开中调用 @close，并隔离 callback 的异常状态。
 * @pre state 有活动外帧，且无 pending control；boundarySlotOffset 指向预留边界槽，
 * 后面依次是 callable、receiver、Error，stackTop 已覆盖参数。
 * @note callback 按零结果调用；正常完成恢复原异常，新异常或执行终止保留 callback 状态。
 * callback 必须回到原生边界；残留帧、handler 或 root frame 的不合法状态会报告运行错误。 */
void ZrCore_ClosureCloseMetaGuard_Invoke(SZrState *state,
                                         TZrMemoryOffset boundarySlotOffset,
                                         TZrBool isYield);

#endif
