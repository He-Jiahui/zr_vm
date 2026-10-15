#ifndef ZR_TEST_SSA_STATE_MAP_CLONE_FAULT_ALLOCATOR_H
#define ZR_TEST_SSA_STATE_MAP_CLONE_FAULT_ALLOCATOR_H

#include <stddef.h>

void ssa_state_map_clone_fail_allocation(size_t ordinal);
int ssa_state_map_clone_allocation_failed(void);
size_t ssa_state_map_clone_allocation_attempts(void);
size_t ssa_state_map_clone_outstanding_allocations(void);

#endif

/**
 * @fn void ssa_state_map_clone_fail_allocation(size_t ordinal)
 *
 * ## Summary
 * 为 clone allocator wrapper 设置下一次按序拦截的分配失败点，或使用零 ordinal 关闭本轮注入。
 *
 * ## Parameters
 * @param[in] ordinal 从调用后首次被 wrapper 截获的 malloc 计为 1；零表示不注入。调用会重置尝试计数与失败标志。
 *
 * ## Returns
 * 无 C 返回值；注入状态通过同组查询接口读取。
 *
 * ## Constraints and lifecycle
 * 该声明由测试 allocator TU 的函数支持；ordinal 只计入该 TU 以宏替换 malloc/free 后包含的 state-map storage 实现，不包括测试 fixture 的 calloc/realloc 或其他 translation unit。状态为 TU-local 静态变量，无同步；测试须串行设置、执行并复位，且不得把查询状态解释为全进程分配统计。
 *
 * ## References
 * allocator 状态重置在 ssa_state_map_clone_fault_allocator.c 15–19 行；替换 allocator 只作用于被包含的 storage 实现，见 62–66 行。
 */

/**
 * @fn int ssa_state_map_clone_allocation_failed(void)
 *
 * ## Summary
 * 查询当前注入周期内是否有某次 wrapper malloc 命中预定 ordinal 并被拒绝。
 *
 * ## Returns
 * @return 返回该周期的失败标志；若 ordinal 尚未命中或随后重新设置/关闭注入则为零。
 *
 * ## Constraints and lifecycle
 * 只反映测试 allocator TU 的静态失败位，不覆盖自然 malloc 返回 NULL 或其他 translation unit 的分配；读取与设置须按 Unity case 顺序串行进行。
 *
 * ## References
 * 标志由 allocator C 的 11 行保存、21 行查询，并在 30–32 行命中时置位。
 */

/**
 * @fn size_t ssa_state_map_clone_allocation_attempts(void)
 *
 * ## Summary
 * 查询最近一次设置注入点之后，storage wrapper 已收到的 malloc 调用次数，用于将 ordinal 与 Clone 的池分配顺序对应。
 *
 * ## Returns
 * @return 返回当前计数；每次 ssa_state_map_clone_fail_allocation 调用都把计数重置为零。
 *
 * ## Constraints and lifecycle
 * 该数值仅属于 allocator TU，包含所拦截的 Clone storage malloc 尝试；不等同于进程总分配数或自然成功分配数。静态状态无同步，测试必须串行使用。
 *
 * ## References
 * 计数在 allocator C 10 行存放、29 行递增、22 行读取；重置入口位于 15–18 行。
 */

/**
 * @fn size_t ssa_state_map_clone_outstanding_allocations(void)
 *
 * ## Summary
 * 查询 allocator wrapper 当前仍登记为存活的 clone 池分配数，供故障清理与正常释放后检查资源归零。
 *
 * ## Returns
 * @return 返回 allocator TU 指针 ledger 中的 outstanding 数量。
 *
 * ## Constraints and lifecycle
 * ledger 仅登记被替换 allocator 成功分配且仍未被对应 wrapper free 的指针；测试 fixture 的常规分配不计入。数组容量是固定的 12 个槽，状态为静态且无同步，因此测试应串行且不能据此声称覆盖别的分配器或线程。
 *
 * ## References
 * ledger 与计数在 allocator C 12–13 行声明，在 36–40、52–56 行更新，查询定义位于 23 行。
 */
