#ifndef ZR_TEST_FUNCTION_EXEC_IR_LEDGER_H
#define ZR_TEST_FUNCTION_EXEC_IR_LEDGER_H
#include <stddef.h>
#include "zr_vm_core/global.h"
typedef struct SZrFnHeapStats { unsigned attempts,records,live,frees,errors; } SZrFnHeapStats;
TZrPtr ZrTests_FnExec_VmAllocate(TZrPtr user,TZrPtr pointer,TZrSize original,TZrSize bytes,TZrInt64 flag);
void *ZrTests_FnExec_AotMalloc(size_t bytes);
void ZrTests_FnExec_AotFree(void *pointer);
void ZrTests_FnExec_LedgerReset(void);
SZrFnHeapStats ZrTests_FnExec_VmStats(void);
SZrFnHeapStats ZrTests_FnExec_AotStats(void);
unsigned ZrTests_FnExec_VmRecord(const void *pointer);
size_t ZrTests_FnExec_VmBytes(unsigned record);
TZrInt64 ZrTests_FnExec_VmType(unsigned record);
unsigned ZrTests_FnExec_VmFrees(unsigned record);
unsigned ZrTests_FnExec_AotRecord(const void *pointer);
unsigned ZrTests_FnExec_AotFrees(unsigned record);
void ZrTests_FnExec_LedgerRecover(void);
void ZrTests_FnExec_LedgerBalanced(void);
#endif
