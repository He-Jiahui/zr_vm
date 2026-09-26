//
// zr.system.gc native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_GC_H
#define ZR_VM_LIB_SYSTEM_GC_H

#include "zr_vm_lib_system/conf.h"

/** @brief 启用当前全局状态的分代增量回收；作用于共享 GC，而非单一脚本对象。 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Enable(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 暂停自动调度与增量步骤；显式完整回收仍可由 collect 请求。 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Disable(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 请求 minor、major 或 full 回收；省略参数使用 full。
 *  @note minor/major 只推进一次调度步骤，不保证返回时整个回收周期已完成。 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_Collect(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 设置当前全局堆限制，零值用于清除限制；拒绝负数。 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_SetHeapLimit(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 将同一微秒预算应用于 GC 暂停和 remark 切片；拒绝负数。 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_SetBudget(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 返回 SystemGcStats 结构化快照，不暴露可变的 collector 内部指针。 */
ZR_VM_LIB_SYSTEM_API TZrBool ZrSystem_Gc_GetStats(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_GC_H
