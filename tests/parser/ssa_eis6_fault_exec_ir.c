#include "ssa_eis6_fault_allocator.h"

#include <stdlib.h>

/* 静态 EIS6 解码测试重编译 Core 图与动态池的生命周期，使解码失败后的整图清理也进入共享跟踪器。
 * 仅该翻译单元被拦截；libc 声明先于宏，三模块共享 ZrEis6Test 状态。
 * 失败序号从 0 开始；测试用 ArmFailure(SIZE_MAX)/DisableFailure 关闭所选故障。 */
#define malloc ZrEis6Test_Malloc
#define calloc ZrEis6Test_Calloc
#define realloc ZrEis6Test_Realloc
#define free ZrEis6Test_Free
#include "../../zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c"
#undef free
#undef realloc
#undef calloc
#undef malloc
