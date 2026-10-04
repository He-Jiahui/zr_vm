#ifndef ZR_TEST_SSA_ORACLE_RESUME_FAULT_ALLOCATOR_H
#define ZR_TEST_SSA_ORACLE_RESUME_FAULT_ALLOCATOR_H

#include <stddef.h>

/**
 * @brief 开始串行 oracle 消费模块的故障场景。
 * @note ordinal 在 malloc/calloc 共享序列中从 1 开始选择一次失败；0 关闭。调用重置计数和命中标志。
 */
void ssa_oracle_resume_fail_allocation(size_t ordinal);
/**
 * @brief 确认恢复操作是否触发了所选合成分配失败。
 * @note 操作返回后、reset 前读取；自然 OOM 不置位，后续分配也不因此持续失败。
 */
int ssa_oracle_resume_allocation_failed(void);

#endif
