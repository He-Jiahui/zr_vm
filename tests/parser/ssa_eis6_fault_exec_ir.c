#include "ssa_eis6_fault_allocator.h"

#include <stdlib.h>

#define malloc ZrEis6Test_Malloc
#define calloc ZrEis6Test_Calloc
#define realloc ZrEis6Test_Realloc
#define free ZrEis6Test_Free
#include "../../zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c"
#undef free
#undef realloc
#undef calloc
#undef malloc
