#ifndef ZR_TEST_EXEC_IR_OWNER_SHIM_H
#define ZR_TEST_EXEC_IR_OWNER_SHIM_H
#include <stdlib.h>
#include "ssa_exec_ir_owned_module_allocator.h"
#ifndef ZR_TEST_EXEC_IR_OWNER_NO_REDIRECT
#define malloc ZrTests_ExecOwner_Malloc
#define calloc ZrTests_ExecOwner_Calloc
#define realloc ZrTests_ExecOwner_Realloc
#define free ZrTests_ExecOwner_Free
#endif
#endif
