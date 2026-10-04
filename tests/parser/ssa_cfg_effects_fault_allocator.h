#ifndef ZR_TEST_SSA_CFG_EFFECTS_FAULT_ALLOCATOR_H
#define ZR_TEST_SSA_CFG_EFFECTS_FAULT_ALLOCATOR_H

#include <stddef.h>
#include "zr_vm_common.h"

/**
 * @brief 开始串行 Core 池增长故障场景。
 * @note ordinal 从 1 开始选择一次 realloc 失败；0 关闭注入。调用清空尝试计数和命中标志。
 */
void ssa_cfg_effects_fault_fail_reallocation(size_t ordinal);
/**
 * @brief 取得本场景的 realloc 尝试数以确认故障位置。
 * @note 包含返回 NULL 的尝试，不是成功分配数；在下一次 arm/reset 之前读取。
 */
size_t ssa_cfg_effects_fault_reallocation_attempts(void);
/**
 * @brief 检查本场景选中的合成 realloc 故障。
 * @note 命中保持到下一次 arm/reset；自然 OOM 不置位，读取不会修改计数或标志。
 */
TZrBool ssa_cfg_effects_fault_reallocation_failed(void);

#endif
