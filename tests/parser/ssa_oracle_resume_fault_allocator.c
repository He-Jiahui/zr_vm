#include "ssa_oracle_resume_fault_allocator.h"

#include <stdlib.h>

/* malloc/calloc 共享一个场景序列，须由测试串行设置并在操作返回后读取命中标志。
 * 第 1 次尝试起编号，0 关闭注入；命中只影响所选一次，不使后续分配持续失败。 */
static size_t failureOrdinal;
static size_t allocationOrdinal;
static int failed;

void ssa_oracle_resume_fail_allocation(size_t ordinal) {
    failureOrdinal = ordinal;
    allocationOrdinal = 0u;
    failed = 0;
}

/* 只报告该场景选中的合成失败，读取不清空；操作后须先读取，再 reset/禁用。 */
int ssa_oracle_resume_allocation_failed(void) { return failed; }

static int allocation_should_fail(void) {
    ++allocationOrdinal;
    if (failureOrdinal != 0u && allocationOrdinal == failureOrdinal) {
        failed = 1;
        return 1;
    }
    return 0;
}

static void *resume_malloc(size_t size) {
    if (allocation_should_fail()) return NULL;
    return malloc(size);
}

static void *resume_calloc(size_t count, size_t size) {
    if (allocation_should_fail()) return NULL;
    return calloc(count, size);
}

/* Compile the production consumer with local allocation interception.
 * OracleResultFree belongs to another translation unit, so ownership and
 * deallocation retain the ordinary allocator throughout this executable. */
/* oracle-resume 及继承其源列表的 conditional-cleanup 测试共享这份消费模块。
 * 只拦本 TU 的 malloc/calloc（也包含异常边 owner 临时缓冲），不是整个执行器。
 * 故障后其余尝试仍可成功，普通 ResultFree 与本 TU 的 free 保持 libc 配对。 */
#define malloc resume_malloc
#define calloc resume_calloc
#include "../../zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c"
