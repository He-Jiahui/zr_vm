#ifndef ZR_TEST_FUNCTION_EXEC_IR_AOT_SHIM_H
#define ZR_TEST_FUNCTION_EXEC_IR_AOT_SHIM_H
#include <stdlib.h>
#include "ssa_function_exec_ir_owner_ledger.h"
#ifndef ZR_TEST_FUNCTION_EXEC_IR_AOT_NO_REDIRECT
#define malloc ZrTests_FnExec_AotMalloc
#define free ZrTests_FnExec_AotFree
#endif
#endif
