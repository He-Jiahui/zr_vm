#include "ssa_eis6_fault_allocator.h"

#include <stdlib.h>

/* 静态 EIS6 解码测试重编译标量读取器，使读取绑定行的临时缓冲与图分配使用同一失败序列。
 * 仅该翻译单元被拦截；libc 声明先于宏，三模块共享 ZrEis6Test 状态。
 * 失败序号从 0 开始；测试用 ArmFailure(SIZE_MAX)/DisableFailure 关闭所选故障。 */
#define malloc ZrEis6Test_Malloc
#define calloc ZrEis6Test_Calloc
#define realloc ZrEis6Test_Realloc
#define free ZrEis6Test_Free
#include "../../zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis6_read.c"
#undef free
#undef realloc
#undef calloc
#undef malloc
