// Interpreter dispatch boundary state.
#ifndef ZR_VM_CORE_EXECUTION_CONTEXT_PRIVATE_H
#define ZR_VM_CORE_EXECUTION_CONTEXT_PRIVATE_H

/* 私有实现沿用公开执行边界 context 声明，不在此复制布局或另建 GC 根。
 * execution_safepoint/cold 消费此桥；PC/栈缓存的发布与暂停后重载由边界实现完成。
 * context 借用 state/帧，不能把缓存地址当作跨栈移动或 GC 边界持续有效的拥有指针。
 */
#include "zr_vm_core/execution_context.h"

#endif
