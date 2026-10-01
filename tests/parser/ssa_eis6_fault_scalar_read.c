#include "ssa_eis6_fault_allocator.h"

#include <stdlib.h>

#define malloc ZrEis6Test_Malloc
#define calloc ZrEis6Test_Calloc
#define realloc ZrEis6Test_Realloc
#define free ZrEis6Test_Free
#include "../../zr_vm_core/src/zr_vm_core/artifact_exec_ir_scalar_eis6_read.c"
#undef free
#undef realloc
#undef calloc
#undef malloc
