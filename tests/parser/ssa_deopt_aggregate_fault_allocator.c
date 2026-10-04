#include "ssa_deopt_aggregate_fault_allocator.h"

#include <stdlib.h>

/* 场景状态只供本进程内的串行测试使用；重设序号会清空尝试计数和命中标志。
 * 失败按第 1 次 malloc 起计；0 用于关闭本组注入，不沿用 EIS6 的编号约定。 */
static size_t failureOrdinal;
static size_t allocationOrdinal;
static int failed;

void ssa_deopt_aggregate_fail_allocation(size_t ordinal) {
    failureOrdinal = ordinal;
    allocationOrdinal = 0u;
    failed = 0;
}

/* 供故障用例确认选中的 malloc 确已命中；自然 OOM 不会置位，读取也不清空。 */
int ssa_deopt_aggregate_allocation_failed(void) { return failed; }

static void *aggregate_malloc(size_t size) {
    if (++allocationOrdinal == failureOrdinal) {
        failed = 1;
        return NULL;
    }
    return malloc(size);
}

/* The exact production preparation module runs with deterministic failures.
 * Storage lifecycle remains linked normally and uses the matching allocator. */
/* 重编译真实 aggregate 准备模块，而非替换其算法；仅本 TU 的 malloc 经钩子。
 * 已获得的存储仍来自 libc，普通 storage 模块继续承担失败清理和旧目标释放。 */
#define malloc aggregate_malloc
#include "../../zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c"
