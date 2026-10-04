#ifndef ZR_TEST_SSA_DEOPT_AGGREGATE_FAULT_ALLOCATOR_H
#define ZR_TEST_SSA_DEOPT_AGGREGATE_FAULT_ALLOCATOR_H

#include <stddef.h>

/**
 * @brief 开始串行 aggregate 准备故障场景。
 * @note ordinal 从 1 开始选择一次 malloc 失败；0 关闭注入。调用同时重置计数和命中标志。
 */
void ssa_deopt_aggregate_fail_allocation(size_t ordinal);
/**
 * @brief 检查所选合成故障是否命中。
 * @note 返回值不表示所有 malloc 的结果；自然 OOM 不置位，读取不清空，reset 前消费。
 */
int ssa_deopt_aggregate_allocation_failed(void);

#endif
