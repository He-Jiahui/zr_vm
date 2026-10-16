#ifndef ZR_TEST_SSA_OWNER_FAULT_ALLOCATOR_H
#define ZR_TEST_SSA_OWNER_FAULT_ALLOCATOR_H

#include <stddef.h>

void ssa_owner_fail_allocation(size_t ordinal);
int ssa_owner_allocation_failed(void);
size_t ssa_owner_outstanding_allocations(void);

#endif

/**
 * @fn void ssa_owner_fail_allocation(size_t ordinal)
 *
 * ## Summary
 * 配置后清allocationOrdinal/failed但不清outstanding，不分配或释放。
 *
 * ## Parameters
 * @param[in] ordinal 0禁用注入；非零设置本TU下一轮要拒绝的hook尝试序号。
 *
 * ## Returns
 * 无C返回值；设置本TU控制状态。
 *
 * ## Constraints and lifecycle
 * 接口的实际实现来自单独编译的ssa_owner_fault_allocator.c；此header本身不替换malloc/calloc/free，也不文本包含生产OwnerC。
 * 同一可执行程序共享TU-static配置和计数，必须串行使用；不同测试程序各有实例。计数只跟踪定义之后文本include的owner分析TU，其他linkedTU不在范围内。
 * 已有分配须按hook所有权正常释放；配置重置不会释放资源或把outstanding归零。ownership OOM case在调用返回后先禁用注入再断言。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.h:6
 * - tests/parser/ssa_owner_fault_allocator.c:10
 * - tests/parser/ssa_owner_fault_allocator.c:16
 * - tests/parser/ssa_owner_fault_allocator.c:17
 * - tests/parser/ssa_owner_fault_allocator.c:19
 * - tests/parser/ssa_owner_fault_allocator.c:28
 * - tests/parser/ssa_owner_fault_allocator.c:36
 * - tests/parser/ssa_owner_fault_allocator.c:44
 * - tests/parser/ssa_owner_fault_allocator.c:51
 * - tests/parser/ssa_owner_fault_allocator.c:54
 * - tests/parser/test_ssa_state_map_ownership.c:322
 * - tests/parser/test_ssa_state_map_ownership.c:325
 * - tests/parser/test_ssa_state_map_ownership.c:358
 * - tests/parser/test_ssa_state_map_ownership.c:361
 * - tests/parser/test_ssa_conditional_cleanup.c:661
 * - tests/parser/test_ssa_conditional_cleanup.c:664
 */

/**
 * @fn int ssa_owner_allocation_failed(void)
 *
 * ## Summary
 * 读取sticky注入命中标记，不检测自然NULL或其他TU失败。
 *
 * ## Returns
 * @return 本轮命中注入返回1，否则0；不清标记。
 *
 * ## Constraints and lifecycle
 * 接口的实际实现来自单独编译的ssa_owner_fault_allocator.c；此header本身不替换malloc/calloc/free，也不文本包含生产OwnerC。
 * 同一可执行程序共享TU-static配置和计数，必须串行使用；不同测试程序各有实例。计数只跟踪定义之后文本include的owner分析TU，其他linkedTU不在范围内。
 * 已有分配须按hook所有权正常释放；配置重置不会释放资源或把outstanding归零。ownership OOM case在调用返回后先禁用注入再断言。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.h:7
 * - tests/parser/ssa_owner_fault_allocator.c:10
 * - tests/parser/ssa_owner_fault_allocator.c:16
 * - tests/parser/ssa_owner_fault_allocator.c:17
 * - tests/parser/ssa_owner_fault_allocator.c:19
 * - tests/parser/ssa_owner_fault_allocator.c:28
 * - tests/parser/ssa_owner_fault_allocator.c:36
 * - tests/parser/ssa_owner_fault_allocator.c:44
 * - tests/parser/ssa_owner_fault_allocator.c:51
 * - tests/parser/ssa_owner_fault_allocator.c:54
 * - tests/parser/test_ssa_state_map_ownership.c:324
 * - tests/parser/test_ssa_state_map_ownership.c:360
 * - tests/parser/test_ssa_conditional_cleanup.c:663
 */

/**
 * @fn size_t ssa_owner_outstanding_allocations(void)
 *
 * ## Summary
 * 读取hook成功分配减匹配free的计数，control不清此数；不是全进程泄漏检测。
 *
 * ## Returns
 * @return 返回本TU当前outstanding数量，不修改状态或转移内存。
 *
 * ## Constraints and lifecycle
 * 接口的实际实现来自单独编译的ssa_owner_fault_allocator.c；此header本身不替换malloc/calloc/free，也不文本包含生产OwnerC。
 * 同一可执行程序共享TU-static配置和计数，必须串行使用；不同测试程序各有实例。计数只跟踪定义之后文本include的owner分析TU，其他linkedTU不在范围内。
 * 已有分配须按hook所有权正常释放；配置重置不会释放资源或把outstanding归零。ownership OOM case在调用返回后先禁用注入再断言。
 *
 * ## References
 * - tests/parser/ssa_owner_fault_allocator.h:8
 * - tests/parser/ssa_owner_fault_allocator.c:10
 * - tests/parser/ssa_owner_fault_allocator.c:16
 * - tests/parser/ssa_owner_fault_allocator.c:17
 * - tests/parser/ssa_owner_fault_allocator.c:19
 * - tests/parser/ssa_owner_fault_allocator.c:28
 * - tests/parser/ssa_owner_fault_allocator.c:36
 * - tests/parser/ssa_owner_fault_allocator.c:44
 * - tests/parser/ssa_owner_fault_allocator.c:51
 * - tests/parser/ssa_owner_fault_allocator.c:54
 * - tests/parser/test_ssa_state_map_ownership.c:326
 * - tests/parser/test_ssa_state_map_ownership.c:362
 * - tests/parser/test_ssa_conditional_cleanup.c:665
 */
